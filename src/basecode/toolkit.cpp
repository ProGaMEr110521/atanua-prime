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
#include <math.h>
#include "toolkit.h"

#ifdef USE_BASIC_POPUPS
Popup popup[MAX_POPUPS];
#endif

#ifdef USE_BASIC_PARTICLES
Particle particle[MAX_PARTICLES];
#endif

struct texpair
{
    const char *mFilename;
    GLuint mHandle;
    int mClamp;
};

int gScreenWidth = 0;
int gScreenHeight = 0;
MersenneTwister gVisualRand, gPhysicsRand;
UIState gUIState = {0,0,0,0,0,0,0,0,0,0};
texpair * gTextureStore = NULL;
int gTextureStoreSize = 0;


static void render_perfcounters(int tex_font, ACFont * font)
{
	int tick = SDL_GetTicks();
#define PERF_FRAMES 50
    drawrect(8,gScreenHeight - 34 - 40,210,40 + 20,0x3f0000ff);

    drawrect(8,gScreenHeight - 34 - 40,210,1,0x3f0000ff);
    drawrect(8,gScreenHeight - 34 - 40,1,40 + 20,0x3f0000ff);
    drawrect(8,gScreenHeight - 34 + 19,210,1,0x3f0000ff);
    drawrect(8 + 209,gScreenHeight - 34 - 40,1,40 + 20,0x3f0000ff);

    static int perf_idx = 0;
    static int perf_data[PERF_FRAMES];
    perf_data[perf_idx] = tick;
    char perf_temp[200];
    sprintf(perf_temp, "%3.3f mspf (%3.3f fps)", (float)(perf_data[perf_idx] - perf_data[(perf_idx+1) % PERF_FRAMES]) / PERF_FRAMES,
                                                 1000.0f / ((float)(perf_data[perf_idx] - perf_data[(perf_idx+1) % PERF_FRAMES]) / PERF_FRAMES));
    perf_idx++;
    perf_idx %= PERF_FRAMES;
    if (tex_font) 
	{
		quickfont_drawstring(tex_font, perf_temp, 16, gScreenHeight - 32, 0xffffff, 1, 1);
	}
	else
	{
		if (font)
		{
			font->drawstring(perf_temp, 16, gScreenHeight - 32, 0xffffffff, 16);
		}
	}

    int perf_i;
    for (perf_i = 0; perf_i < PERF_FRAMES - 1; perf_i++)
    {
        float h = (perf_data[(perf_idx + perf_i + 1) % PERF_FRAMES] - perf_data[(perf_idx + perf_i) % PERF_FRAMES]);
        drawrect(perf_i * 4 + 16, gScreenHeight - 34 - h, 3, h, 0x7fff0000);
    }
}

void render_perfcounters(int tex_font)
{
	render_perfcounters(tex_font, 0);
}

void render_perfcounters(ACFont *font)
{
	render_perfcounters(0, font);
}

void render_perfcounters(ACFont &font)
{
	render_perfcounters(0, &font);
}

void set2d()
{
	glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
#ifdef DESIRED_ASPECT
	glOrtho(0, DESIRED_WINDOW_WIDTH, DESIRED_WINDOW_HEIGHT, 0, -1, 1);
#else
    glOrtho(0, gScreenWidth, gScreenHeight, 0, -1, 1);
#endif

	glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}


void initvideo(int argc)
{
    (void)argc;
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 5);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

#ifdef DESIRED_ASPECT
    float aspect = DESIRED_ASPECT;
    if (((float)gScreenWidth / gScreenHeight) > aspect)
    {
        float realx = gScreenHeight * aspect;
        float extrax = gScreenWidth - realx;

        glViewport((GLint)(extrax / 2), 0, (GLsizei)realx, gScreenHeight);
    }
    else
    {
        float realy = gScreenWidth / aspect;
        float extray = gScreenHeight - realy;

        glViewport(0, (GLint)(extray / 2), gScreenWidth, (GLsizei)realy);
    }
