/*
Atanua Real-Time Logic Simulator
Copyright (c) 2008-2014 Jari Komppa

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.
Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
claim that you wrote the original software. If you use this software
in a product, an acknowledgment in the product documentation would be
appreciated but is not required.

2. Altered source versions must be plainly marked as such, and must not be
misrepresented as being the original software.

3. This notice may not be removed or altered from any source
distribution.
*/
#include "atanua.h"
#include "atanua_internal.h"
#include "fileutils.h"
#include "ui_theme.h"

#include "basechipfactory.h"
#include "pluginchipfactory.h"

#include "stb/stb_image_write.h"

#define C_MENUBG 0xff20242c
#define C_MENULINE 0xff333947
#define C_WIDGETBG 0xff2c313c
#define C_WIDGETTHUMB 0xff59637a
#define C_WIDGETHOT 0xff4c8dff
#define C_TEXT 0xffeef1f6
#define C_TEXTDIM 0xff8b93a7
#define C_HOTROW 0xff31406b
#define C_ACCENTTEXT 0xff7ddf8a

#define UI_TOPBAR_H 48
#define UI_TAB_W 68
#define UI_BTN_W 68
#define UI_ROW_H 30

int gTopbarH = UI_TOPBAR_H;

#define WORLDTOSCREENX(x) ((((x)+gWorldOfsX) * gZoomFactor) + gConfig.mToolkitWidth)
#define WORLDTOSCREENY(y) ((((y)+gWorldOfsY) * gZoomFactor) + gTopbarH)

ACFont fn, fn14;

class Box;
vector<BoxLoadQueueItem *> gBoxLoadQueue;
Box *gBoxBeingLoaded = NULL;
vector<char*> gBoxNames;
int gBoxCount = 0;
int gActiveBoxes = 0;

vector<Chip*> gChip;
vector<const char*> gChipName;
vector<Wire*> gWire;
vector<Net*> gNet;
vector<ChipFactory*> gChipFactory;

vector<char *> gAvailableChip[5];

vector<File *> gUndoStack;
vector<File *> gRedoStack;

vector<BoxcacheData> gBoxCache;

vector<Chip*> gMultiSelectChip;
vector<Wire*> gMultiSelectWire;
int gMultiselectDirty = 1;

AtanuaConfig gConfig;
int gVisibleChiplist = 0;

Chip * gNewChip = NULL;
const char * gNewChipName = NULL;
Pin * gWireStartDrag = NULL;
int gKeyState[ATANUA_KEYSTATE_SIZE];
SDL_Window *gMainWindow = NULL;
void *gGLContext = NULL;

int AtanuaKeyIndex(int keysym)
{
    if (keysym >= 32 && keysym < 127)
        return keysym;
    SDL_Keycode kc = (SDL_Keycode)keysym;
    SDL_Scancode sc = SDL_GetScancodeFromKey(kc);
    if (sc != SDL_SCANCODE_UNKNOWN)
        return 128 + (sc % (ATANUA_KEYSTATE_SIZE - 128));
    return (keysym & 0xff) % ATANUA_KEYSTATE_SIZE;
}

float gWorldOfsX = 0, gWorldOfsY = 0;
float gZoomFactor = 20.0f;
int gDragMode = DRAGMODE_NONE;
int gSnap = 1;
int gLiveWires = 1;
int gBlackBackground = 1;

int gSelectKeyMask;
int gCloneKeyMask;

int gSavePNG = 0;

char * gSidebarTooltip = NULL;
int gSidebarTooltipId = -1;

SDL_AudioSpec *gAudioSpec = NULL;

SDL_Cursor *cursor_normal, *cursor_drag, *cursor_scissors;

#define AUDIOBUF_SIZE 512
unsigned char gAudioBuffer[AUDIOBUF_SIZE];
int gRecordHead = AUDIOBUF_SIZE/2;
float gPlayHead = 0;
unsigned char *gAudioOut;

void initvideo();

void handle_key(int keysym, int down)
{
    switch(keysym)
    {
    case SDLK_ESCAPE:
        if (down)
        {
            do_cancel();
        }
        break;        
    }
    gKeyState[AtanuaKeyIndex(keysym)] = down;
}

void do_rotate()
{
    if (IS_CHIP_ID(gUIState.kbditem))
    {
        int id = GET_CHIP_ID(gUIState.kbditem);
        if (id < 0 || id >= (int)gChip.size() || !gChip[id])
            return;
        save_undo();
        gChip[id]->mAngleIn90DegreeSteps++;
        gChip[id]->mAngleIn90DegreeSteps &= 0x03;
        gChip[id]->rotate(gChip[id]->mAngleIn90DegreeSteps);
    }
}

void do_screengrab()
{
	FILE * f = NULL;
	char tempname[256];
	int i = 0;
	do
	{
		if (f) fclose(f);
		i++;
		sprintf(tempname, "atanua%03d.png", i);
		f = fopen(tempname, "rb");
	}
	while(f);

	int x0 = gConfig.mToolkitWidth;
	int y0 = gTopbarH;
	int w = gScreenWidth - x0;
	int h = gScreenHeight - y0;
	
	char * data = new char[w*h*4];
	char * flipdata = new char[w*h*4];
	glReadPixels(x0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,data);
	
	for (i = 0; i < h; i++)
		memcpy(flipdata+(h-i-1)*w*4,data+i*w*4,w*4);

	stbi_write_png(tempname,w,h,4,flipdata,w*4);
	delete[] data;
	delete[] flipdata;

	char tempout[512];
	sprintf(tempout, "%s saved.", tempname);

	okcancel(tempout);
}

void process_events()
{
    SDL_Event event;

    while (SDL_PollEvent(&event)) 
    {
        switch (event.type) 
        {
        case SDL_KEYDOWN:
            handle_key(event.key.keysym.sym, 1);
            // If a key is pressed, report it to the widgets
            gUIState.keyentered = event.key.keysym.sym;
            gUIState.keymod = event.key.keysym.mod;
            if (event.key.keysym.sym == SDLK_LCTRL) gUIState.keymod |= KMOD_LCTRL;
            if (event.key.keysym.sym == SDLK_RCTRL) gUIState.keymod |= KMOD_RCTRL;
            if (event.key.keysym.sym == SDLK_LSHIFT) gUIState.keymod |= KMOD_LSHIFT;
            if (event.key.keysym.sym == SDLK_RSHIFT) gUIState.keymod |= KMOD_RSHIFT;
            if (event.key.keysym.sym == SDLK_LALT) gUIState.keymod |= KMOD_LALT;
            if (event.key.keysym.sym == SDLK_RALT) gUIState.keymod |= KMOD_RALT;

            // Alias for 'del', as accessing it may be difficult on laptops etc.
            if (event.key.keysym.sym == SDLK_d &&
                event.key.keysym.mod & KMOD_CTRL)
                gUIState.keyentered = SDLK_DELETE;


            // SDL2: no unicode field; printable ASCII comes via sym, full text via SDL_TEXTINPUT
            if (event.key.keysym.sym >= 32 && event.key.keysym.sym < 127)
                gUIState.keychar = event.key.keysym.sym;
            break;
        case SDL_TEXTINPUT:
            if (event.text.text[0] >= 32 && event.text.text[0] < 127)
                gUIState.keychar = event.text.text[0];
            break;
        case SDL_KEYUP:
            gUIState.keymod = event.key.keysym.mod;
            handle_key(event.key.keysym.sym, 0);
            if (event.key.keysym.sym == SDLK_z &&
                event.key.keysym.mod & KMOD_GUI)
                do_undo();
            if (event.key.keysym.sym == SDLK_z &&
                event.key.keysym.mod & KMOD_CTRL)
                do_undo();
            if (event.key.keysym.sym == SDLK_BACKSPACE &&
                event.key.keysym.mod & KMOD_ALT)
                do_undo();
            if (event.key.keysym.sym == SDLK_y &&
                event.key.keysym.mod & KMOD_GUI)
                do_redo();
            if (event.key.keysym.sym == SDLK_y &&
                event.key.keysym.mod & KMOD_CTRL)
                do_redo();
            if (event.key.keysym.sym == SDLK_z &&
                event.key.keysym.mod & KMOD_CTRL &&
                event.key.keysym.mod & KMOD_SHIFT)
                do_redo();
            if (event.key.keysym.sym == SDLK_z &&
                event.key.keysym.mod & KMOD_CTRL &&
                event.key.keysym.mod & KMOD_ALT)
                do_redo();
            if (event.key.keysym.sym == SDLK_BACKSPACE &&
                event.key.keysym.mod & KMOD_ALT &&
                event.key.keysym.mod & KMOD_SHIFT)
                do_redo();
            if (event.key.keysym.sym == SDLK_s &&
                event.key.keysym.mod & KMOD_CTRL)
                do_savedialog();
            if (event.key.keysym.sym == SDLK_l &&
                event.key.keysym.mod & KMOD_CTRL)
                do_loaddialog();
            if (event.key.keysym.sym == SDLK_m &&
                event.key.keysym.mod & KMOD_CTRL)
                do_loaddialog(1);
            if (event.key.keysym.sym == SDLK_b &&
                event.key.keysym.mod & KMOD_CTRL)
                do_loaddialog(2);
            if (event.key.keysym.sym == SDLK_n &&
                event.key.keysym.mod & KMOD_CTRL)
                do_resetdialog();
            if (event.key.keysym.sym == SDLK_h &&
                event.key.keysym.mod & KMOD_CTRL)
                do_home();
            if (event.key.keysym.sym == SDLK_e &&
                event.key.keysym.mod & KMOD_CTRL)
                do_zoomext();
            if (event.key.keysym.sym == SDLK_p &&
                event.key.keysym.mod & KMOD_CTRL)
                gSnap = !gSnap;
            if (event.key.keysym.sym == SDLK_w &&
                event.key.keysym.mod & KMOD_CTRL)
                gLiveWires = !gLiveWires;
            if (event.key.keysym.sym == SDLK_r &&
                event.key.keysym.mod & KMOD_CTRL)
                do_rotate();
            if (event.key.keysym.sym == SDLK_o &&
                event.key.keysym.mod & KMOD_CTRL)
			{
				save_undo();
                do_optimize_box(0);
			}
            if (event.key.keysym.sym == SDLK_g &&
                event.key.keysym.mod & KMOD_CTRL)
                do_screengrab();

            break;
        case SDL_MOUSEMOTION:
            // update mouse position
            gUIState.mousex = event.motion.x;
            gUIState.mousey = event.motion.y;
            break;
        case SDL_MOUSEBUTTONDOWN:
            // update button down state if left-clicking
            if (event.button.button == SDL_BUTTON_LEFT)
            {
                gUIState.mousedown = 1;
                gUIState.mousedownx = (float)event.button.x;
                gUIState.mousedowny = (float)event.button.y;
                gUIState.mousedownkeymod = gUIState.keymod;
            }
            if (event.button.button == SDL_BUTTON_RIGHT)
            {
                do_cancel();
            }

            break;
        case SDL_MOUSEWHEEL:
            if (event.wheel.y > 0)
                gUIState.scroll = +1;
            else if (event.wheel.y < 0)
                gUIState.scroll = -1;
            break;
        case SDL_MOUSEBUTTONUP:
            // update button down state if left-clicking
            if (event.button.button == SDL_BUTTON_LEFT)
                gUIState.mousedown = 0;
            break;
        case SDL_QUIT:
            SDL_Quit();
            exit(0);
            break;
        case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_RESIZED ||
                event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
            {
                if (event.window.data1 >= 100 && event.window.data2 >= 100)
                {
                    gScreenWidth = event.window.data1;
                    gScreenHeight = event.window.data2;
                    initvideo();
                }
            }
            break;
        }
    }
}

int getChipIdForPad(Pin *p)
{
    if (!p)
        return 0;
    int k, l;
    for (k = 0; k < (signed)gChip.size(); k++)
    {
        if (!gChip[k])
            continue;
        for (l = 0; l < (signed)gChip[k]->mPin.size(); l++)
        {
            if (gChip[k]->mPin[l] == p)
            {
                return CHIP_ID(l+1, k);
            }
        }
    }
    return 0;
}

