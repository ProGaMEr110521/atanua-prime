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
#include <tinyxml2.h>
#include <time.h>

using namespace tinyxml2;

AtanuaConfig::AtanuaConfig()
{
    mAntialiasedLines = 1;
    mPropagateInvalidState = PINSTATE_PROPAGATE_INVALID;
    mWireFry = 1;
    mToolkitWidth = 150;
    mMaxPhysicsMs = 40;
    mWindowWidth = 1280;
    mWindowHeight = 800;
    mTooltipDelay = 1500;
    mLinePickTolerance = 0.05f;
    mLineEndTolerance = 0.2f;
    mLineSplitDragDistance = 10;
    mCustomCursors = 1;
    mPerformanceIndicators = 0;
    mChipCloneDragDistance = 10;
    mSwapShiftAndCtrl = 0;
    mAudioEnable = 1;
    mUserInfo = mystrdup("[No user name set]");
    mFontCacheMax = 512;
    mUseVBOs = 0;
    mUseOldFontSystem = 0;
    mUseBlending = 1;
	mMaxActiveBoxes = 200;
	mLEDSamples = 40;
	mPhysicsKHz = 1;
	mAutosaveDir = mystrdup("");
	mAutosaveCount = 10;
	mAutosaveEnable = 1;
	mAutosaveInterval = 5;
}

AtanuaConfig::~AtanuaConfig()
{
    delete[] mUserInfo;
    delete[] mAutosaveDir;
}

static int h2i(char d)
{
    if (d >= '0' && d <= '9')
        return d - '0';
    if (d >= 'A' && d <= 'F')
        return d - 'A' + 10;
    return -1;
}