#else
    glViewport(0, 0, gScreenWidth, gScreenHeight);
#endif

	set2d();

    reload_textures();
}


static void do_loadtexture(const char * aFilename, int clamp = 1)
{
    int i, j;

    // Load texture using stb
	int x, y, n;
	unsigned char *data = stbi_load(aFilename, &x, &y, &n, 4);
    
    if (data == NULL)
        return;

    // mark all pixels with alpha = 0 to black
    unsigned int * src = (unsigned int*)data;
    for (i = 0; i < x * y; i++)
    {
        if ((src[i] & 0xff000000) == 0)
            src[i] = 0;
    }

    // Tell OpenGL to read the texture
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, x, y, 0, GL_RGBA, GL_UNSIGNED_BYTE, (GLvoid*)data);

    // Mip chain: each level is a 2x2 box filter of the previous one, with
    // color weighted by alpha so transparent texels (black after the pass
    // above) do not darken glyph and line edges. The old summed-area
    // version sampled one texel per block (an off-by-one), which made
    // zoomed-out text and sprites shimmer and look pixelated.
    {
        int w = x, h = y, l = 0;
        unsigned char *prev = data;
        unsigned char *owned = NULL;
        while (w > 1 || h > 1)
        {
            int nw = w > 1 ? w / 2 : 1;
            int nh = h > 1 ? h / 2 : 1;
            unsigned char *next = new unsigned char[nw * nh * 4];
            for (j = 0; j < nh; j++)
            {
                for (i = 0; i < nw; i++)
                {
                    unsigned int r = 0, g = 0, b = 0, a = 0;
                    for (int dy = 0; dy < 2; dy++)
                    {
                        int sy = j * 2 + dy;
                        if (sy >= h) sy = h - 1;
                        for (int dx = 0; dx < 2; dx++)
                        {
                            int sx = i * 2 + dx;
                            if (sx >= w) sx = w - 1;
                            const unsigned char *px = prev + (sy * w + sx) * 4;
                            r += px[0] * px[3];
                            g += px[1] * px[3];
                            b += px[2] * px[3];
                            a += px[3];
                        }
                    }
                    unsigned char *o = next + (j * nw + i) * 4;
                    if (a == 0)
                    {
                        o[0] = o[1] = o[2] = o[3] = 0;
                    }
                    else
                    {
                        o[0] = (unsigned char)(r / a);
                        o[1] = (unsigned char)(g / a);
                        o[2] = (unsigned char)(b / a);
                        o[3] = (unsigned char)((a + 2) / 4);
                    }
                }
            }
            l++;
            glTexImage2D(GL_TEXTURE_2D, l, GL_RGBA, nw, nh, 0, GL_RGBA, GL_UNSIGNED_BYTE, (GLvoid*)next);
            delete[] owned;
            owned = next;
            prev = next;
            w = nw;
            h = nh;
        }
        delete[] owned;
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR); // Linear Filtering
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR); // Linear Filtering
    }

    // and cleanup.
	stbi_image_free(data);

    if (clamp)
    {
        // Set up texture parameters
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    }
    else
    {
        // Set up texture parameters
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }
}

char * mystrdup(const char *aString)
{
	if (!aString)
		return NULL;
	size_t srclen = strlen(aString);
	if (srclen > 1024 * 1024)
		srclen = 1024 * 1024;
	int len = (int)srclen;
	char * d = new char[len+1];
	memcpy(d, aString, len);
	d[len] = 0;
	return d;
}

