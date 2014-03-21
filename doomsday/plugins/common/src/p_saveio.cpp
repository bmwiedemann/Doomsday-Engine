/** @file p_saveio.cpp  Game save file IO.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "common.h"
#include "p_saveio.h"

#include "g_common.h"
#include "p_savedef.h"    // CONSISTENCY
#include <de/ArrayValue>
#include <de/FixedByteArray>
#include <de/NativePath>

// Used during write:
static de::Writer *writer;

// Used during read:
static de::Reader *reader;

static char sri8(reader_s *r)
{
    if(!r) return 0;
    int8_t val;
    DENG2_ASSERT(reader);
    *reader >> val;
    return char(val);
}

static short sri16(reader_s *r)
{
    if(!r) return 0;
    int16_t val;
    DENG2_ASSERT(reader);
    *reader >> val;
    return short(val);
}

static int sri32(reader_s *r)
{
    if(!r) return 0;
    DENG2_ASSERT(reader);
    int32_t val;
    *reader >> val;
    return int(val);
}

static float srf(reader_s *r)
{
    if(!r) return 0;
    DENG2_ASSERT(reader);
    DENG2_ASSERT(sizeof(float) == 4);
    int32_t val;
    *reader >> val;
    float rerVal = 0;
    std::memcpy(&rerVal, &val, 4);
    return rerVal;
}

static void srd(reader_s *r, char *data, int len)
{
    if(!r) return;
    DENG2_ASSERT(reader);
    if(data)
    {
        de::Block tmp(len);
        *reader >> de::FixedByteArray(tmp);
        tmp.get(0, (de::Block::Byte *)data, len);
    }
    else
    {
        reader->seek(len);
    }
}

reader_s *SV_NewReader()
{
    DENG2_ASSERT(reader != 0);
    return Reader_NewWithCallbacks(sri8, sri16, sri32, srf, srd);
}

de::Reader &SV_RawReader()
{
    if(reader != 0)
    {
        return *reader;
    }
    throw de::Error("SV_RawReader", "No map reader exists");
}

void SV_CloseFile()
{
    delete reader; reader = 0;
    delete writer; writer = 0;
}

bool SV_OpenFileForRead(de::File const &file)
{
    SV_CloseFile();
    reader = new de::Reader(file);
    return true;
}

bool SV_OpenFileForWrite(de::IByteArray &block)
{
    SV_CloseFile();
    writer = new de::Writer(block);
    return true;
}

#if 0
bool SV_OpenFile_LZSS(de::Path path)
{
    App_Log(DE2_DEV_RES_XVERBOSE, "SV_OpenFile: Opening \"%s\"",
            de::NativePath(path).pretty().toLatin1().constData());

    DENG2_ASSERT(savefile == 0);
    savefile = lzOpen(de::NativePath(path).expand().toUtf8().constData(), "wp");
    return savefile != 0;
}

void SV_CloseFile_LZSS()
{
    if(savefile)
    {
        lzClose(savefile);
        savefile = 0;
    }
}

void SV_AssertSegment(int segmentId)
{
#if __JHEXEN__
    if(segmentId == ASEG_END && SV_HxBytesLeft() < 4)
    {
        App_Log(DE2_LOG_WARNING, "Savegame lacks ASEG_END marker (unexpected end-of-file)");
        return;
    }
    if(SV_ReadLong() != segmentId)
    {
        Con_Error("Corrupt save game: Segment [%d] failed alignment check", segmentId);
    }
#else
    DENG_UNUSED(segmentId);
#endif
}

void SV_BeginSegment(int segType)
{
#if __JHEXEN__
    SV_WriteLong(segType);
#else
    DENG_UNUSED(segType);
#endif
}

void SV_EndSegment()
{
    SV_BeginSegment(ASEG_END);
}

void SV_WriteSessionMetadata(de::game::SessionMetadata const &metadata, Writer *writer)
{
    DENG2_ASSERT(writer != 0);

    Writer_WriteInt32(writer, metadata["magic"].value().asNumber());
    Writer_WriteInt32(writer, metadata["version"].value().asNumber());

    AutoStr *gameIdentityKeyStr = AutoStr_FromTextStd(metadata["gameIdentityKey"].value().asText().toUtf8().constData());
    Str_Write(gameIdentityKeyStr, writer);

    AutoStr *descriptionStr = AutoStr_FromTextStd(metadata["userDescription"].value().asText().toUtf8().constData());
    Str_Write(descriptionStr, writer);

    Uri *mapUri = Uri_NewWithPath2(metadata["mapUri"].value().asText().toUtf8().constData(), RC_NULL);
    Uri_Write(mapUri, writer);
    Uri_Delete(mapUri); mapUri = 0;

#if !__JHEXEN__
    Writer_WriteInt32(writer, metadata["mapTime"].value().asNumber());
#endif

    GameRuleset *rules = GameRuleset::fromRecord(metadata["gameRules"].valueAsRecord());
    rules->write(writer);
    delete rules; rules = 0;

#if !__JHEXEN__
    de::ArrayValue const &players = metadata["players"].value().as<de::ArrayValue>();
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        Writer_WriteByte(writer, byte(players.at(i).asNumber()));
    }
#endif

    Writer_WriteInt32(writer, metadata["sessionId"].value().asNumber());
}

void SV_WriteConsistencyBytes()
{
#if !__JHEXEN__
    SV_WriteByte(CONSISTENCY);
#endif
}

void SV_ReadConsistencyBytes()
{
#if !__JHEXEN__
    if(SV_ReadByte() != CONSISTENCY)
    {
        Con_Error("Corrupt save game: Consistency test failed.");
    }
#endif
}
#endif

static void swi8(writer_s *w, char val)
{
    if(!w) return;
    DENG2_ASSERT(writer);
    *writer << val;
}

static void swi16(Writer *w, short val)
{
    if(!w) return;
    DENG2_ASSERT(writer);
    *writer << val;
}

static void swi32(Writer *w, int val)
{
    if(!w) return;
    DENG2_ASSERT(writer);
    *writer << val;
}

static void swf(Writer *w, float val)
{
    if(!w) return;
    DENG2_ASSERT(writer);
    DENG2_ASSERT(sizeof(float) == 4);

    int32_t temp = 0;
    std::memcpy(&temp, &val, 4);
    *writer << val;
}

static void swd(Writer *w, char const *data, int len)
{
    if(!w) return;
    DENG2_ASSERT(writer);
    if(data)
    {
        *writer << de::Block(data, len);
    }
}

writer_s *SV_NewWriter()
{
    return Writer_NewWithCallbacks(swi8, swi16, swi32, swf, swd);
}

de::Writer &SV_RawWriter()
{
    if(writer != 0)
    {
        return *writer;
    }
    throw de::Error("SV_RawWriter", "No map writer exists");
}