void AtanuaConfig::load()
{
    XMLDocument doc;
    FILE * f = fopen("atanua.xml", "rb");

    if (!f)
    {
        // Save config
        XMLNode *decl = doc.NewDeclaration();
        doc.InsertFirstChild(decl);
        XMLElement * topelement = doc.NewElement("AtanuaConfig");
        topelement->SetAttribute("GeneratedWith", TITLE);
        doc.InsertEndChild(topelement);

        XMLElement *element;

        element = doc.NewElement("PropagateInvalidState");
        topelement->InsertEndChild(element);
        element->SetAttribute("value", mPropagateInvalidState == PINSTATE_PROPAGATE_INVALID);

        element = doc.NewElement("CustomCursors");
        topelement->InsertEndChild(element);
        element->SetAttribute("value", mCustomCursors);

        element = doc.NewElement("PerformanceIndicators");
        topelement->InsertEndChild(element);
        element->SetAttribute("value", mPerformanceIndicators);

        element = doc.NewElement("SwapShiftAndCtrl");
        topelement->InsertEndChild(element);
        element->SetAttribute("value", mSwapShiftAndCtrl);

        element = doc.NewElement("WireFry");
        topelement->InsertEndChild(element);
        element->SetAttribute("value", mWireFry);

        element = doc.NewElement("AudioEnable");
        topelement->InsertEndChild(element);
        element->SetAttribute("value", mAudioEnable);

        element = doc.NewElement("ToolkitWidth");
        topelement->InsertEndChild(element);
        element->SetAttribute("value", mToolkitWidth);

        element = doc.NewElement("MaxPhysicsMs");
        topelement->InsertEndChild(element);
        element->SetAttribute("value", mMaxPhysicsMs);

        element = doc.NewElement("InitialWindow");
        topelement->InsertEndChild(element);
        element->SetAttribute("width", mWindowWidth);
        element->SetAttribute("height", mWindowHeight);

        element = doc.NewElement("TooltipDelay");
        topelement->InsertEndChild(element);
        element->SetAttribute("value", mTooltipDelay);

        element = doc.NewElement("LinePickTolerance");
        topelement->InsertEndChild(element);
        element->SetAttribute("value", mLinePickTolerance);

        element = doc.NewElement("LineEndTolerance");
        topelement->InsertEndChild(element);
        element->SetAttribute("value", mLineEndTolerance);

        element = doc.NewElement("LineSplitDragDistance");
        topelement->InsertEndChild(element);
        element->SetAttribute("value", mLineSplitDragDistance);

        element = doc.NewElement("ChipCloneDragDistance");
        topelement->InsertEndChild(element);
        element->SetAttribute("value", mChipCloneDragDistance);

        element = doc.NewElement("User");
        topelement->InsertEndChild(element);
        element->SetAttribute("name", "[No user name set]");

        element = doc.NewElement("FontSystem");
        topelement->InsertEndChild(element);
        element->SetAttribute("CacheKeys", mFontCacheMax);
        element->SetAttribute("VBO", mUseVBOs);
        element->SetAttribute("SafeMode", mUseOldFontSystem);

        element = doc.NewElement("PerformanceOptions");
        topelement->InsertEndChild(element);
        element->SetAttribute("Blending", mUseBlending);
        element->SetAttribute("AntialiasedLines", mAntialiasedLines);

        element = doc.NewElement("Limits");
        topelement->InsertEndChild(element);
		element->SetAttribute("MaxBoxes", mMaxActiveBoxes);
		element->SetAttribute("PhysicsKHz", mPhysicsKHz);

        element = doc.NewElement("LED");
        topelement->InsertEndChild(element);
		element->SetAttribute("Samples", mLEDSamples);

        element = doc.NewElement("Autosave");
        topelement->InsertEndChild(element);
		element->SetAttribute("Directory", mAutosaveDir ? mAutosaveDir : "");
		element->SetAttribute("Enable", mAutosaveEnable);
		element->SetAttribute("SaveCount", mAutosaveCount);
		element->SetAttribute("Interval", mAutosaveInterval);

        f = fopen("atanua.xml", "wb");
        if (f)
        {
            doc.SaveFile(f);
            fclose(f);
        }
    }
    else
    {
        if (doc.LoadFile(f) != XML_SUCCESS)
        {
            fclose(f);
            return;
        }
        fclose(f);
        // Load config
        XMLElement *root = doc.FirstChildElement("AtanuaConfig");
        if (!root)
            return;
        for (XMLElement *part = root->FirstChildElement(); part != 0; part = part->NextSiblingElement())
        {
            if (stricmp(part->Value(), "FontSystem") == 0)
            {
                part->QueryIntAttribute("CacheKeys", &mFontCacheMax);
                part->QueryIntAttribute("VBO", &mUseVBOs);
                part->QueryIntAttribute("SafeMode", &mUseOldFontSystem);
            }
            else
            if (stricmp(part->Value(), "PerformanceOptions") == 0)
            {
                part->QueryIntAttribute("AntialiasedLines", &mAntialiasedLines);
                part->QueryIntAttribute("Blending", &mUseBlending);
            }
            else
            if (stricmp(part->Value(), "PropagateInvalidState") == 0)
            {
                int propagate = 0;
                part->QueryIntAttribute("value", &propagate);
                if (propagate)
                    mPropagateInvalidState = PINSTATE_PROPAGATE_INVALID;
                else
                    mPropagateInvalidState = PINSTATE_HIGHZ;
            }
            else
            if (stricmp(part->Value(), "CustomCursors") == 0)
            {
                part->QueryIntAttribute("value", &mCustomCursors);
            }
            else
            if (stricmp(part->Value(), "PerformanceIndicators") == 0)
            {
                part->QueryIntAttribute("value", &mPerformanceIndicators);
            }
            else
            if (stricmp(part->Value(), "SwapShiftAndCtrl") == 0)
            {
                part->QueryIntAttribute("value", &mSwapShiftAndCtrl);
            }
            else
            if (stricmp(part->Value(), "WireFry") == 0)
            {
                part->QueryIntAttribute("value", &mWireFry);
            }
            else
            if (stricmp(part->Value(), "AudioEnable") == 0)
            {
                part->QueryIntAttribute("value", &mAudioEnable);
            }
            else
            if (stricmp(part->Value(), "ToolkitWidth") == 0)
            {
                part->QueryIntAttribute("value", &mToolkitWidth);
            }
            else
            if (stricmp(part->Value(), "MaxPhysicsMs") == 0)
            {
                part->QueryIntAttribute("value", &mMaxPhysicsMs);
            }
            else
            if (stricmp(part->Value(), "InitialWindow") == 0)
            {
                part->QueryIntAttribute("width", &mWindowWidth);
                part->QueryIntAttribute("height", &mWindowHeight);
            }
            else
            if (stricmp(part->Value(), "TooltipDelay") == 0)
            {
                part->QueryIntAttribute("value", &mTooltipDelay);
            }
            else
            if (stricmp(part->Value(), "LinePickTolerance") == 0)
            {
                part->QueryFloatAttribute("value", &mLinePickTolerance);
            }
            else
            if (stricmp(part->Value(), "LineEndTolerance") == 0)
            {
                part->QueryFloatAttribute("value", &mLineEndTolerance);
            }
            else
            if (stricmp(part->Value(), "LineSplitDragDistance") == 0)
            {
                part->QueryFloatAttribute("value", &mLineSplitDragDistance);
            }
            else
            if (stricmp(part->Value(), "ChipCloneDragDistance") == 0)
            {
                part->QueryFloatAttribute("value", &mChipCloneDragDistance);
            }
            else
			if (stricmp(part->Value(), "Limits") == 0)
			{
				part->QueryIntAttribute("MaxBoxes", &mMaxActiveBoxes);
				part->QueryIntAttribute("PhysicsKHz", &mPhysicsKHz);
			}
			else
			if (stricmp(part->Value(), "LED") == 0)
			{
				part->QueryIntAttribute("Samples", &mLEDSamples);
				if (mLEDSamples <= 0) mLEDSamples = 1;
				if (mLEDSamples > 10000) mLEDSamples = 10000;
			}
			else
			if (stricmp(part->Value(), "User") == 0)
			{
				const char *t = part->Attribute("name");
				if (t && strlen(t) > 0)
				{
					delete[] mUserInfo;
					mUserInfo = mystrdup(t);
				}
			}
			if (stricmp(part->Value(), "Autosave") == 0)
			{
				part->QueryIntAttribute("Enable", &mAutosaveEnable);
				part->QueryIntAttribute("SaveCount", &mAutosaveCount);
				part->QueryIntAttribute("Interval", &mAutosaveInterval);
				if (mAutosaveCount <= 0) mAutosaveCount = 1;
				if (mAutosaveCount > 100) mAutosaveCount = 100;
				if (mAutosaveInterval <= 0) mAutosaveInterval = 1;
				const char * dir = part->Attribute("Directory");
				delete[] mAutosaveDir;
				mAutosaveDir = mystrdup("");
				if (dir && dir[0])
				{
					int len = (int)strlen(dir);
					if (dir[len - 1] == '/' || dir[len - 1] == '\\')
					{
						delete[] mAutosaveDir;
						mAutosaveDir = mystrdup(dir);
					}
					else
					{
						char temp[1024];
						snprintf(temp, sizeof(temp), "%s/", dir);
						delete[] mAutosaveDir;
						mAutosaveDir = mystrdup(temp);
					}
				}
			}
        }
    }
}