// Nearest connectable pin within radius world units of the release point,
// ignoring boxed internals. Lets a slightly-off release still land on its
// clearly-intended pin instead of dropping a stray anchor beside it.
static Pin *find_release_pin(float worldx, float worldy, float radius)
{
    Pin *best = NULL;
    float best2 = radius * radius;
    for (size_t i = 0; i < gChip.size(); i++)
    {
        Chip *c = gChip[i];
        if (!c || c->mBox != 0)
            continue;
        for (size_t j = 0; j < c->mPin.size(); j++)
        {
            Pin *p = c->mPin[j];
            if (!p)
                continue;
            float px = c->mRotatedX + p->mRotatedX + 0.25f;
            float py = c->mRotatedY + p->mRotatedY + 0.25f;
            float dx = worldx - px;
            float dy = worldy - py;
            float d2 = dx * dx + dy * dy;
            if (d2 < best2)
            {
                best2 = d2;
                best = p;
            }
        }
    }
    return best;
}

// Existing bend-point anchor near (worldx, worldy), if any: dropping another
// one on top (double-clicks, jitter) must reuse it instead of stacking.
static Chip *find_anchor_near(float worldx, float worldy)
{
    const float r = 0.5f;
    for (size_t i = 0; i < gChip.size(); i++)
    {
        Chip *c = gChip[i];
        if (!c || c->mBox != 0)
            continue;
        if (!gChipName[i] || stricmp(gChipName[i], "Connection Pin") != 0)
            continue;
        float cx = c->mRotatedX + c->mRotatedW / 2.0f;
        float cy = c->mRotatedY + c->mRotatedH / 2.0f;
        float dx = worldx - cx;
        float dy = worldy - cy;
        if (dx * dx + dy * dy < r * r)
            return c;
    }
    return NULL;
}

static int chip_index_of(Chip *c)
{
    if (!c)
        return -1;
    for (size_t i = 0; i < gChip.size(); i++)
    {
        if (gChip[i] == c)
            return (int)i;
    }
    return -1;
}

static int split_wire_middle_at(float worldx, float worldy, int wireid);
static int drop_routing_anchor_at(float worldx, float worldy);

int split_wire(int aDoSplit)
{
    if (gZoomFactor < 1.0f)
        gZoomFactor = 1.0f;
    float endTol = UiTheme::wireEndTolerance(gZoomFactor, gConfig.mLineEndTolerance);
    float worldmousex = ((gUIState.mousex - gConfig.mToolkitWidth) / gZoomFactor) - gWorldOfsX;
    float worldmousey = ((gUIState.mousey - gTopbarH) / gZoomFactor) - gWorldOfsY;
    float pos1[2], pos2[2], pos3[2];
    int wireid;
    if (aDoSplit)
    {
        if (!IS_WIRE_ID(gUIState.activeitem))
            return 0;
        wireid = GET_WIRE_ID(gUIState.activeitem);
    }
    else
    {
        if (!IS_WIRE_ID(gUIState.hotitem))
            return 0;
        wireid = GET_WIRE_ID(gUIState.hotitem);
    }
    if (wireid < 0 || wireid >= (int)gWire.size())
        return 0;
    Wire *w = gWire[wireid];
    if (!w || !w->mFirst || !w->mSecond || !w->mFirst->mHost || !w->mSecond->mHost)
        return 0;
    pos1[0] = w->mFirst->mHost->mRotatedX + w->mFirst->mRotatedX + 0.25f;
    pos1[1] = w->mFirst->mHost->mRotatedY + w->mFirst->mRotatedY + 0.25f;
    pos2[0] = w->mSecond->mHost->mRotatedX + w->mSecond->mRotatedX + 0.25f;
    pos2[1] = w->mSecond->mHost->mRotatedY + w->mSecond->mRotatedY + 0.25f;
    pos1[0] = WORLDTOSCREENX(pos1[0]);
    pos1[1] = WORLDTOSCREENY(pos1[1]);
    pos2[0] = WORLDTOSCREENX(pos2[0]);
    pos2[1] = WORLDTOSCREENY(pos2[1]);
    if (aDoSplit)
    {
        pos3[0] = gUIState.mousedownx;
        pos3[1] = gUIState.mousedowny;
    }
    else
    {
        pos3[0] = gUIState.mousex;
        pos3[1] = gUIState.mousey;
    }

    float wirelen = sqrt((pos1[0]-pos2[0])*(pos1[0]-pos2[0]) + (pos1[1]-pos2[1])*(pos1[1]-pos2[1]));
    float p1dist = sqrt((pos1[0]-pos3[0])*(pos1[0]-pos3[0]) + (pos1[1]-pos3[1])*(pos1[1]-pos3[1]));
    float p2dist = sqrt((pos2[0]-pos3[0])*(pos2[0]-pos3[0]) + (pos2[1]-pos3[1])*(pos2[1]-pos3[1]));
        
    if (p1dist < wirelen * endTol)
    {
        if (!aDoSplit) return 0;
        // start new line from pin 1
        gDragMode = DRAGMODE_WIRE;
        gWireStartDrag = w->mFirst;
        gUIState.activeitem = getChipIdForPad(gWireStartDrag);
    }
    else
    if (p2dist < wirelen * endTol)
    {
        if (!aDoSplit) return 0;
        // start new line from pin 2
        gDragMode = DRAGMODE_WIRE;
        gWireStartDrag = w->mSecond;
        gUIState.activeitem = getChipIdForPad(gWireStartDrag);
    }
    else
    {
        if (!aDoSplit) return 1;
        // do the split
        split_wire_middle_at(worldmousex, worldmousey, wireid);
    }
    return 0;
}
// Split wire[wireid] with a Connection Pin at (worldx, worldy), rewiring the
// second half through the new pin so the wire gains a bend point.
// Returns the new chip index, or -1 on failure.
static int split_wire_middle_at(float worldx, float worldy, int wireid)
{
    if (wireid < 0 || wireid >= (int)gWire.size())
        return -1;
    Wire *w = gWire[wireid];
    if (!w || !w->mFirst || !w->mSecond || !w->mFirst->mHost || !w->mSecond->mHost)
        return -1;
    // Reuse a bend point that is already here instead of stacking a duplicate.
    Chip *reuse = find_anchor_near(worldx, worldy);
    if (reuse && !reuse->mPin.empty() && reuse->mPin[0])
    {
        int idx = chip_index_of(reuse);
        if (w->mFirst == reuse->mPin[0] || w->mSecond == reuse->mPin[0])
        {
            gUIState.activeitem = CHIP_ID(0, idx);
            gUIState.kbditem = gUIState.activeitem;
            return idx;
        }
        save_undo();
        Pin *second = w->mSecond;
        w->mSecond = reuse->mPin[0];
        add_wire(reuse->mPin[0], second);
        gUIState.activeitem = CHIP_ID(0, idx);
        gUIState.kbditem = gUIState.activeitem;
        return idx;
    }
    if (gChipFactory.empty() || !gChipFactory[0])
        return -1;
    save_undo();
    Chip *newpin = gChipFactory[0]->build("Connection Pin");
    if (!newpin || newpin->mPin.empty() || !newpin->mPin[0])
    {
        delete newpin;
        return -1;
    }
    gChip.push_back(newpin);
    gChipName.push_back("Connection Pin");
    newpin->mX = UiTheme::snapWorld(worldx, gSnap) - 0.5f;
    newpin->mY = UiTheme::snapWorld(worldy, gSnap) - 0.5f;
    newpin->rotate(0);
    gUIState.mousedownx = (float)gUIState.mousex;
    gUIState.mousedowny = (float)gUIState.mousey;
    Pin *second = w->mSecond;
    w->mSecond = newpin->mPin[0];
    add_wire(newpin->mPin[0], second);
    gUIState.activeitem = CHIP_ID(0, (int)gChip.size() - 1);
    gUIState.kbditem = gUIState.activeitem;
    return (int)gChip.size() - 1;
}

// While routing (DRAGMODE_WIRE), releasing over empty canvas drops an anchor
// at the release point and keeps routing from it. Releases within a world
// unit of the drag origin count as plain clicks and do nothing.
static int drop_routing_anchor_at(float worldx, float worldy)
{
    if (!gWireStartDrag || !gWireStartDrag->mHost)
        return -1;
    if (gChipFactory.empty() || !gChipFactory[0])
        return -1;
    float sx = gWireStartDrag->mHost->mRotatedX + gWireStartDrag->mRotatedX + 0.25f;
    float sy = gWireStartDrag->mHost->mRotatedY + gWireStartDrag->mRotatedY + 0.25f;
    float dx = worldx - sx;
    float dy = worldy - sy;
    if (sqrtf(dx * dx + dy * dy) <= 1.0f)
        return -1;
    // Reuse a bend point that is already here instead of stacking a duplicate.
    Chip *reuse = find_anchor_near(worldx, worldy);
    if (reuse && !reuse->mPin.empty() && reuse->mPin[0])
    {
        int idx = chip_index_of(reuse);
        if (gWireStartDrag == reuse->mPin[0])
            return idx;
        save_undo();
        add_wire(gWireStartDrag, reuse->mPin[0]);
        gWireStartDrag = reuse->mPin[0];
        gUIState.mousedownx = (float)gUIState.mousex;
        gUIState.mousedowny = (float)gUIState.mousey;
        gUIState.activeitem = CHIP_ID(0, idx);
        gUIState.kbditem = gUIState.activeitem;
        return idx;
    }
    save_undo();
    Chip *newpin = gChipFactory[0]->build("Connection Pin");
    if (!newpin || newpin->mPin.empty() || !newpin->mPin[0])
    {
        delete newpin;
        return -1;
    }
    gChip.push_back(newpin);
    gChipName.push_back("Connection Pin");
    newpin->mX = UiTheme::snapWorld(worldx, gSnap) - 0.5f;
    newpin->mY = UiTheme::snapWorld(worldy, gSnap) - 0.5f;
    newpin->rotate(0);
    add_wire(gWireStartDrag, newpin->mPin[0]);
    gWireStartDrag = newpin->mPin[0];
    gUIState.mousedownx = (float)gUIState.mousex;
    gUIState.mousedowny = (float)gUIState.mousey;
    gUIState.activeitem = CHIP_ID(0, (int)gChip.size() - 1);
    gUIState.kbditem = gUIState.activeitem;
    return (int)gChip.size() - 1;
}


void multiselect_active()
{
    if (IS_CHIP_ID(gUIState.kbditem))
    {
        int id = GET_CHIP_ID(gUIState.kbditem);
        if (id >= 0 && id < (int)gChip.size() && gChip[id] && gChip[id]->mMultiSelectState == 0)
        {
            gMultiSelectChip.push_back(gChip[id]);
            gChip[id]->mMultiSelectState = 1;
        }
    }

    if (IS_WIRE_ID(gUIState.kbditem))
    {
        int id = GET_WIRE_ID(gUIState.kbditem);
        if (id >= 0 && id < (int)gWire.size() && gWire[id] && gWire[id]->mMultiSelectState == 0)
        {
            gMultiSelectWire.push_back(gWire[id]);
            gWire[id]->mMultiSelectState = 1;
        }
    }
}

void move_chip(Chip *c, int charcode)
{
	if (!c)
		return;
	switch (charcode)
    {
    case SDLK_LEFT:
        c->mX-=0.5;
        break;
    case SDLK_RIGHT:
        c->mX+=0.5;
        break;
    case SDLK_UP:
        c->mY-=0.5;
        break;
    case SDLK_DOWN:
        c->mY+=0.5;
        break;
    }
    c->rotate(c->mAngleIn90DegreeSteps);
}

void do_build_nets();