GLuint load_texture(const char * aFilename, int clamp)
{
    // First check if we have loaded this texture already
    int i;
    for (i = 0; i < gTextureStoreSize; i++)
    {
        if (stricmp(gTextureStore[i].mFilename, aFilename) == 0)
            return gTextureStore[i].mHandle;
    }

    // Create OpenGL texture handle and bind it to use

    GLuint texname;
    glGenTextures(1,&texname);
    glBindTexture(GL_TEXTURE_2D,texname);

    do_loadtexture(aFilename, clamp);

    gTextureStoreSize++;

	texpair * t = (texpair *)realloc(gTextureStore, sizeof(texpair) * gTextureStoreSize);
	if (t != NULL)
	{
	    gTextureStore = t;
		gTextureStore[gTextureStoreSize-1].mFilename = mystrdup(aFilename);
		gTextureStore[gTextureStoreSize-1].mHandle = texname;
		gTextureStore[gTextureStoreSize-1].mClamp = clamp;
	}

    return texname;
}

void reload_textures()
{
    // bind the textures to the same texture names as the last time.
    int i;
    for (i = 0; i < gTextureStoreSize; i++)
    {
        glBindTexture(GL_TEXTURE_2D, gTextureStore[i].mHandle);
        do_loadtexture(gTextureStore[i].mFilename, gTextureStore[i].mClamp);
    }
}

// Convert image to SDL cursor, assumes max. 32x32 cursors
SDL_Cursor *load_cursor(const char *aFilename, int hotx, int hoty) 
{
    int             bytewidth, x, y;
    Uint8           data[32*32*2], *mask, *d, *m;
    SDL_Cursor      *cursor;
	int ix = 0, iy = 0, n = 0;
	unsigned char *imgdata = stbi_load(aFilename, &ix, &iy, &n, 4);

    if (imgdata == NULL)
        return NULL;

    if (ix > 32 || iy > 32)
    {
        stbi_image_free(imgdata);
        return NULL;
    }
    
    bytewidth = (ix + 7) / 8;
    memset(data, 0, bytewidth * iy * 2);
    mask = data + bytewidth * iy;
  
    for (y = 0; y < iy; y++) 
    {
        d = data + y * bytewidth;
        m = mask + y * bytewidth;
        for (x = 0; x < ix; x++) 
        {
            unsigned int color = ((unsigned int *)imgdata)[y * ix + x];
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
            if (color & 0x80000000)
                m[x / 8] |= 0x80 >> (x & 7);
            if (!(color & 0x80))
                d[x / 8] |= 0x80 >> (x & 7);
#else
            if (color & 0x80)
                m[x / 8] |= 0x80 >> (x & 7);
            if (!(color & 0x80000000))
                d[x / 8] |= 0x80 >> (x & 7);
#endif
        }
    }

    cursor = SDL_CreateCursor(data, mask, ix, iy, hotx, hoty);
    stbi_image_free(imgdata);
    return cursor;
}


int rect_rect_collide(float x0a, float y0a, float x1a, float y1a, 
                      float x0b, float y0b, float x1b, float y1b)
{
    // Make sure rect a is properly shaped
    if (x0a > x1a)
    {
        float temp = x0a;
        x0a = x1a;
        x1a = temp;
    }
    if (y0a > y1a)
    {
        float temp = y0a;
        y0a = y1a;
        y1a = temp;
    }

    if (x0b > x1a && x1b > x1a) return 0; // totally to the right of the rect
    if (x0b < x0a && x1b < x0a) return 0; // totally to the left of the rect
    if (y0b > y1a && y1b > y1a) return 0; // totally below of the rect
    if (y0b < y0a && y1b < y0a) return 0; // totally above of the rect
    // otherwise, we collide
    return 1;
}

