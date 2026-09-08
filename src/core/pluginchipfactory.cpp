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
#include "pluginchipfactory.h"
#include <tinyxml2.h>
#include "pluginchip.h"

using namespace tinyxml2;

PluginChipFactory::~PluginChipFactory()
{
}

PluginChipFactory::PluginChipFactory()
{
    XMLDocument doc;
    FILE * f = fopen("atanua.xml", "rb");
    if (!f)
        return;
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
        if (stricmp(part->Value(), "Plugin") == 0)
        {
            const char *dll = part->Attribute("dll");
            if (!dll)
                continue;
            DLLHANDLETYPE h = opendll(dll);
            if (h)
            {
                getatanuadllinfoproc getdllinfo = (getatanuadllinfoproc)getdllproc(h, "getatanuadllinfo");
                if (getdllinfo)
                {
                    atanuadllinfo *dllinfo = new atanuadllinfo;
                    getdllinfo(dllinfo);
                    if (dllinfo->mDllVersion <= ATANUA_PLUGIN_DLL_VERSION)
                    {
                        mDllInfo.push_back(dllinfo);
                        mDllHandle.push_back(h);
                    }
                    else
                    {
                        char temp[512];
                        sprintf(temp, "Plug-in \"%s\" version is incompatible \n"
                                     "with the current version of Atanua.\n"
                                     "\n"
                                     "Try to use it anyway?", dll);
                        if (okcancel(temp))
                        {
                            mDllInfo.push_back(dllinfo);
                            mDllHandle.push_back(h);
                        }
                        else
                        {
                            delete dllinfo;
                        }
                    }
                }
            }
        }
    }
}

void PluginChipFactory::getSupportedChips(vector<char *> aChipList[5])
{
    int i;
    int dirtycats[5];
    dirtycats[0] = dirtycats[1] = dirtycats[2] = dirtycats[3] = dirtycats[4] =  0;
    for (i = 0; i < (signed)mDllInfo.size(); i++)
    {
        int j;
        for (j = 0; j < mDllInfo[i]->mChipCount; j++)
        {
            int cat = 1;
            if (mDllInfo[i]->mChipCategory != NULL &&
                mDllInfo[i]->mChipCategory[j] != 0)
                cat = (mDllInfo[i]->mChipCategory[j] - 1) % 3;
            aChipList[cat].push_back((char*)(mDllInfo[i]->mChipName[j]));
            dirtycats[cat] = 1;
        }
    }
    
    // Push the divider to the categories where chips were inserted
    for (i = 0; i < 5; i++)
        if (dirtycats[i])
            aChipList[i].push_back(NULL);
}

Chip * PluginChipFactory::build(const char *aChipId)
{
    int i;
    for (i = 0; i < (signed)mDllInfo.size(); i++)
    {
        int j;
        for (j = 0; j < mDllInfo[i]->mChipCount; j++)
        {
            if (strcmp(aChipId, mDllInfo[i]->mChipName[j]) == 0)
            {
                PluginChip * c = new PluginChip(mDllHandle[i], mDllInfo[i]->mChipName[j]);
                return c;
            }
        }
    }
    return NULL;
}