static void draw_screen()
{
    int i;
    int tick = SDL_GetTicks();
    static int slidervalue = 0;
    {
        // Surface a newer release once; stays silent when up to date,
        // offline, or unsupported. The dialog blocks like other prompts.
        char ver[64];
        char url[256];
        if (AppUpdate_Poll(ver, (int)sizeof(ver), url, (int)sizeof(url)))
        {
            char msg[512];
            snprintf(msg, sizeof(msg),
                "Update available: %s\nYou have: %s\n\nDownload:\n%s",
                ver, ATANUAVERSION, url);
            okcancel(msg);
        }
    }
    UiTheme::TopbarLayout tb = UiTheme::topbarLayout(gScreenWidth);
    gTopbarH = tb.topH;
    float worldmousex = ((gUIState.mousex - gConfig.mToolkitWidth) / gZoomFactor) - gWorldOfsX;
    float worldmousey = ((gUIState.mousey - gTopbarH) / gZoomFactor) - gWorldOfsY;
    float worldmousedownx = ((gUIState.mousedownx - gConfig.mToolkitWidth) / gZoomFactor) - gWorldOfsX;
    float worldmousedowny = ((gUIState.mousedowny - gTopbarH) / gZoomFactor) - gWorldOfsY;
    static float physicstick = 0;
    static int lasttick = 0;
    int mousemode = 0;
    static int lastmousemode = 0;
    static int sMoveUndoSaved = 0;
    static int sPrevDown_Move = 0;
    if (gUIState.mousedown && !sPrevDown_Move)
        sMoveUndoSaved = 0;
    sPrevDown_Move = gUIState.mousedown ? 1 : 0;


	if (gSavePNG)
	{
		do_screengrab();
		gSavePNG = 0;
	}

    ////////////////////////////////////
    // Physics
    ////////////////////////////////////

    if (tick - lasttick > 500)
    {
        // time warp..
        lasttick = tick;
    }

    if (tick - lasttick < 10)
    {
        SDL_Delay(5);
        return;
    }

	// avoid physics getting stuck
	if (physicstick > 10000.0)
	{
		physicstick = 0;
	}

    static int physics_iterations = 0;
    int physms = SDL_GetTicks();

	do_build_nets();

	int chips = (signed)gChip.size();
	int nets = (signed)gNet.size();

    while (lasttick < tick)
    {
		int k;
		for (k = 0; k < gConfig.mPhysicsKHz; k++)
		{

			gAudioOut = gAudioBuffer + gRecordHead;
			gRecordHead++;
			gRecordHead &= AUDIOBUF_SIZE-1;

//#pragma omp parallel default(shared) num_threads(4)

			// Chip updates could be split into jobs for a thread pool.
//#pragma omp for 
			for (i = 0; i < chips; i++)
			{
				if (gChip[i]->mDirty)
				{
					gChip[i]->mDirty = 0;
					gChip[i]->update(physicstick);
				}
			}
		
			// Net updates are relatively simple, and there's relatively few nets..
//#pragma omp for 
			for (i = 0; i < nets; i++)
			{
				if (gNet[i]->mDirty)
				{
					gNet[i]->mDirty = 0;
					gNet[i]->update();
				}
				else
				{
					gNet[i]->mHighFreqChanges = 0;
				}
			}
			physics_iterations++;
        
			physicstick += 1.0 / gConfig.mPhysicsKHz;
		}
       
        lasttick += 1;

        // don't allow for physics to drop framerate too low
        
        if ((signed)SDL_GetTicks() - tick > gConfig.mMaxPhysicsMs)
        {
            lasttick = tick;
        }        
    }

    physms = SDL_GetTicks() - physms;

    ////////////////////////////////////
    // Rendering
    ////////////////////////////////////

    if (gScreenWidth < 100)
        gScreenWidth = 100;
    if (gScreenHeight < 100)
        gScreenHeight = 100;
    if (gBlackBackground)
		glClearColor(0.086f, 0.094f, 0.114f, 1.0f);
	else
		glClearColor(0.941f, 0.945f, 0.957f, 1.0f);
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

	drawrect(0, 0, gConfig.mToolkitWidth, gScreenHeight, C_MENUBG);
	drawrect(0, 0, gScreenWidth, gTopbarH, C_MENUBG);
	drawrect(gConfig.mToolkitWidth, 0, 1, gScreenHeight, C_MENULINE);
	drawrect(0, gTopbarH, gScreenWidth, 1, C_MENULINE);

    imgui_prepare();

    glEnable(GL_SCISSOR_TEST);
    glScissor(0,0,gConfig.mToolkitWidth-20,gScreenHeight - gTopbarH);

    int loc = -1;
    int chipListCount = (int)gAvailableChip[gVisibleChiplist < 0 || gVisibleChiplist > 4 ? 0 : gVisibleChiplist].size();
    if (gVisibleChiplist < 0 || gVisibleChiplist > 4)
        gVisibleChiplist = 0;
    if (gUIState.scroll && gUIState.mousex < gConfig.mToolkitWidth && gUIState.mousey > gTopbarH)
    {
        slidervalue -= gUIState.scroll * UI_ROW_H * 3;
        int max = UiTheme::clampSliderMax(chipListCount, UI_ROW_H, gScreenHeight - gTopbarH);
        slidervalue = UiTheme::clampSliderValue(slidervalue, max);
    }
    if (gUIState.mousex < gConfig.mToolkitWidth-20 && gUIState.mousey > gTopbarH)
    {
        loc = UiTheme::chipListIndex(gUIState.mousey, gTopbarH, slidervalue, UI_ROW_H, chipListCount);
    }    

    for (i = 0; i < (signed)gAvailableChip[gVisibleChiplist].size(); i++)
    {
        if (gAvailableChip[gVisibleChiplist][i])
        {
            float rowY = (float)(gTopbarH + i * UI_ROW_H - slidervalue);
            if (rowY + UI_ROW_H >= gTopbarH && rowY <= gScreenHeight)
            {
            if (loc == i)
            {
                gUIState.hotitem = NEWCHIP_ID(loc);
                if(gDragMode == DRAGMODE_NEWCHIP && gUIState.activeitem == 0 && gUIState.mousedown)
                {
                    do_cancel();
                }
                else
                if (gDragMode == DRAGMODE_NONE && gUIState.activeitem == 0 && gUIState.mousedown)
                {
                    // clear multiselect if any
                    gMultiSelectChip.clear();
                    gMultiSelectWire.clear();
                    gMultiselectDirty = 1;                    
                    int j;
                    for (j = 0; gNewChip == NULL && j < (signed)gChipFactory.size(); j++)
                        gNewChip = gChipFactory[j]->build(gAvailableChip[gVisibleChiplist][i]);
                    if (gNewChip)
                    {
                        gNewChipName = gAvailableChip[gVisibleChiplist][i];
                        gDragMode = DRAGMODE_NEWCHIP;
	                    gUIState.mousedownkeymod &= ~gCloneKeyMask; // stop cloning (if pressed)
                    }
                    gUIState.activeitem = gUIState.hotitem;
                }
                if (gUIState.hotitem == gUIState.activeitem)
                    drawrect(0, gTopbarH+i*UI_ROW_H-slidervalue, gConfig.mToolkitWidth-20, UI_ROW_H, C_HOTROW);
                else
                    drawrect(0, gTopbarH+i*UI_ROW_H-slidervalue, gConfig.mToolkitWidth-20, UI_ROW_H, C_WIDGETBG);
                drawrect(0, (float)(gTopbarH+i*UI_ROW_H-slidervalue), 3, UI_ROW_H, C_WIDGETHOT);
            }
            else
            {
                drawrect(0, (float)(gTopbarH+i*UI_ROW_H-slidervalue+UI_ROW_H-1), gConfig.mToolkitWidth-20, 1, C_MENULINE);
            }
            if (!(gVisibleChiplist == 3 && i == 8))
                fn14.drawstring(gAvailableChip[gVisibleChiplist][i], 8, (float)(gTopbarH + 8 + i * UI_ROW_H - slidervalue), C_TEXT);
            }
        }
    }
    glDisable(GL_SCISSOR_TEST);
    tb = UiTheme::topbarLayout(gScreenWidth);
    gTopbarH = tb.topH;

    imgui_slider(GEN_ID,gConfig.mToolkitWidth-20,gTopbarH,20,gScreenHeight-gTopbarH,C_WIDGETBG,C_WIDGETTHUMB,C_WIDGETHOT,((signed)gAvailableChip[gVisibleChiplist].size() * UI_ROW_H) - (gScreenHeight - gTopbarH),slidervalue, (gScreenHeight - gTopbarH), UI_ROW_H);

	int tabW = tb.tabW;
	int btnW = tb.btnW;
	int quitW = tb.quitW;
	int actionY = (tb.rows == 1) ? 0 : UI_TOPBAR_H;
	const char *lblNew = tb.compactLabels ? "New" : "New\nCtrl-N";
	const char *lblLoad = tb.compactLabels ? "Load" : "Load\nCtrl-L";
	const char *lblMerge = tb.compactLabels ? "Merge" : "Merge\nCtrl-M";
	const char *lblBox = tb.compactLabels ? "Box" : "Box\nCtrl-B";
	const char *lblSave = tb.compactLabels ? "Save" : "Save\nCtrl-S";
	const char *lblUndo = tb.compactLabels ? "Undo" : "Undo\nCtrl-Z";
	const char *lblRedo = tb.compactLabels ? "Redo" : "Redo\nCtrl-Y";
	const char *lblZoom = tb.compactLabels ? "Zoom" : "Zoom\next";
	const char *lblSnap = tb.compactLabels ? (gSnap ? "Snap" : "Snap") : (gSnap ? "Snap\n(on)" : "Snap\n(off)");
	const char *lblView = tb.compactLabels ? "View" : (gLiveWires ? "View\n(live)" : "View\n(grey)");
	const char *lblPng = tb.compactLabels ? "PNG" : "PNG it\nCtrl-G";
	int xofs = 0;

    if (imgui_button(GEN_ID,fn14,"Base",xofs,0,tabW,UI_TOPBAR_H,(gVisibleChiplist==0?C_WIDGETHOT:C_WIDGETBG),C_WIDGETTHUMB,C_WIDGETHOT,C_TEXT))
    {
        int active = gUIState.kbditem;
        do_cancel();
        gUIState.kbditem = active;
        gVisibleChiplist = 0;
        slidervalue = 0;
    }
	xofs += tabW;
    if (imgui_button(GEN_ID,fn14,"Chips",xofs,0,tabW,UI_TOPBAR_H,(gVisibleChiplist==1?C_WIDGETHOT:C_WIDGETBG),C_WIDGETTHUMB,C_WIDGETHOT,C_TEXT))
    {
        int active = gUIState.kbditem;
        do_cancel();
        gUIState.kbditem = active;
        gVisibleChiplist = 1;
        slidervalue = 0;
    }
	xofs += tabW;
    if (imgui_button(GEN_ID,fn14,"In",xofs,0,tabW,UI_TOPBAR_H,(gVisibleChiplist==2?C_WIDGETHOT:C_WIDGETBG),C_WIDGETTHUMB,C_WIDGETHOT,C_TEXT))
    {
        int active = gUIState.kbditem;
        do_cancel();
        gUIState.kbditem = active;
        gVisibleChiplist = 2;
        slidervalue = 0;
    }
	xofs += tabW;
    if (imgui_button(GEN_ID,fn14,"Out",xofs,0,tabW,UI_TOPBAR_H,(gVisibleChiplist==3?C_WIDGETHOT:C_WIDGETBG),C_WIDGETTHUMB,C_WIDGETHOT,C_TEXT))
    {
        int active = gUIState.kbditem;
        do_cancel();
        gUIState.kbditem = active;
        gVisibleChiplist = 3;
        slidervalue = 0;
    }
	xofs += tabW;
    if (imgui_button(GEN_ID,fn14,"Misc",xofs,0,tabW,UI_TOPBAR_H,(gVisibleChiplist==4?C_WIDGETHOT:C_WIDGETBG),C_WIDGETTHUMB,C_WIDGETHOT,C_TEXT))
    {
        int active = gUIState.kbditem;
        do_cancel();
        gUIState.kbditem = active;
        gVisibleChiplist = 4;
        slidervalue = 0;
    }
    drawrect((float)(gVisibleChiplist * tabW), (float)(UI_TOPBAR_H - 3), (float)tabW, 3, C_WIDGETHOT);
	xofs += tabW;
	if (tb.rows == 1)
	{
	xofs += tb.gapA;
    if (imgui_button(GEN_ID,fn14,lblNew,xofs,0,btnW,UI_TOPBAR_H,C_WIDGETBG,C_WIDGETTHUMB,C_WIDGETHOT,C_TEXT))
    {
        do_resetdialog();
    }
	xofs += btnW;
    if (imgui_button(GEN_ID,fn14,lblLoad,xofs,0,btnW,UI_TOPBAR_H,C_WIDGETBG,C_WIDGETTHUMB,C_WIDGETHOT,C_TEXT))
    {
        do_loaddialog();
    }
	xofs += btnW;
    if (imgui_button(GEN_ID,fn14,lblMerge,xofs,0,btnW,UI_TOPBAR_H,C_WIDGETBG,C_WIDGETTHUMB,C_WIDGETHOT,C_TEXT))
    {
        do_loaddialog(1);
    }
	xofs += btnW;
    if (imgui_button(GEN_ID,fn14,lblBox,xofs,0,btnW,UI_TOPBAR_H,C_WIDGETBG,C_WIDGETTHUMB,C_WIDGETHOT,C_TEXT))
    {
        do_loaddialog(2);
    }
	xofs += btnW;
    if (imgui_button(GEN_ID,fn14,lblSave,xofs,0,btnW,UI_TOPBAR_H,C_WIDGETBG,C_WIDGETTHUMB,C_WIDGETHOT,C_TEXT))
    {
        do_savedialog();
    }
	xofs += btnW;
	xofs += tb.gapB;
    if (imgui_button(GEN_ID,fn14,lblUndo,xofs,0,btnW,UI_TOPBAR_H,C_WIDGETBG,C_WIDGETTHUMB,C_WIDGETHOT,C_TEXT))
    {
        int active = gUIState.kbditem;
        do_undo();
        gUIState.kbditem = active;
    }
	xofs += btnW;
    if (imgui_button(GEN_ID,fn14,lblRedo,xofs,0,btnW,UI_TOPBAR_H,C_WIDGETBG,C_WIDGETTHUMB,C_WIDGETHOT,C_TEXT))
    {
        int active = gUIState.kbditem;
        do_redo();
        gUIState.kbditem = active;
    }
	xofs += btnW;
	xofs += tb.gapC;
    if (imgui_button(GEN_ID,fn14,"Home",xofs,0,btnW,UI_TOPBAR_H,C_WIDGETBG,C_WIDGETTHUMB,C_WIDGETHOT,C_TEXT))
    {
        do_home();
    }
	xofs += btnW;
    if (imgui_button(GEN_ID,fn14,lblZoom,xofs,0,btnW,UI_TOPBAR_H,C_WIDGETBG,C_WIDGETTHUMB,C_WIDGETHOT,C_TEXT))
    {
        do_zoomext();
    }
	xofs += btnW;
    if (imgui_button(GEN_ID,fn14,lblSnap,xofs,0,btnW,UI_TOPBAR_H,C_WIDGETBG,C_WIDGETTHUMB,C_WIDGETHOT,C_TEXT))
    {
        gSnap = !gSnap;
    }
	xofs += btnW;
    if (imgui_button(GEN_ID,fn14,lblView,xofs,0,btnW,UI_TOPBAR_H,C_WIDGETBG,C_WIDGETTHUMB,C_WIDGETHOT,C_TEXT))
    {
        gLiveWires = !gLiveWires;
		gBlackBackground ^= gLiveWires;
    }
	xofs += btnW;
    if (imgui_button(GEN_ID,fn14,lblPng,xofs,0,btnW,UI_TOPBAR_H,C_WIDGETBG,C_WIDGETTHUMB,C_WIDGETHOT,C_TEXT))
    {
        gSavePNG = 1;
    }
	xofs += btnW;
	}
	else
	{
	xofs = 0;
    if (imgui_button(GEN_ID,fn14,lblNew,xofs,actionY,btnW,UI_TOPBAR_H,C_WIDGETBG,C_WIDGETTHUMB,C_WIDGETHOT,C_TEXT))
    {
        do_resetdialog();
    }
	xofs += btnW;
    if (imgui_button(GEN_ID,fn14,lblLoad,xofs,actionY,btnW,UI_TOPBAR_H,C_WIDGETBG,C_WIDGETTHUMB,C_WIDGETHOT,C_TEXT))
    {
        do_loaddialog();
    }
	xofs += btnW;
    if (imgui_button(GEN_ID,fn14,lblMerge,xofs,actionY,btnW,UI_TOPBAR_H,C_WIDGETBG,C_WIDGETTHUMB,C_WIDGETHOT,C_TEXT))
    {
        do_loaddialog(1);
    }
	xofs += btnW;
    if (imgui_button(GEN_ID,fn14,lblBox,xofs,actionY,btnW,UI_TOPBAR_H,C_WIDGETBG,C_WIDGETTHUMB,C_WIDGETHOT,C_TEXT))
    {
        do_loaddialog(2);
    }
	xofs += btnW;
    if (imgui_button(GEN_ID,fn14,lblSave,xofs,actionY,btnW,UI_TOPBAR_H,C_WIDGETBG,C_WIDGETTHUMB,C_WIDGETHOT,C_TEXT))
    {
        do_savedialog();
    }
	xofs += btnW;
	xofs += tb.gapB;
    if (imgui_button(GEN_ID,fn14,lblUndo,xofs,actionY,btnW,UI_TOPBAR_H,C_WIDGETBG,C_WIDGETTHUMB,C_WIDGETHOT,C_TEXT))
    {
        int active = gUIState.kbditem;
        do_undo();
        gUIState.kbditem = active;
    }
	xofs += btnW;
    if (imgui_button(GEN_ID,fn14,lblRedo,xofs,actionY,btnW,UI_TOPBAR_H,C_WIDGETBG,C_WIDGETTHUMB,C_WIDGETHOT,C_TEXT))
    {
        int active = gUIState.kbditem;
        do_redo();
        gUIState.kbditem = active;
    }
	xofs += btnW;
	xofs += tb.gapC;
    if (imgui_button(GEN_ID,fn14,"Home",xofs,actionY,btnW,UI_TOPBAR_H,C_WIDGETBG,C_WIDGETTHUMB,C_WIDGETHOT,C_TEXT))
    {
        do_home();
    }
	xofs += btnW;
    if (imgui_button(GEN_ID,fn14,lblZoom,xofs,actionY,btnW,UI_TOPBAR_H,C_WIDGETBG,C_WIDGETTHUMB,C_WIDGETHOT,C_TEXT))
    {
        do_zoomext();
    }
	xofs += btnW;
    if (imgui_button(GEN_ID,fn14,lblSnap,xofs,actionY,btnW,UI_TOPBAR_H,C_WIDGETBG,C_WIDGETTHUMB,C_WIDGETHOT,C_TEXT))
    {
        gSnap = !gSnap;
    }
	xofs += btnW;
    if (imgui_button(GEN_ID,fn14,lblView,xofs,actionY,btnW,UI_TOPBAR_H,C_WIDGETBG,C_WIDGETTHUMB,C_WIDGETHOT,C_TEXT))
    {
        gLiveWires = !gLiveWires;
		gBlackBackground ^= gLiveWires;
    }
	xofs += btnW;
    if (imgui_button(GEN_ID,fn14,lblPng,xofs,actionY,btnW,UI_TOPBAR_H,C_WIDGETBG,C_WIDGETTHUMB,C_WIDGETHOT,C_TEXT))
    {
        gSavePNG = 1;
    }
	xofs += btnW;
	}

    if (imgui_button(GEN_ID,fn14,"Quit",tb.quitX,0,quitW,UI_TOPBAR_H,C_WIDGETBG,C_WIDGETTHUMB,C_WIDGETHOT,C_TEXT))
    {
        if (okcancel("Are you sure you want to exit?\nAny unsaved changes will be lost."))
		{
            exit(0);
		}
    }
  
    if (gUIState.mousex > gConfig.mToolkitWidth && gUIState.mousey > gTopbarH)
    {
        if (gDragMode == DRAGMODE_NONE)
        {
            // Check for collisions with wires
            float pickTol = UiTheme::wirePickTolerance(gZoomFactor, gConfig.mLinePickTolerance);
            for (i = 0; i < (signed)gWire.size(); i++)
            {
				if (!gWire[i] || gWire[i]->mBox != 0)
					continue;
                Pin * a, * b;
                a = gWire[i]->mFirst;
                b = gWire[i]->mSecond;
                if (!a || !b || !a->mHost || !b->mHost)
                    continue;
                if (line_point_distance(worldmousex,
                                        worldmousey,
                                        a->mHost->mRotatedX + a->mRotatedX + 0.25,
                                        a->mHost->mRotatedY + a->mRotatedY + 0.25,
                                        b->mHost->mRotatedX + b->mRotatedX + 0.25,
                                        b->mHost->mRotatedY + b->mRotatedY + 0.25) < pickTol)
                {
                    gUIState.hotitem = WIRE_ID(i);
                    int newly_multiselected = 0;
                    if (gUIState.keymod & gSelectKeyMask && gUIState.mousedown && gWire[i]->mMultiSelectState == 0)
                    {
                        newly_multiselected = 1;
                        multiselect_active();
                        gMultiSelectWire.push_back(gWire[i]);
                        gWire[i]->mMultiSelectState = 1;
                        gMultiselectDirty = 1;
                    } 

                    if (gUIState.activeitem == 0 && gUIState.mousedown)
                    {
                        gUIState.activeitem = gUIState.hotitem;
                        if (gUIState.activeitem == WIRE_ID(i))
                            gUIState.kbditem = gUIState.activeitem;
                        if (gWire[i]->mMultiSelectState == 0 && !(gUIState.keymod & gSelectKeyMask))
                        {
                            gMultiselectDirty = 1;
                            gMultiSelectWire.clear();
                            gMultiSelectChip.clear();
                        }
                    }                    
                }
            }
        }

        if (!IS_WIRE_ID(gUIState.hotitem) || gDragMode != DRAGMODE_NONE)
        {
            // Check collisions with chips
            for (i = 0; i < (signed)gChip.size(); i++)
            {
                if (!gChip[i] || gChip[i]->mBox != 0)
                    continue;
                // Tiny anchor chips get a padded grab area so 1x1 bend
                // points stay clickable at any zoom.
                int tinyAnchor = (gChip[i]->mRotatedW < 2.0f || gChip[i]->mRotatedH < 2.0f) ? 1 : 0;
                float grabPad = 0.0f;
                if (tinyAnchor)
                    grabPad = UiTheme::anchorGrabPad(gZoomFactor);
                if (gChip[i]->mBox == 0 &&
					worldmousex > gChip[i]->mRotatedX - grabPad &&
                    worldmousey > gChip[i]->mRotatedY - grabPad &&
                    worldmousex < gChip[i]->mRotatedX + gChip[i]->mRotatedW + grabPad &&
                    worldmousey < gChip[i]->mRotatedY + gChip[i]->mRotatedH + grabPad)
                {
                    gUIState.hotitem = CHIP_ID(0, i);

                    int j;
                    for (j = 0; j < (signed)gChip[i]->mPin.size(); j++)
                    {
                        if (!gChip[i]->mPin[j])
                            continue;
                        if (tinyAnchor)
                        {
                            // Inner square starts a connection, outer area moves.
                            float pcx = gChip[i]->mRotatedX + gChip[i]->mPin[j]->mRotatedX + 0.25f;
                            float pcy = gChip[i]->mRotatedY + gChip[i]->mPin[j]->mRotatedY + 0.25f;
                            if (UiTheme::anchorHotZone(worldmousex - pcx, worldmousey - pcy))
                                gUIState.hotitem = CHIP_ID(j + 1, i);
                        }
                        else
                        {
                            // Padded connection zone: grabbing a pin beats
                            // nearby wires, the body around it still moves.
                            float pp = UiTheme::pinGrabPad(gZoomFactor);
                            if (worldmousex > gChip[i]->mRotatedX + gChip[i]->mPin[j]->mRotatedX - pp &&
                                worldmousey > gChip[i]->mRotatedY + gChip[i]->mPin[j]->mRotatedY - pp &&
                                worldmousex < gChip[i]->mRotatedX + gChip[i]->mPin[j]->mRotatedX + 0.5 + pp &&
                                worldmousey < gChip[i]->mRotatedY + gChip[i]->mPin[j]->mRotatedY + 0.5 + pp)
                            {
                                gUIState.hotitem = CHIP_ID(j + 1, i);
                            }
                        }
                    }
                    if (gUIState.keymod & gSelectKeyMask && gUIState.mousedown && gChip[i]->mMultiSelectState == 0)
                    {
                        multiselect_active();
                        if (gChip[i]->mMultiSelectState == 0)
                        {
                            gMultiSelectChip.push_back(gChip[i]);
                            gChip[i]->mMultiSelectState = 1;
                        }
                        gMultiselectDirty = 1;
                    }                    

                    if (gUIState.activeitem == 0 && gUIState.mousedown)
                    {
                        gUIState.activeitem = gUIState.hotitem;
                        if (gUIState.activeitem == CHIP_ID(0,i))
                        {
                            gUIState.kbditem = gUIState.activeitem;
                        }
                        if (gChip[i]->mMultiSelectState == 0 && !(gUIState.keymod & gSelectKeyMask))
                        {
                            gMultiselectDirty = 1;
                            gMultiSelectWire.clear();
                            gMultiSelectChip.clear();
                        }
                    }
                    
                }
            }
        }

        if (IS_CHIP_ID(gUIState.activeitem))
        {
            int activeChip = GET_CHIP_ID(gUIState.activeitem);
            int activePin = GET_PIN_ID(gUIState.activeitem);
            if (activeChip < 0 || activeChip >= (int)gChip.size() || !gChip[activeChip])
            {
                gUIState.activeitem = 0;
            }
            else if (activePin > 0)
            {
                if (activePin - 1 >= (int)gChip[activeChip]->mPin.size())
                {
                    gUIState.activeitem = 0;
                }
                else
                {
                Pin *hotpin = gChip[activeChip]->mPin[activePin-1];
                if (!hotpin)
                {
                    gUIState.activeitem = 0;
                }
                else
                {

                if (gDragMode == DRAGMODE_WIRE)
                {
                    // Accept clicking on a pad to make connection
                    if (gUIState.activeitem == gUIState.hotitem && !gUIState.mousedown)
                    {
                        if (gWireStartDrag && gWireStartDrag != hotpin)
                        {
                            save_undo();
                            add_wire(gWireStartDrag, hotpin);
                            gDragMode = DRAGMODE_NONE;
                        }
                    }
                    // Accept drag-end to make connection
                    if (gUIState.activeitem != gUIState.hotitem && !gUIState.mousedown && IS_CHIP_ID(gUIState.hotitem) && GET_PIN_ID(gUIState.hotitem) > 0)
                    {
                        int hotChip = GET_CHIP_ID(gUIState.hotitem);
                        int hotPin = GET_PIN_ID(gUIState.hotitem);
                        if (hotChip >= 0 && hotChip < (int)gChip.size() && gChip[hotChip] && hotPin - 1 < (int)gChip[hotChip]->mPin.size())
                        {
                            Pin *targetPin = gChip[hotChip]->mPin[hotPin-1];
                            if (targetPin && gWireStartDrag && gWireStartDrag != targetPin)
                            {
                                save_undo();
                                add_wire(gWireStartDrag, targetPin);
                                gDragMode = DRAGMODE_NONE;
                            }
                        }
                    }
                    // Release mid-drag over empty canvas (or a wire, but no
                    // pin): drop an anchor here and keep routing from it.
                    // Edge-triggered so one release drops exactly one anchor.
                    {
                        static int sWireWasDown = 0;
                        if (gUIState.mousedown)
                            sWireWasDown = 1;
                        if (!gUIState.mousedown && sWireWasDown &&
                            !(IS_CHIP_ID(gUIState.hotitem) && GET_PIN_ID(gUIState.hotitem) > 0))
                        {
                            sWireWasDown = 0;
                            // A release near (but not exactly on) a pin still
                            // means that pin: finish there instead of
                            // stranding an anchor beside it.
                            float snapR = UiTheme::wireFinishSnap(gZoomFactor, gConfig.mLineEndTolerance);
                            Pin *target = find_release_pin(worldmousex, worldmousey, snapR);
                            if (target && gWireStartDrag && target != gWireStartDrag)
                            {
                                save_undo();
                                add_wire(gWireStartDrag, target);
                                gDragMode = DRAGMODE_NONE;
                            }
                            else if (!target)
                            {
                                drop_routing_anchor_at(worldmousex, worldmousey);
                            }
                        }
                    }
                }
                else
                if (gDragMode == DRAGMODE_NONE)
                {
                    gDragMode = DRAGMODE_WIRE;
                    gWireStartDrag = hotpin;
                }
                }
                }
            }
            else
            {
                i = GET_CHIP_ID(gUIState.activeitem);
                if (i < 0 || i >= (int)gChip.size() || !gChip[i])
                {
                    gUIState.activeitem = 0;
                }
                else
                // Don't actually move a chip if ctrl is pressed
                if (!(gUIState.mousedownkeymod & gCloneKeyMask))
                {
                    // handle dragging
                    float movex = worldmousex - worldmousedownx;
                    float movey = worldmousey - worldmousedowny;

                    if (gSnap)
                    {
                        float newx = floor((gChip[i]->mX+movex)*2+0.5)/2;
                        float newy = floor((gChip[i]->mY+movey)*2+0.5)/2;
                        movex = newx - gChip[i]->mX;
                        movey = newy - gChip[i]->mY;
                    }
                    // Snapshot once per drag so Undo restores the pre-move
                    // positions instead of skipping to an older action.
                    if (!sMoveUndoSaved && (movex != 0.0f || movey != 0.0f))
                    {
                        save_undo();
                        sMoveUndoSaved = 1;
                    }
                    if (!gMultiSelectChip.empty())
                    {
                        // multi-select move
                        int c;
                        for (c = 0; c < (signed)gMultiSelectChip.size(); c++)
                        {
                            gMultiSelectChip[c]->mX += movex;
                            gMultiSelectChip[c]->mY += movey;
                            gMultiSelectChip[c]->rotate(gMultiSelectChip[c]->mAngleIn90DegreeSteps);
                        }
                    }
                    else
                    {
                        // not multi-select
                        gChip[i]->mX += movex;
                        gChip[i]->mY += movey;
                        gChip[i]->rotate(gChip[i]->mAngleIn90DegreeSteps);
                    }
                    gUIState.mousedownx += movex * gZoomFactor;
                    gUIState.mousedowny += movey * gZoomFactor;
                }
            }
        }   

        // If nothing is active so far, we're in "move the world" mode
        if (gDragMode == DRAGMODE_NONE && gUIState.activeitem == -1 && gUIState.mousedown)
        {
            if (gUIState.keymod & gSelectKeyMask)
            {
                // Move to select-drag mode
                gDragMode = DRAGMODE_SELECT;
            }
            else
            {
                int dx = (int)floor(gUIState.mousedownx - gUIState.mousex);
                int dy = (int)floor(gUIState.mousedowny - gUIState.mousey);
                gWorldOfsX -= dx / gZoomFactor;
                gWorldOfsY -= dy / gZoomFactor;
                gUIState.mousedownx -= dx;
                gUIState.mousedowny -= dy;
                gUIState.kbditem = 0; 
                mousemode = 1;
                if (!(gUIState.keymod & gSelectKeyMask) && (!gMultiSelectWire.empty() || !gMultiSelectChip.empty()))
                {
                    gMultiselectDirty = 1;
                    gMultiSelectWire.clear();
                    gMultiSelectChip.clear();
                }
            }
        }

        if (gDragMode == DRAGMODE_SELECT && !gUIState.mousedown)
        {
            gDragMode = DRAGMODE_NONE;
            gMultiselectDirty = 1;
            for (i = 0; i < (signed)gChip.size(); i++)
            {
				if (gChip[i]->mBox != 0)
					continue;
				
				if (rect_rect_collide(worldmousex, worldmousey, 
									  worldmousedownx, worldmousedowny,
									  gChip[i]->mRotatedX, gChip[i]->mRotatedY, 
									  gChip[i]->mRotatedX + gChip[i]->mRotatedW, gChip[i]->mRotatedY + gChip[i]->mRotatedH))
				{
					if (gChip[i]->mMultiSelectState == 0)
					{
						gMultiSelectChip.push_back(gChip[i]);
						gChip[i]->mMultiSelectState = 1;                        
					}
				}
				
            }
            for (i = 0; i < (signed)gWire.size(); i++)            
            {
				if (gWire[i]->mBox != 0)
					continue;
                Pin * a, * b;
                a = gWire[i]->mFirst;
                b = gWire[i]->mSecond;
                if (rect_line_collide(worldmousex, worldmousey, 
                                      worldmousedownx, worldmousedowny,
                                      a->mHost->mRotatedX + a->mRotatedX + 0.25, a->mHost->mRotatedY + a->mRotatedY + 0.25,
                                      b->mHost->mRotatedX + b->mRotatedX + 0.25, b->mHost->mRotatedY + b->mRotatedY + 0.25))
                {
                    if (gWire[i]->mMultiSelectState == 0)
                    {
                        gMultiSelectWire.push_back(gWire[i]);
                        gWire[i]->mMultiSelectState = 1;
                    }
                }
            }
        }

        // If wire is being dragged, when far enough, split the wire with pin
        if (gDragMode == DRAGMODE_NONE && IS_WIRE_ID(gUIState.activeitem))
        {
            int splitId = GET_WIRE_ID(gUIState.activeitem);
            if (splitId >= 0 && splitId < (int)gWire.size() && gWire[splitId])
            {
            float dist = sqrt(((float)gUIState.mousex - (float)gUIState.mousedownx) * ((float)gUIState.mousex - (float)gUIState.mousedownx) +
                              ((float)gUIState.mousey - (float)gUIState.mousedowny) * ((float)gUIState.mousey - (float)gUIState.mousedowny));
            if (dist > gConfig.mLineSplitDragDistance)
            {
                // clear multiselect if any
                gMultiSelectChip.clear();
                gMultiSelectWire.clear();
                gMultiselectDirty = 1;
                // First check if distance from the dragged position to one of the original pins was
                // short enough, and draw a new line from said pin instead of splitting the wire.
                split_wire(1);
            }
            }
        }

        // Click (press and release without dragging) on the middle of a wire
        // drops a bend point there. Dragging still splits or rewires instead,
        // and shift-click still multi-selects.
        {
            static int sPrevDown = 0;
            static int sClickWire = -1;
            if (gUIState.mousedown && !sPrevDown)
            {
                sClickWire = -1;
                if (gDragMode == DRAGMODE_NONE && IS_WIRE_ID(gUIState.hotitem) &&
                    !(gUIState.keymod & gSelectKeyMask))
                {
                    int hid = GET_WIRE_ID(gUIState.hotitem);
                    if (hid >= 0 && hid < (int)gWire.size() && gWire[hid])
                        sClickWire = hid;
                }
            }
            if (!gUIState.mousedown && sPrevDown && sClickWire >= 0)
            {
                int wid = sClickWire;
                sClickWire = -1;
                float dx = (float)gUIState.mousex - gUIState.mousedownx;
                float dy = (float)gUIState.mousey - gUIState.mousedowny;
                float moved = sqrtf(dx * dx + dy * dy);
                if (gDragMode == DRAGMODE_NONE && wid < (int)gWire.size() && gWire[wid] &&
                    gWire[wid]->mFirst && gWire[wid]->mSecond &&
                    gWire[wid]->mFirst->mHost && gWire[wid]->mSecond->mHost &&
                    gUIState.activeitem == WIRE_ID(wid) &&
                    moved < gConfig.mLineSplitDragDistance &&
                    !(gUIState.keymod & gSelectKeyMask))
                {
                    Pin *a = gWire[wid]->mFirst;
                    Pin *b = gWire[wid]->mSecond;
                    float tol = UiTheme::wirePickTolerance(gZoomFactor, gConfig.mLinePickTolerance);
                    if (line_point_distance(worldmousex, worldmousey,
                            a->mHost->mRotatedX + a->mRotatedX + 0.25f,
                            a->mHost->mRotatedY + a->mRotatedY + 0.25f,
                            b->mHost->mRotatedX + b->mRotatedX + 0.25f,
                            b->mHost->mRotatedY + b->mRotatedY + 0.25f) < tol)
                    {
                        int savedHot = gUIState.hotitem;
                        gUIState.hotitem = WIRE_ID(wid);
                        int middle = split_wire(0);
                        gUIState.hotitem = savedHot;
                        if (middle)
                            split_wire_middle_at(worldmousex, worldmousey, wid);
                    }
                }
            }
            sPrevDown = gUIState.mousedown ? 1 : 0;
        }

        // If a chip is ctrl-dragged, clone the chip
        if (gDragMode == DRAGMODE_NONE && IS_CHIP_ID(gUIState.activeitem) && (gUIState.mousedownkeymod & gCloneKeyMask))
        {
            int oldchipid = GET_CHIP_ID(gUIState.activeitem);
            if (oldchipid >= 0 && oldchipid < (int)gChip.size() && gChip[oldchipid] && gChipName[oldchipid])
            {
            float dist = sqrt(((float)gUIState.mousex - (float)gUIState.mousedownx) * ((float)gUIState.mousex - (float)gUIState.mousedownx) +
                              ((float)gUIState.mousey - (float)gUIState.mousedowny) * ((float)gUIState.mousey - (float)gUIState.mousedowny));
            if (dist > gConfig.mChipCloneDragDistance)
            {
				save_undo();
                // clear multiselect if any
                gMultiSelectChip.clear();
                gMultiSelectWire.clear();
                gMultiselectDirty = 1;
                gNewChip = NULL;
                for (i = 0; gNewChip == NULL && i < (signed)gChipFactory.size(); i++)
				{
                    gNewChip = gChipFactory[i]->build(gChipName[oldchipid]);
				}

                if (gNewChip)
                {
                    gNewChipName = gChipName[oldchipid];

                    gChip.push_back(gNewChip);
                    gChipName.push_back(gNewChipName);
                    gNewChip->mX = worldmousex - gNewChip->mW / 2;
                    gNewChip->mY = worldmousey - gNewChip->mH / 2;
					
                    gNewChip->rotate(gChip[oldchipid]->mAngleIn90DegreeSteps);
                    gUIState.mousedownx = gUIState.mousex;
                    gUIState.mousedowny = gUIState.mousey;
                    gUIState.mousedownkeymod &= ~gCloneKeyMask; // stop cloning
                    gChip[oldchipid]->clone(gNewChip);
				
					int i;
					for (i = 0; i < (signed)gChip.size(); i++)
					{
						if (gChip[i] == gNewChip)
						{
							gUIState.activeitem = CHIP_ID(0, i);
						}
					}
                    gUIState.hotitem = gUIState.activeitem;
                    gUIState.kbditem = gUIState.activeitem;
                    gNewChip = NULL;
                    gNewChipName = NULL;
				}
            }
            }
        }

        if ((gUIState.activeitem == 0 && gUIState.scroll) || 
			gUIState.keyentered == SDLK_KP_PLUS || 
			gUIState.keyentered == SDLK_KP_MINUS ||
			gUIState.keyentered == SDLK_PAGEUP ||
			gUIState.keyentered == SDLK_PAGEDOWN)
        {
            float oldfactor = gZoomFactor;
            if (gUIState.scroll > 0 || 
				gUIState.keyentered == SDLK_KP_PLUS ||
				gUIState.keyentered == SDLK_PAGEUP)
            {
                if (gZoomFactor < 30000)
                    gZoomFactor *= 1.2;
            }
            else
            {
                if (gZoomFactor > 1)
                    gZoomFactor /= 1.2;
            }

            gWorldOfsX += (gUIState.mousex - gConfig.mToolkitWidth) / gZoomFactor - (gUIState.mousex - gConfig.mToolkitWidth) / oldfactor;
	gWorldOfsY += (gUIState.mousey - gTopbarH) / gZoomFactor - (gUIState.mousey - gTopbarH) / oldfactor;
        }
    }

    // Handle keyboard events for selected objects
    static int sLastNudgeTick = 0;
    if ((!gMultiSelectChip.empty()) || (!gMultiSelectWire.empty()))
    {    
        // multiselect mode
        if (gUIState.keyentered == SDLK_LEFT ||
            gUIState.keyentered == SDLK_RIGHT ||
            gUIState.keyentered == SDLK_UP ||
            gUIState.keyentered == SDLK_DOWN)
        {
            // One undo step per burst of nudges, not one per keypress.
            if (UiTheme::shouldSaveNudge(tick, sLastNudgeTick))
                save_undo();
            sLastNudgeTick = tick;
            for (i = 0; i < (signed)gMultiSelectChip.size(); i++)
                move_chip(gMultiSelectChip[i], gUIState.keyentered);
        }

        if (gUIState.keyentered == SDLK_DELETE)
        {
            save_undo();
            // Tricky delete operation.
            // Since wirefry is a recursive operation and deletes chips as well
            // (connection pins), we have no idea where the index goes after
            // deletion. Thus:
            // - go through the whole list of chips
            // - delete the first object which has multi-select on
            // - if no such is found, we're done, otherwise start over
            int found = 1;
            int guard = 0;
            while (found && guard++ < 100000)
            {
                found = 0;
                i = 0;
                while (i < (signed)gChip.size() && (!gChip[i] || gChip[i]->mMultiSelectState == 0)) i++;
                if (i < (signed)gChip.size())
                {
                    found = 1;
                    delete_chip(gChip[i]);
                }
            }
            
            // Deletion of wires is easier (although it's unlikely that any remain 
            // at this point due to wirefry..)

            // This operation would be more efficient if done from end to the beginning.
            // Simpler code leads to fewer bugs though, so optimize only if needed.
            for (i = 0; i < (signed)gWire.size(); i++)
            {
                if (gWire[i] && gWire[i]->mMultiSelectState)
                {
                    delete gWire[i];
                    gWire.erase(gWire.begin() + i);
                    i--; // go back in indices since the indices have moved.
                }
            }
            // Clean up after work
            gMultiSelectChip.clear();
            gMultiSelectWire.clear();
            gUIState.kbditem = 0;
            gUIState.activeitem = 0;
            gUIState.hotitem = 0;
            build_nets(); // wire removed, rebuild the nets
        }
    }
    else
    {
        // not multi-select..

        if (IS_CHIP_ID(gUIState.kbditem) && GET_PIN_ID(gUIState.kbditem) == 0)
        {
            int kbdChip = GET_CHIP_ID(gUIState.kbditem);
            if (kbdChip < 0 || kbdChip >= (int)gChip.size() || !gChip[kbdChip])
            {
                gUIState.kbditem = 0;
            }
            else
            {
            Chip *c = gChip[kbdChip];
		    switch (gUIState.keyentered)
            {
            case SDLK_LEFT:
            case SDLK_RIGHT:
            case SDLK_UP:
            case SDLK_DOWN:
                if (UiTheme::shouldSaveNudge(tick, sLastNudgeTick))
                    save_undo();
                sLastNudgeTick = tick;
                move_chip(c, gUIState.keyentered);
                break;
            case SDLK_DELETE:
                save_undo();
                delete_chip(c);
				build_nets();
                gUIState.kbditem = 0;
                gUIState.activeitem = 0;
                gUIState.hotitem = 0;
                break;
            }
            }
        }

        if (IS_WIRE_ID(gUIState.kbditem))
        {
            int kbdWire = GET_WIRE_ID(gUIState.kbditem);
            if (kbdWire < 0 || kbdWire >= (int)gWire.size())
            {
                gUIState.kbditem = 0;
            }
            else
            {
		    switch (gUIState.keyentered)
            {
            case SDLK_DELETE:
                save_undo();
                delete gWire[kbdWire];
                gWire.erase(gWire.begin() + kbdWire);
                gUIState.kbditem = 0;
                gUIState.activeitem = 0;
                gUIState.hotitem = 0;
                build_nets(); // wire removed, rebuild the nets
                break;
            }
            }
        }
    }

    if (gMultiselectDirty)
    {
        // Reset and set chips' and wires' multiselect states
        for (i = 0; i < (signed)gChip.size(); i++)
            if (gChip[i]) gChip[i]->mMultiSelectState = 0;
        for (i = 0; i < (signed)gWire.size(); i++)
            if (gWire[i]) gWire[i]->mMultiSelectState = 0;
        for (i = 0; i < (signed)gMultiSelectChip.size(); i++)
            if (gMultiSelectChip[i]) gMultiSelectChip[i]->mMultiSelectState = 1;
        for (i = 0; i < (signed)gMultiSelectWire.size(); i++)
            if (gMultiSelectWire[i]) gMultiSelectWire[i]->mMultiSelectState = 1;
        gMultiselectDirty = 0;
    }

	do_build_nets();

    glEnable(GL_SCISSOR_TEST);
	glScissor(gConfig.mToolkitWidth,0,gScreenWidth-gConfig.mToolkitWidth,gScreenHeight-gTopbarH);

    glPushMatrix();
	glTranslatef((float)gConfig.mToolkitWidth, (float)gTopbarH, 0);
    glScalef(gZoomFactor, gZoomFactor, 1);
    glTranslatef(gWorldOfsX, gWorldOfsY, 0);

    // Draw grid
	if (gBlackBackground)
		glColor4f(0.188f, 0.216f, 0.278f, 1.0f);
	else
		glColor4f(0.812f, 0.831f, 0.871f, 1.0f);
    glBegin(GL_LINES);    
    for (i = 0; i < 20; i++)
    {
            glVertex2f(i * 10, 0);
            glVertex2f(i * 10, 190);
            glVertex2f(0     , i * 10);
            glVertex2f(190   , i * 10);
    }
    glEnd();
    fn.drawstring(TITLE,0.5,0.5,C_ACCENTTEXT,1);
    fn.drawstring(gConfig.mUserInfo,0.5,2.0,C_ACCENTTEXT,1);
    fn.drawstring("http://iki.fi/sol/",0.5,3.2,C_TEXTDIM,0.5);

    if (gZoomFactor > 100)
    {
        fn.drawstring("Congrats, you can zoom.",0.57,2.3,C_ACCENTTEXT,0.05);
        if (gZoomFactor > 1000)
        {
            fn.drawstring("Far enough.",0.63,2.342,C_ACCENTTEXT,0.005);
            if (gZoomFactor > 10000)
            {
                fn.drawstring("Are we there yet?",0.6515,2.346,C_ACCENTTEXT,0.0005);
            }
        }
    }
    

    if (gDragMode == DRAGMODE_SELECT)
    {
        drawrect(
            worldmousedownx,
            worldmousedowny, 
            worldmousex - worldmousedownx, 
            worldmousey - worldmousedowny, 
            0x3fffff00);
    }

	int color_hotitem1 = 0xffffafaf;
	int color_hotitem2 = 0xffffcfcf;
	int color_hotpin1 = 0x7fffffff;
	int color_hotpin2 = 0x7fffffff;
	int color_pinhilight = 0x9fff0000;
	int color_normalpin = 0x3fffffff;
	int color_hotchip = 0x7fffcf00;
	int color_kbdchip = 0x3fffff00;
	int color_multiselect = 0x1fffff00;

	if (!gBlackBackground)
	{
		color_hotitem1 = 0xff7f6f6f;
		color_hotitem2 = 0xff7f5f5f;
		color_hotpin1 = 0x7f7f7f7f;
		color_hotpin2 = 0x7f7f7f7f;
		color_pinhilight = 0x9f7f0000;
		color_normalpin = 0x3f7f7f7f;
		color_hotchip = 0x7f7f5f00;
		color_kbdchip = 0x3f7f7f00;
		color_multiselect = 0x1f7f7f00;
	}

    for (i = 0; i < (signed)gChip.size(); i++)
    {
		// Don't draw items in boxes
		if (!gChip[i] || gChip[i]->mBox != 0)
			continue;
        // Render chips rotated with opengl matrices so that all texture stuff etc. works out automatically.
        glPushMatrix();
        glTranslatef(gChip[i]->mX + gChip[i]->mW / 2, gChip[i]->mY + gChip[i]->mH / 2, 0);
        glRotatef(gChip[i]->mAngleIn90DegreeSteps * 90, 0, 0, 1);
        glTranslatef(-(gChip[i]->mX + gChip[i]->mW / 2), -(gChip[i]->mY + gChip[i]->mH / 2), 0);
        if (gUIState.hotitem == CHIP_ID(0,i))
            drawrect(gChip[i]->mX-0.5,gChip[i]->mY-0.5,gChip[i]->mW+1,gChip[i]->mH+1,color_hotchip);
        else
        if (gUIState.kbditem == CHIP_ID(0,i))
            drawrect(gChip[i]->mX-0.5,gChip[i]->mY-0.5,gChip[i]->mW+1,gChip[i]->mH+1,color_kbdchip);
        else
        if (gChip[i]->mMultiSelectState)
            drawrect(gChip[i]->mX-0.5,gChip[i]->mY-0.5,gChip[i]->mW+1,gChip[i]->mH+1,color_multiselect);
            
        gChip[i]->render(CHIP_ID(0, i));



        int j;
        int tinyPins = (gChip[i]->mRotatedW < 2.0f || gChip[i]->mRotatedH < 2.0f) ? 1 : 0;
        int anchorDraw = (gChipName[i] && stricmp(gChipName[i], "Connection Pin") == 0) ? 1 : 0;
        float pinPad = tinyPins ? 0.0f : UiTheme::pinGrabPad(gZoomFactor);
        for (j = 0; j < (signed)gChip[i]->mPin.size(); j++)
        {
            if (!gChip[i]->mPin[j])
                continue;
            if (anchorDraw)
                continue; // anchor chip draws its own hover zones
            if (gUIState.hotitem == CHIP_ID(j+1,i))
            {
                drawrect(gChip[i]->mX + gChip[i]->mPin[j]->mX - pinPad,
                         gChip[i]->mY + gChip[i]->mPin[j]->mY - pinPad,
                         0.5f + 2 * pinPad, 0.5f + 2 * pinPad, color_hotitem1);
                drawrect(gChip[i]->mX + gChip[i]->mPin[j]->mX + 0.1,
                         gChip[i]->mY + gChip[i]->mPin[j]->mY + 0.1, 0.3, 0.3, color_hotitem2);
            }
            else
            if (IS_CHIP_ID(gUIState.hotitem) && GET_CHIP_ID(gUIState.hotitem) == i)
            {
                drawrect(gChip[i]->mX + gChip[i]->mPin[j]->mX, 
                         gChip[i]->mY + gChip[i]->mPin[j]->mY, 0.5, 0.5, color_hotpin1);
                drawrect(gChip[i]->mX + gChip[i]->mPin[j]->mX + 0.1, 
                         gChip[i]->mY + gChip[i]->mPin[j]->mY + 0.1, 0.3, 0.3, color_hotpin2);
            }
            else
            {
                if (gUIState.keymod & KMOD_ALT && gChip[i]->mPin[j]->getState() == PINSTATE_READ && gChip[i]->mPin[j]->mNet == NULL)
                {
                    drawrect(gChip[i]->mX + gChip[i]->mPin[j]->mX + 0.1, 
                            gChip[i]->mY + gChip[i]->mPin[j]->mY + 0.1, 0.3, 0.3, color_pinhilight);
                }
                else
                {
                    drawrect(gChip[i]->mX + gChip[i]->mPin[j]->mX + 0.1, 
                            gChip[i]->mY + gChip[i]->mPin[j]->mY + 0.1, 0.3, 0.3, color_normalpin);
                }
            }
        }
        glPopMatrix();
    }

    if (gConfig.mAntialiasedLines)
    {
        glEnable(GL_LINE_SMOOTH);
        glLineWidth(0.075 * gZoomFactor);
    }

    glBegin(GL_LINES);
    for (i = 0; i < (signed)gWire.size(); i++)
    {
		// Don't draw items in boxes
		if (!gWire[i] || gWire[i]->mBox != 0)
			continue;
        if (!gWire[i]->mFirst || !gWire[i]->mSecond)
            continue;
        if (!gWire[i]->mFirst->mHost || !gWire[i]->mSecond->mHost)
            continue;

		float rc, bc, gc;
        int netState = NETSTATE_NC;
        if (gWire[i]->mFirst->mNet)
            netState = gWire[i]->mFirst->mNet->mState;
        switch(netState)
        {
        case NETSTATE_NC:
            rc = 0.5; gc = 0.5; bc = 0.5;
            break;
        case NETSTATE_HIGH:
            rc = 0; gc = 1; bc = 0;
            break;
        case NETSTATE_LOW:
            rc = 0; gc = 0.5; bc = 0;
            break;
        default:
        //case NETSTATE_INVALID:
            rc = 0.75; gc = 0; bc = 0;
            break;
        }
      
        if (gUIState.kbditem == WIRE_ID(i))
        {
            rc = (rc + 1) / 2;
            gc = (gc + 1) / 2;
            bc = (bc + 0.5) / 2;
        }
        else        
        if (gWire[i]->mMultiSelectState)
        {
            rc = (rc + 0.75) / 2;
            gc = (gc + 0.75) / 2;
            bc = (bc + 0.25) / 2;
        }

		if (!gBlackBackground)
		{
			rc *= 0.75;
			gc *= 0.75;
			bc *= 0.75;
		}

        if (gLiveWires)
        {
            glColor4f(rc, gc, bc, 1);
        }
        else
        {
			if (gBlackBackground)
				glColor4f(0.75f,0.75f,0.75f,1.0f);
			else
				glColor4f(0,0,0,1.0f);
        }

        Pin * a, * b;
        a = gWire[i]->mFirst;
        b = gWire[i]->mSecond;

        int hotWire = IS_WIRE_ID(gUIState.hotitem) ? GET_WIRE_ID(gUIState.hotitem) : -1;
        if (hotWire >= 0 && hotWire < (int)gWire.size() && gWire[hotWire] && gWire[hotWire]->mFirst &&
            gWire[hotWire]->mFirst->mNet == a->mNet && a->mNet)
        {
            glColor4f(1,1,0,0.5);
        }

        if (gUIState.hotitem == WIRE_ID(i))
        {
            glColor4f(1,1,1,0.5);
            if (split_wire(0))
            {
                float xv = (a->mHost->mRotatedX + a->mRotatedX + 0.25) - (b->mHost->mRotatedX + b->mRotatedX + 0.25);
                float yv = (a->mHost->mRotatedY + a->mRotatedY + 0.25) - (b->mHost->mRotatedY + b->mRotatedY + 0.25);
                
                float l = sqrt(xv*xv+yv*yv);
                if (l!=0)
                {
                    xv /= l;
                    yv /= l;
                }
                else
                {
                    xv = yv = 0;
                }

                glVertex2f(worldmousex-(yv*16)/gZoomFactor,
                           worldmousey+(xv*16)/gZoomFactor);
                glVertex2f(worldmousex+(yv*16)/gZoomFactor,
                           worldmousey-(xv*16)/gZoomFactor);

                mousemode = 2;

                // Preview marker: cross exactly where a click would drop
                // the bend point (snapped like the real placement).
                float mx = UiTheme::snapWorld(worldmousex, gSnap);
                float my = UiTheme::snapWorld(worldmousey, gSnap);
                float ms = 10.0f / gZoomFactor;
                glVertex2f(mx - ms, my); glVertex2f(mx + ms, my);
                glVertex2f(mx, my - ms); glVertex2f(mx, my + ms);
            }
            glColor4f(1,1,0,1);
        }

        glVertex2f(a->mHost->mRotatedX + a->mRotatedX + 0.25,
                   a->mHost->mRotatedY + a->mRotatedY + 0.25);
        glVertex2f(b->mHost->mRotatedX + b->mRotatedX + 0.25,
                   b->mHost->mRotatedY + b->mRotatedY + 0.25);
    }


    if (gDragMode == DRAGMODE_WIRE)
    {
        if (gWireStartDrag && gWireStartDrag->mHost)
        {
        glColor4f(0.5,1,0.5,1);
        glVertex2f(worldmousex, worldmousey);
        glVertex2f(gWireStartDrag->mHost->mRotatedX + gWireStartDrag->mRotatedX + 0.25f,
                   gWireStartDrag->mHost->mRotatedY + gWireStartDrag->mRotatedY + 0.25f);
        }
    }
    glEnd();

    if (gConfig.mAntialiasedLines)
    {
        glLineWidth(1);
        glDisable(GL_LINE_SMOOTH);
    }

    if (gDragMode == DRAGMODE_NEWCHIP && gUIState.mousex > gConfig.mToolkitWidth && gUIState.mousey > gTopbarH && gUIState.mousedown)
    {
        if (gNewChip && gNewChipName)
        {
        save_undo();
        gChip.push_back(gNewChip);
        gChipName.push_back(gNewChipName);
        gDragMode = DRAGMODE_NONE;
        gNewChip->mX = worldmousex - gNewChip->mW / 2;
        gNewChip->mY = worldmousey - gNewChip->mH / 2;
        gUIState.mousedownx = (float)gUIState.mousex;
        gUIState.mousedowny = (float)gUIState.mousey;
        gUIState.activeitem = CHIP_ID(0, (int)gChip.size() - 1);
        gUIState.kbditem = gUIState.activeitem;
        gNewChip = NULL;
        }
        else
        {
            gDragMode = DRAGMODE_NONE;
            gNewChip = NULL;
        }
    }

    glPopMatrix();
    glScissor(0,0,gScreenWidth,gScreenHeight);

    // Tooltips
    if (gUIState.activeitem == 0 && gUIState.hotitem != 0 && (tick - gUIState.lasthottick) > gConfig.mTooltipDelay)
    {
        const char *tooltip = NULL;

        if (loc != -1 && gVisibleChiplist >= 0 && gVisibleChiplist <= 4 && loc >= 0 && loc < (int)gAvailableChip[gVisibleChiplist].size() && !(gVisibleChiplist == 2 && loc == 8))
        {
            if ((loc | (gVisibleChiplist << 16)) != gSidebarTooltipId)
            {
                gSidebarTooltipId = loc | (gVisibleChiplist << 16);
                // Now, *this* is quite wasteful.
                Chip * nChip = NULL;
                int j;
                const char *want = gAvailableChip[gVisibleChiplist][loc];
                if (want)
                {
                for (j = 0; nChip == NULL && j < (signed)gChipFactory.size(); j++)
                    nChip = gChipFactory[j]->build(want);
                }
                if (nChip)
                {
                    delete[] gSidebarTooltip;
                    if (nChip->mTooltip)
                        gSidebarTooltip = mystrdup(nChip->mTooltip);
                    else
                        gSidebarTooltip = mystrdup(gAvailableChip[gVisibleChiplist][loc]);
                    delete nChip;
                    nChip = NULL;
                    tooltip = gSidebarTooltip;
                }
                else
                {
                    tooltip = NULL;
                }                
            }
            else
            {
                tooltip = gSidebarTooltip;
            }
        }

        if (IS_CHIP_ID(gUIState.hotitem))
        {
            int hc = GET_CHIP_ID(gUIState.hotitem);
            int hp = GET_PIN_ID(gUIState.hotitem);
            if (hc >= 0 && hc < (int)gChip.size() && gChip[hc])
            {
            if (hp == 0)
            {
                if (gChip[hc]->mTooltip == NULL)
                {
                    tooltip = gChipName[hc];
                }
                else
                {
                    tooltip = gChip[hc]->mTooltip;
                }
            }
            else
            {
                if (hp - 1 < (int)gChip[hc]->mPin.size() && gChip[hc]->mPin[hp-1])
                    tooltip = gChip[hc]->mPin[hp-1]->mTooltip;
            }
            }
        }
        if (IS_WIRE_ID(gUIState.hotitem))
        {
            int wid = GET_WIRE_ID(gUIState.hotitem);
            Wire *w = (wid >= 0 && wid < (int)gWire.size()) ? gWire[wid] : NULL;
            int st = NETSTATE_NC;
            if (w && w->mFirst && w->mFirst->mNet)
                st = w->mFirst->mNet->mState;
            switch (st)
            {
            case NETSTATE_NC:
                tooltip = "Not connected:\nNet not connected\nto an input";
                break;
            case NETSTATE_INVALID:
                tooltip = "Invalid state:\nTwo or more outputs\nconnected together\nor invalid wiring\non a chip.";
                break;
            case NETSTATE_HIGH:
                tooltip = "Signal 'High'";
                break;
            case NETSTATE_LOW:
                tooltip = "Signal 'Low'";
                break;
            }
        }
        if (tooltip)
        {
            float w, h, llw;
            w = h = llw = 0;
            fn14.stringmetrics(tooltip, w, h, llw);
            if (w > 0 && h > 0)
            {
            float tx = (float)(gUIState.mousex + 16);
            float ty = (float)(gUIState.mousey + 16);
            if (tx + w + 8 > gScreenWidth) tx = (float)(gScreenWidth - w - 10);
            if (ty + h + 8 > gScreenHeight) ty = (float)(gScreenHeight - h - 10);
            if (tx < 0) tx = 0;
            if (ty < 0) ty = 0;
            drawrect(tx, ty, w+8, h+8, C_WIDGETBG);
            drawrect(tx, ty, w+8, 1, C_MENULINE);
            fn14.drawstring(tooltip, tx + 4, ty + 4, C_TEXT);
            }
        }
    }

    // Status bar: live counts, zoom, mode flags. Screen-space chrome.
    {
        const float barH = 24.0f;
        float barY = (float)(gScreenHeight - barH);
        drawrect(0, barY, (float)gScreenWidth, 1, C_MENULINE);
        drawrect(0, barY + 1, (float)gScreenWidth, barH - 1, C_MENUBG);
        char status[256];
        snprintf(status, sizeof(status), "Chips:%d  Wires:%d  Nets:%d   Zoom:%.0f   %s   %s   Undo:%d Redo:%d",
            (int)gChip.size(), (int)gWire.size(), (int)gNet.size(),
            gZoomFactor,
            gSnap ? "Snap:on" : "Snap:off",
            gLiveWires ? "Live" : "Grey",
            (int)gUndoStack.size(), (int)gRedoStack.size());
        fn14.drawstring(status, (float)(gConfig.mToolkitWidth + 10), barY + 6, C_TEXTDIM);
    }

    imgui_finish();

    if (gConfig.mPerformanceIndicators)
    {
        static int frame = 0; frame++;
        #define PERF_FRAMES 50
        static int perf_idx = 0;
        static int perf_data1[PERF_FRAMES];
        static int perf_data2[PERF_FRAMES];
        perf_data1[perf_idx] = tick;
        perf_data2[perf_idx] = physics_iterations;
        char perf_temp[200];
        float fps = 1000.0f / ((float)(perf_data1[perf_idx] - perf_data1[(perf_idx+1) % PERF_FRAMES]) / PERF_FRAMES);
        float mspf = (float)(perf_data1[perf_idx] - perf_data1[(perf_idx+1) % PERF_FRAMES]) / PERF_FRAMES;
        float ppf = (float)(perf_data2[perf_idx] - perf_data2[(perf_idx+1) % PERF_FRAMES]) / PERF_FRAMES;
		sprintf(perf_temp, "%3.0f%%rt, %3.0ffps, %3.0fmspf, %dms phys", ppf*fps / 10.0f / gConfig.mPhysicsKHz, fps, mspf, physms);
        fn.drawstring(perf_temp, gConfig.mToolkitWidth+10, gScreenHeight - 40,0x3fffffff,32);
        sprintf(perf_temp, "font1:%d/%d, font2:%d/%d",fn.mFontCacheTrashed,fn.mFontCacheUse,fn14.mFontCacheTrashed,fn14.mFontCacheUse);
        fn.drawstring(perf_temp, gConfig.mToolkitWidth+10, gScreenHeight - 72,0x3fffffff,32);
		sprintf(perf_temp, "Chips:%d Wires:%d Nets:%d",(int)gChip.size(), (int)gWire.size(), (int)gNet.size());
        fn.drawstring(perf_temp, gConfig.mToolkitWidth+10, gScreenHeight - 104,0x3fffffff,32);
        perf_idx++;
        perf_idx %= PERF_FRAMES;
	}
    fn.fontcacheframe();
    fn14.fontcacheframe();

    if (gDragMode == DRAGMODE_NEWCHIP)
    {
        if (gNewChip)
        {
        int w = (int)(gNewChip->mW * gZoomFactor);
        int h = (int)(gNewChip->mH * gZoomFactor);
        drawrect((float)(gUIState.mousex - w/2), (float)(gUIState.mousey - h/2), (float)w, (float)h, 0x7fffffff);
        }
    }

    if (gConfig.mCustomCursors && mousemode != lastmousemode)
    {
        lastmousemode = mousemode;
        SDL_Cursor *want = cursor_normal;
        switch (mousemode)
        {
        case 1:
            want = cursor_drag ? cursor_drag : cursor_normal;
            break;
        case 2:
            want = cursor_scissors ? cursor_scissors : cursor_normal;
            break;
        default:
            want = cursor_normal;
            break;
        }
        if (want)
            SDL_SetCursor(want);
    }

    SDL_Delay(10);
    glFinish();
    if (gMainWindow)
        SDL_GL_SwapWindow(gMainWindow);
}