// adapted from 'realtime collision detection', p.183
int rect_line_collide(float x0a, float y0a, float x1a, float y1a, 
                      float x0b, float y0b, float x1b, float y1b)
{
    // Make sure rect a is properly shaped
    if (x0a > x1a)
    {
        float temp = x0a;
        x0a = x1a;
        x1a = temp;
    }
    if (y0a > y1a)
    {
        float temp = y0a;
        y0a = y1a;
        y1a = temp;
    }

    float cx = (x0a + x1a) / 2; // rect center
    float cy = (y0a + y1a) / 2;
    float ex = x1a - cx; // halflength extents
    float ey = y1a - cy;
    float mx = (x0b + x1b) / 2; // segment halflength
    float my = (y0b + y1b) / 2;
    float dx = x1b - mx; // segment halflength vec
    float dy = y1b - my;
    mx -= cx; // translate box and segment to origin
    my -= cy; 

    // try world coordinate axes as separating axes
    float adx = fabs(dx);
    if (fabs(mx) > ex + adx) return 0;
    float ady = fabs(dy);
    if (fabs(my) > ey + ady) return 0;

    // try cross-product of segment direction vector with coordinate axis
    if (fabs(mx * dy - my * dx) > ex * ady + ey * adx) return 0;

    return 1;
}


void drawrect(float x, float y, float w, float h, int color)
{
    glColor4f(((color >> 16) & 0xff) / 256.0f,
              ((color >> 8) & 0xff) / 256.0f,
              ((color >> 0) & 0xff) / 256.0f,
              ((color >> 24) & 0xff) / 256.0f);
    glBegin(GL_TRIANGLE_STRIP);
      glVertex2f(x,y);
      glVertex2f(x,y+h);
      glVertex2f(x+w,y);
      glVertex2f(x+w,y+h);
    glEnd();
}

void drawtexturedrect(int tex, float x, float y, float w, float h, int color)
{
    glBindTexture(GL_TEXTURE_2D, tex);
    glEnable(GL_TEXTURE_2D);
    glColor4f(((color >> 16) & 0xff) / 256.0f,
              ((color >> 8) & 0xff) / 256.0f,
              ((color >> 0) & 0xff) / 256.0f,
              ((color >> 24) & 0xff) / 256.0f);
    glBegin(GL_TRIANGLE_STRIP);
      glTexCoord2f(0,0);
      glVertex2f(x,y);
      glTexCoord2f(0,1);
      glVertex2f(x,y+h);
      glTexCoord2f(1,0);
      glVertex2f(x+w,y);
      glTexCoord2f(1,1);
      glVertex2f(x+w,y+h);
    glEnd();
    glDisable(GL_TEXTURE_2D);
}

void quickfont_drawchar(int ch, float x, float y, float w, float h)
{
    // font = 6 rows, 16 chars per row
    float uofs = (ch & 15) * (1.0f / 16.0f) + 0.002f;
    float vofs = ((ch - 32) / 16) * (1.0f / 6.0f) + 0.002f;
    float uwid = (1.0f / 16.0f) - 0.004f;
    float vwid = (1.0f / 6.0f) - 0.004f;
    glBegin(GL_TRIANGLE_STRIP);
      glTexCoord2f(uofs, vofs);
      glVertex2f(x, y);
      glTexCoord2f(uofs, vofs + vwid);
      glVertex2f(x, y + h);
      glTexCoord2f(uofs + uwid, vofs);
      glVertex2f(x + w, y);
      glTexCoord2f(uofs + uwid, vofs + vwid);
      glVertex2f(x + w, y + h);
    glEnd();
}

void quickfont_drawstring(int tex, char * string, float x, float y, int color, float alpha, float size)
{
    glBindTexture(GL_TEXTURE_2D, tex);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(((color >> 16) & 0xff) / 256.0f,
              ((color >> 8) & 0xff) / 256.0f,
              ((color >> 0) & 0xff) / 256.0f,
              alpha);
    while (*string)
    {
        quickfont_drawchar(*string, x, y, 8 * size, 16 * size);
        string++;
        x += 8 * size;
    }
    glDisable(GL_TEXTURE_2D);
}

