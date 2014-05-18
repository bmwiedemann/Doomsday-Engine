/** @file defs/dedfile.cpp
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "doomsday/defs/dedfile.h"
#include "doomsday/defs/dedparser.h"
#include "doomsday/filesys/fs_main.h"
#include "doomsday/filesys/fs_util.h"
#include <de/Log>

using namespace de;

char dedReadError[512];

void DED_SetError(char const *str)
{
    strncpy(dedReadError, str, sizeof(dedReadError));
}

void Def_ReadProcessDED(ded_t *defs, char const* path)
{
    LOG_AS("Def_ReadProcessDED");

    if(!path || !path[0]) return;

    de::Uri const uri(path, RC_NULL);
    if(!App_FileSystem().accessFile(uri))
    {
        LOG_RES_WARNING("\"%s\" not found!") << NativePath(uri.asText()).pretty();
        return;
    }

    // We use the File Ids to prevent loading the same files multiple times.
    if(!App_FileSystem().checkFileId(uri))
    {
        // Already handled.
        LOG_RES_XVERBOSE("\"%s\" has already been read") << NativePath(uri.asText()).pretty();
        return;
    }

    if(!DED_Read(defs, path))
    {
        App_FatalError("Def_ReadProcessDED: %s\n", dedReadError);
    }
}

int DED_ReadLump(ded_t* ded, lumpnum_t lumpNum)
{
    int lumpIdx;
    struct file1_s* file = F_FindFileForLumpNum2(lumpNum, &lumpIdx);
    if(file)
    {
        if(F_LumpLength(lumpNum) != 0)
        {
            uint8_t const* lumpPtr = F_CacheLump(file, lumpIdx);
            DED_ReadData(ded, (char const*)lumpPtr, Str_Text(F_ComposePath(file)));
            F_UnlockLump(file, lumpIdx);
        }
        return true;
    }
    DED_SetError("Bad lump number.");
    return false;
}

int DED_Read(ded_t* ded, const char* path)
{
    ddstring_t transPath;
    size_t bufferedDefSize;
    char* bufferedDef;
    filehandle_s* file;
    int result;

    // Compose the (possibly-translated) path.
    Str_InitStd(&transPath);
    Str_Set(&transPath, path);
    F_FixSlashes(&transPath, &transPath);
    F_ExpandBasePath(&transPath, &transPath);

    // Attempt to open a definition file on this path.
    file = F_Open(Str_Text(&transPath), "rb");
    if(!file)
    {
        DED_SetError("File could not be opened for reading.");
        Str_Free(&transPath);
        return false;
    }

    // We will buffer a local copy of the file. How large a buffer do we need?
    FileHandle_Seek(file, 0, SeekEnd);
    bufferedDefSize = FileHandle_Tell(file);
    FileHandle_Rewind(file);
    bufferedDef = (char*) calloc(1, bufferedDefSize + 1);
    if(NULL == bufferedDef)
    {
        DED_SetError("Out of memory while trying to buffer file for reading.");
        Str_Free(&transPath);
        return false;
    }

    // Copy the file into the local buffer and parse definitions.
    FileHandle_Read(file, (uint8_t*)bufferedDef, bufferedDefSize);
    F_Delete(file);
    result = DED_ReadData(ded, bufferedDef, Str_Text(&transPath));

    // Done. Release temporary storage and return the result.
    free(bufferedDef);
    Str_Free(&transPath);
    return result;
}

int DED_ReadData(ded_t* ded, const char* buffer, const char* _sourceFile)
{
    return DEDParser(ded).parse(buffer, _sourceFile);
}