void initvideo()
{
    if (gMainWindow == NULL)
    {
        gMainWindow = SDL_CreateWindow("Atanua",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            gScreenWidth, gScreenHeight,
            SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
        if (!gMainWindow)
        {
            fprintf(stderr, "Video mode set failed: %s\n", SDL_GetError());
            SDL_Quit();
            exit(0);
        }
        gGLContext = SDL_GL_CreateContext(gMainWindow);
        if (!gGLContext)
        {
            fprintf(stderr, "GL context creation failed: %s\n", SDL_GetError());
            SDL_Quit();
            exit(0);
        }
    }
    else
    {
        int curW = 0, curH = 0;
        SDL_GetWindowSize(gMainWindow, &curW, &curH);
        if (curW != gScreenWidth || curH != gScreenHeight)
            SDL_SetWindowSize(gMainWindow, gScreenWidth, gScreenHeight);
    }

    if (gScreenWidth < 100) gScreenWidth = 100;
    if (gScreenHeight < 100) gScreenHeight = 100;
    glViewport(0, 0, gScreenWidth, gScreenHeight);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glOrtho(0, gScreenWidth, gScreenHeight, 0, -1, 1);

    if (gConfig.mUseBlending)
        glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    reload_textures();
}

void audiomixer(void *userdata, Uint8 *stream, int len)
{
    static int filter = 0;
    int n = len / 2;
    short * buf = (short*)stream;
    memset(stream,0,len);
    float step = 1000.0f / gAudioSpec->freq;
    int i;
    for (i = 0; i < n; i++)
    {
        int p = (int)floor(gPlayHead);
        if (p < 0) p = 0;
        if (p >= AUDIOBUF_SIZE) p = AUDIOBUF_SIZE - 1;
        int d = (gAudioBuffer[p] - 127) * 250;
        filter = (d + filter * 15) / 16;
        buf[i] = filter;
        gPlayHead += step;
        if (gPlayHead >= AUDIOBUF_SIZE)
            gPlayHead -= AUDIOBUF_SIZE;
    }
}


int main(int argc, char** args)
{
    memset(gAudioBuffer,0,AUDIOBUF_SIZE);

    gotoappdirectory(argc, args);

    gConfig.load();

    if (gConfig.mSwapShiftAndCtrl)
    {
        gSelectKeyMask = KMOD_CTRL;
        gCloneKeyMask = KMOD_SHIFT;
    }
    else
    {
        gSelectKeyMask = KMOD_SHIFT;
        gCloneKeyMask = KMOD_CTRL;
    }


    memset(gKeyState, 0, sizeof(gKeyState));

    gVisualRand.init_genrand(0xc0cac01a);

    int sdlflags = SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS;
    
    if (gConfig.mAudioEnable)
        sdlflags |= SDL_INIT_AUDIO;

    if (SDL_Init(sdlflags) < 0) 
    {
        fprintf(stderr, "Video initialization failed: %s\n", SDL_GetError());
        SDL_Quit();
        exit(0);
    }

    if (gConfig.mAudioEnable)
    {
        SDL_AudioSpec *as = new SDL_AudioSpec;
        SDL_zero(*as);
        as->freq = 44100;
        as->format = AUDIO_S16SYS;
        as->channels = 1;
        as->samples = 4096;
        as->callback = audiomixer;
        as->userdata = NULL;
        if (SDL_OpenAudio(as, NULL) < 0)
        {
            fprintf(stderr, "Unable to init SDL audio: %s\n", SDL_GetError());
            delete as;
        }
        else
        {
            gAudioSpec = as;
        }
        // audio is now started only when the audio device is created, to avoid popping sounds..
        //SDL_PauseAudio(0);
    }

    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 5);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    gScreenWidth = gConfig.mWindowWidth;
    gScreenHeight = gConfig.mWindowHeight;
    UiTheme::clampWindowSize(gScreenWidth, gScreenHeight);

    initvideo();

    {
        char temp[256];
        sprintf(temp, "%s - %s", TITLE, gConfig.mUserInfo);
        if (gMainWindow)
            SDL_SetWindowTitle(gMainWindow, temp);
    }

    SDL_StartTextInput();

    fn.load("data/vera31.fnt");
    fn14.load("data/vera14.fnt");
    if (fn.pages.pages == 0 || fn14.pages.pages == 0)
        fprintf(stderr, "Warning: font assets missing in data/; text may not render.\n");

#ifndef __APPLE__ // Use the higher-resolution OSX icon instead
	int x, y, n;
	unsigned char *data = stbi_load("data/icon.png", &x, &y, &n, 4);
    if (data)
    {
        SDL_Surface *icon = SDL_CreateRGBSurfaceFrom(data, x, y, 32, x * 4, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
        if (icon && gMainWindow)
            SDL_SetWindowIcon(gMainWindow, icon);
        if (icon)
            SDL_FreeSurface(icon);
        stbi_image_free(data);
    }
#endif

    SDL_ShowCursor(1);

    if (gConfig.mCustomCursors)
    {
        cursor_drag = load_cursor("data/cursor_drag.png", 10, 0);
        cursor_scissors = load_cursor("data/cursor_scissors.png", 6, 6);
        cursor_normal = load_cursor("data/cursor_ptr.png", 0, 0);
        if (cursor_normal)
            SDL_SetCursor(cursor_normal);
    }

    gChipFactory.push_back(new BaseChipFactory);
    gChipFactory.push_back(new PluginChipFactory);


    int i;
    for (i = 0; i < (signed)gChipFactory.size(); i++)
        gChipFactory[i]->getSupportedChips(gAvailableChip);

    if (argc > 1)
        do_loaddialog(0, args[1]);

    AppUpdate_StartCheck();

    while (1) 
    {
        process_events();
        draw_screen();
    }

    return 0;
}