void drawsprite(int tex, float x, float y, float w, float h, float alpha)
{
    glBindTexture(GL_TEXTURE_2D, tex);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE);
    glColor4f(1.0f,1.0f,1.0f,alpha);
    glBegin(GL_TRIANGLE_STRIP);
      glTexCoord2f(0,0);
      glVertex2f(x - w / 2,y - h / 2);
      glTexCoord2f(0,1);
      glVertex2f(x - w / 2,y + h / 2);
      glTexCoord2f(1,0);
      glVertex2f(x + w / 2,y - h / 2);
      glTexCoord2f(1,1);
      glVertex2f(x + w / 2,y + h / 2);
    glEnd();
    glDisable(GL_TEXTURE_2D);
}

#ifdef USE_BASIC_POPUPS
void spawn_popup(char * text, float x, float y)
{
    int i = 0;
    while (i < MAX_POPUPS && popup[i].live) i++;
    if (i >= MAX_POPUPS) return;
    popup[i].live = 100;
    popup[i].text = mystrdup(text); 
    popup[i].y = y;
    popup[i].x = x - strlen(text) * 4;
}

void physics_popups()
{
    int j;
    for (j = 0; j < MAX_POPUPS; j++)
    {
        if (popup[j].live)
        {
            popup[j].live--;
            popup[j].y -= 0.05f;
            if (!popup[j].live)
            {
                delete[] popup[j].text;
                popup[j].text = 0;
            }
        }
    }
}

void reset_popups()
{
    int i;
    for (i = 0; i < MAX_POPUPS; i++)
    {
        popup[i].live = 0;
        delete[] popup[i].text;
        popup[i].text = 0;
    }
}
#endif

#ifdef USE_BASIC_PARTICLES
void spawn_particle(float x, float y, int tex)
{
    int i = gVisualRand.genrand_int31() % MAX_PARTICLES;
    if (particle[i].live)
        return;
    particle[i].live = 100 + gVisualRand.genrand_int31() % 100;
    particle[i].x = x;
    particle[i].y = y;
    float r = (gVisualRand.genrand_int31() % 1000) / 1000.0f;
    float d = (gVisualRand.genrand_int31() % 1000) / 1000.0f;
    particle[i].dirx = sin(r * 2 * M_PI) * 0.25 * (1 + d);
    particle[i].diry = -cos(r * 2 * M_PI) * 0.25 * (1 + d);    
    particle[i].tex = tex;
}

void reset_particles()
{
    int i;
    for (i = 0; i < MAX_PARTICLES; i++)
        particle[i].live = 0;
}
#endif

void rotate2d(float &x, float &y, float angle)
{
    // 2d rotation:
    //x' = x cos f - y sin f
    //y' = y cos f + x sin f 

    float x1, y1;
    float sa,ca;
    
    x1 = x;
    y1 = y;
    sa = (float)sin(angle);
    ca = (float)cos(angle);

    x = x1 * ca - y1 * sa;
    y = y1 * ca + x1 * sa;

}

///////////////////////////////////////////////////
// Homegrown immediate-mode widgets (imgui_button/slider/textfield) lived
// here until the chrome moved to Dear ImGui; canvas popups/particles and
// the gUIState frame bookkeeping below are still in use.


void imgui_prepare()
{
	gUIState.hotitem = 0;
}

void imgui_finish()
{
	if (gUIState.mousedown == 0)
	{
		gUIState.activeitem = 0;
	}
	else
	{
		if (gUIState.activeitem == 0)
			gUIState.activeitem = -1;
	}
	// If no widget grabbed tab, clear focus
	if (gUIState.keyentered == SDLK_TAB)
		gUIState.kbditem = 0;
	// Clear the entered key
	gUIState.keyentered = 0;	
	gUIState.keychar = 0;
    gUIState.scroll = 0;

    if (gUIState.hotitem != gUIState.lasthotitem)
    {
        gUIState.lasthotitem = gUIState.hotitem;
        gUIState.lasthottick = SDL_GetTicks();
    }
}
