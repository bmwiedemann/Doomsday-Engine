/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2008 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * p_svtexarc.c: Archived texture names (save games).
 */

// HEADER FILES ------------------------------------------------------------

#if  __DOOM64TC__
#  include "doom64tc.h"
#elif __WOLFTC__
#  include "wolftc.h"
#elif __JDOOM__
#  include "jdoom.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#elif __JSTRIFE__
#  include "jstrife.h"
#endif

#include "p_mapsetup.h"
#include "p_svtexarc.h"

// MACROS ------------------------------------------------------------------

#define MAX_ARCHIVED_TEXTURES	1024
#define BADTEXNAME  "DD_BADTX"  // string that will be written in the texture
                                // archives to denote a missing texture.

// TYPES -------------------------------------------------------------------

typedef struct {
	char            name[9];
} texentry_t;

typedef struct {
    //// \todo Remove fixed limit.
	texentry_t      table[MAX_ARCHIVED_TEXTURES];
	int             count;
} texarchive_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// Savegame I/O:
extern void    SV_Write(void *data, int len);
extern void    SV_WriteShort(short val);
extern void    SV_Read(void *data, int len);
extern short   SV_ReadShort();

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

texarchive_t flat_archive;
texarchive_t tex_archive;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * Called for every texture and flat in the level before saving by
 * Sv_InitTextureArchives.
 */
void SV_PrepareTexture(int tex, materialtype_t type, texarchive_t *arc)
{
    int     c;
    char    name[9];

    // Get the name of the material.
    if(R_MaterialNameForNum(tex, type))
        strncpy(name, R_MaterialNameForNum(tex, type), 8);
    else
        strncpy(name, BADTEXNAME, 8);

    name[8] = 0;

    // Has this already been registered?
    for(c = 0; c < arc->count; c++)
    {
        if(!stricmp(arc->table[c].name, name))
        {
            // Yes, skip it...
            break;
        }
    }

    if(c == arc->count)
        strcpy(arc->table[arc->count++].name, name);
}

/**
 * Initializes the texture and flat archives (translation tables).
 * Must be called before saving. The tables are written before any
 * world data is saved.
 */
void SV_InitTextureArchives(void)
{
    uint    i;

    // Init flats.
    flat_archive.count = 0;

    for(i = 0; i < numsectors; ++i)
    {
        SV_PrepareTexture(P_GetInt(DMU_SECTOR, i, DMU_FLOOR_MATERIAL), MAT_FLAT, &flat_archive);
        SV_PrepareTexture(P_GetInt(DMU_SECTOR, i, DMU_CEILING_MATERIAL), MAT_FLAT, &flat_archive);
    }

    // Init textures.
    tex_archive.count = 0;

    for(i = 0; i < numsides; ++i)
    {
        SV_PrepareTexture(P_GetInt(DMU_SIDEDEF, i, DMU_MIDDLE_MATERIAL), MAT_TEXTURE, &tex_archive);
        SV_PrepareTexture(P_GetInt(DMU_SIDEDEF, i, DMU_TOP_MATERIAL), MAT_TEXTURE, &tex_archive);
        SV_PrepareTexture(P_GetInt(DMU_SIDEDEF, i, DMU_BOTTOM_MATERIAL), MAT_TEXTURE, &tex_archive);
    }
}

unsigned short SV_SearchArchive(texarchive_t *arc, char *name)
{
    int     i;

    for(i = 0; i < arc->count; i++)
        if(!stricmp(arc->table[i].name, name))
            return i;

    // Not found?!!!
    return 0;
}

/**
 * @return              The archive number of the given texture.
 */
unsigned short SV_MaterialArchiveNum(int materialID, materialtype_t type)
{
    char            name[9];

    if(R_MaterialNameForNum(materialID, type))
        strncpy(name, R_MaterialNameForNum(materialID, type), 8);
    else
        strncpy(name, BADTEXNAME, 8);
    name[8] = 0;

    return SV_SearchArchive((type == MAT_TEXTURE? &tex_archive : &flat_archive), name);
}

int SV_GetArchiveMaterial(int archivenum, materialtype_t type)
{
    switch(type)
    {
    case MAT_FLAT:
    if(!strncmp(flat_archive.table[archivenum].name, BADTEXNAME, 8))
        return -1;
    else
        return R_MaterialNumForName(flat_archive.table[archivenum].name, MAT_FLAT);

    case MAT_TEXTURE:
    if(!strncmp(tex_archive.table[archivenum].name, BADTEXNAME, 8))
        return -1;
    else
        return R_MaterialNumForName(tex_archive.table[archivenum].name, MAT_TEXTURE);

    default:
        Con_Error("SV_GetArchiveMaterial: Unknown material type %i.",
                  type);
    }

    return -1;
}

void SV_WriteTexArchive(texarchive_t *arc)
{
    int         i;

    SV_WriteShort(arc->count);
    for(i = 0; i < arc->count; ++i)
    {
        SV_Write(arc->table[i].name, 8);
    }
}

void SV_ReadTexArchive(texarchive_t *arc)
{
    int         i;

    arc->count = SV_ReadShort();
    for(i = 0; i < arc->count; ++i)
    {
        SV_Read(arc->table[i].name, 8);
        arc->table[i].name[8] = 0;
    }
}

void SV_WriteTextureArchive(void)
{
    SV_WriteTexArchive(&flat_archive);
    SV_WriteTexArchive(&tex_archive);
}

void SV_ReadTextureArchive(void)
{
    SV_ReadTexArchive(&flat_archive);
    SV_ReadTexArchive(&tex_archive);
}
