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
#include "extrapin.h"
#include "ui_theme.h"

#define ANCHOR_WIRE_GREEN 0xff2fbf5f

ExtraPin::ExtraPin()
{
    set(0, 0, 1, 1,NULL);
    mPin.push_back(&mInputPin);
    mInputPin.set((mW-0.5)/2, (mH-0.5)/2, this, "Just a pin");

	mInputPin.mReadOnly = 1;
}

void ExtraPin::render(int aChipId)
{
    const UiTheme::Palette &pal = UiTheme::palette(gConfig.mThemeVariant);
    int ink = gBlackBackground ? pal.gridMajorDark : pal.canvasPaper;
    int core = gBlackBackground ? 0xffe6e7e9 : 0xff1c1d20;
    int hotBody = (gUIState.hotitem == aChipId);
    int hotPin = (IS_CHIP_ID(gUIState.hotitem) &&
        GET_CHIP_ID(gUIState.hotitem) == GET_CHIP_ID(aChipId) &&
        GET_PIN_ID(gUIState.hotitem) > 0);
    if (hotBody || hotPin)
        drawrect(mX, mY, mW, mH, UiTheme::withAlpha(pal.accent, 0x30));
    if (hotPin)
    {
        // Inner square: grab here to start another connection.
        drawrect(mX + 0.25f, mY + 0.25f, 0.5f, 0.5f, ANCHOR_WIRE_GREEN);
        drawrect(mX + 0.4f, mY + 0.4f, 0.2f, 0.2f, core);
    }
    else
    {
        if (hotBody)
        {
            // Outer ring: grab here (outside the inner square) to move it.
            drawrect(mX, mY, mW, 0.1f, pal.accent);
            drawrect(mX, mY + mH - 0.1f, mW, 0.1f, pal.accent);
            drawrect(mX, mY, 0.1f, mH, pal.accent);
            drawrect(mX + mW - 0.1f, mY, 0.1f, mH, pal.accent);
        }
        // Always-on anchor dot so bend points stay findable on dark and
        // paper canvas: canvas-colored ring plus a core, accent on hover.
        float cx = mX + mW / 2;
        float cy = mY + mH / 2;
        drawrect(cx - 0.18f, cy - 0.18f, 0.36f, 0.36f, ink);
        drawrect(cx - 0.10f, cy - 0.10f, 0.20f, 0.20f,
            hotBody ? pal.accent : core);
    }
}

void ExtraPin::update(float aTick) 
{
}    
