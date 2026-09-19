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
#include <stdio.h>
#include <stdlib.h>
#include "toolkit.h"
#include "fileutils.h"

File::File() 
{ 
    f = NULL; 
}

File::File(FILE *aFileHandle) 
{ 
    f = aFileHandle;
}

File::File(const char *aFilename, const char *aFileOpenTypes)
{
    f = fopen(aFilename, aFileOpenTypes);
}

File::~File()
{
    if (f) fclose(f);
}

int File::readbyte()
{
    char i = 0;
    if (!f)
        return i;
    if (fread(&i,1,1,f) != 1)
        return 0;
    return i;
}

int File::readword()
{
    short i = 0;
    if (!f)
        return i;
    if (fread(&i,2,1,f) != 1)
        return 0;
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
    return i;
#else
    return (signed short)SDL_Swap16(i);
#endif
}

int File::readint()
{
    int i = 0;
    if (!f)
        return i;
    if (fread(&i,4,1,f) != 1)
        return 0;
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
    return i;
#else
    return SDL_Swap32(i);
#endif
}

void File::readchars(char * dst, int count)
{
    if (!dst || count <= 0)
        return;
    // NOTE: must go through the virtual readbyte so MemoryFile snapshots
    // read from memory instead of (absent) disk handles.
    while (count > 0)
    {
        *dst = (char)readbyte();
        dst++;
        count--;
    }
}

void File::writebyte(char data)
{
    if (!f)
        return;
    fwrite(&data,1,1,f);
}

void File::writeword(short data)
{
    if (!f)
        return;
#if SDL_BYTEORDER != SDL_LIL_ENDIAN
    data = SDL_Swap16(data);
#endif
    fwrite(&data,2,1,f);
}

void File::writeint(int data)
{
    if (!f)
        return;
#if SDL_BYTEORDER != SDL_LIL_ENDIAN
    data = SDL_Swap32(data);
#endif
    fwrite(&data,4,1,f);
}

void File::writechars(const char * src, int count)
{
    if (!src || count <= 0)
        return;
    // NOTE: must go through the virtual writebyte so MemoryFile snapshots
    // actually store the bytes instead of dropping them for lack of a disk
    // handle (same virtual-dispatch rule as readchars above).
    while (count > 0)
    {
        writebyte(*src);
        src++;
        count--;
    }
}

int File::tell()
{
    if (!f)
        return 0;
    return ftell(f);
}

void File::seek(int pos)
{
    if (!f || pos < 0)
        return;
    fseek(f,pos,SEEK_SET);
}



MemoryFile::MemoryFile() 
{ 
    mDataIdx = 0;
}

MemoryFile::~MemoryFile()
{
}

int MemoryFile::readbyte()
{
    if (mDataIdx < 0 || mDataIdx >= (int)mData.size())
        return 0;
    char i = mData[mDataIdx];
    mDataIdx++;
    return i;
}

int MemoryFile::readword()
{
    if (mDataIdx < 0 || mDataIdx + 1 >= (int)mData.size())
    {
        mDataIdx = (int)mData.size();
        return 0;
    }
    unsigned short i = 0;
    i |= (unsigned char)readbyte(); i <<= 8;
    i |= (unsigned char)readbyte();
    return (short)i;
}

int MemoryFile::readint()
{
    if (mDataIdx < 0 || mDataIdx + 3 >= (int)mData.size())
    {
        mDataIdx = (int)mData.size();
        return 0;
    }
    unsigned int i = 0;
    i |= (unsigned char)readbyte(); i <<= 8;
    i |= (unsigned char)readbyte(); i <<= 8;
    i |= (unsigned char)readbyte(); i <<= 8;
    i |= (unsigned char)readbyte();
    return i;
}

void MemoryFile::writebyte(char data)
{
    mData.push_back(data);
    mDataIdx++;
}

void MemoryFile::writeword(short data)
{
    unsigned short d = (unsigned short)data;
    mData.push_back((d >> 8) & 0xff);
    mData.push_back((d >> 0) & 0xff);
    mDataIdx += 2;
}

void MemoryFile::writeint(int data)
{
    unsigned int d = (unsigned int)data;
    mData.push_back((d >> 24) & 0xff);
    mData.push_back((d >> 16) & 0xff);
    mData.push_back((d >> 8) & 0xff);
    mData.push_back((d >> 0) & 0xff);
    mDataIdx += 4;
}

int MemoryFile::tell()
{
    return mDataIdx;
}

void MemoryFile::seek(int pos)
{
    if (pos < 0)
        pos = 0;
    if (pos > (int)mData.size())
        pos = (int)mData.size();
    mDataIdx = pos;
}

int MemoryFile::bytesRemaining() const
{
    int rem = (int)mData.size() - mDataIdx;
    return rem > 0 ? rem : 0;
}
