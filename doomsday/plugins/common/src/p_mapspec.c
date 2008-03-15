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
 * p_mapspec.c:
 *
 * Line Tag handling. Line and Sector groups. Specialized iteration
 * routines, respective utility functions...
 */

// HEADER FILES ------------------------------------------------------------

#if __WOLFTC__
#  include "wolftc.h"
#elif __JDOOM__
#  include "jdoom.h"
#elif __JDOOM64__
#  include "jdoom64.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#elif __JSTRIFE__
#  include "jstrife.h"
#endif

#include "dmu_lib.h"
#include "p_mapsetup.h"
#include "p_mapspec.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct taglist_s {
    int         tag;
    iterlist_t *list;
} taglist_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

iterlist_t  *spechit; // for crossed line specials.
iterlist_t  *linespecials; // for surfaces that tick eg wall scrollers.

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static taglist_t *lineTagLists = NULL;
static int numLineTagLists = 0;

static taglist_t *sectorTagLists = NULL;
static int numSectorTagLists = 0;

// CODE --------------------------------------------------------------------

/**
 *
 */
void P_DestroyLineTagLists(void)
{
    int                 i;

    if(numLineTagLists == 0)
        return;

    for(i = 0; i < numLineTagLists; ++i)
    {
        P_EmptyIterList(lineTagLists[i].list);
        P_DestroyIterList(lineTagLists[i].list);
    }

    free(lineTagLists);
    lineTagLists = NULL;
    numLineTagLists = 0;
}

/**
 *
 */
iterlist_t *P_GetLineIterListForTag(int tag, boolean createNewList)
{
    int                 i;
    taglist_t          *tagList;

    // Do we have an existing list for this tag?
    for(i = 0; i < numLineTagLists; ++i)
        if(lineTagLists[i].tag == tag)
            return lineTagLists[i].list;

    if(!createNewList)
        return NULL;

    // Nope, we need to allocate another.
    numLineTagLists++;
    lineTagLists = realloc(lineTagLists, sizeof(taglist_t) * numLineTagLists);
    tagList = &lineTagLists[numLineTagLists - 1];
    tagList->tag = tag;

    return (tagList->list = P_CreateIterList());
}

/**
 *
 */
void P_DestroySectorTagLists(void)
{
    int                 i;

    if(numSectorTagLists == 0)
        return;

    for(i = 0; i < numSectorTagLists; ++i)
    {
        P_EmptyIterList(sectorTagLists[i].list);
        P_DestroyIterList(sectorTagLists[i].list);
    }

    free(sectorTagLists);
    sectorTagLists = NULL;
    numSectorTagLists = 0;
}

/**
 *
 */
iterlist_t *P_GetSectorIterListForTag(int tag, boolean createNewList)
{
    int                 i;
    taglist_t          *tagList;

    // Do we have an existing list for this tag?
    for(i = 0; i < numSectorTagLists; ++i)
        if(sectorTagLists[i].tag == tag)
            return sectorTagLists[i].list;

    if(!createNewList)
        return NULL;

    // Nope, we need to allocate another.
    numSectorTagLists++;
    sectorTagLists = realloc(sectorTagLists, sizeof(taglist_t) * numSectorTagLists);
    tagList = &sectorTagLists[numSectorTagLists - 1];
    tagList->tag = tag;

    return (tagList->list = P_CreateIterList());
}

/**
 * Get the sector on the other side of the line that is NOT the given sector.
 *
 * @param line          Ptr to the line to test.
 * @param sec           Reference sector to compare against.
 *
 * @return              Ptr to the other sector or @c NULL if the specified
 *                      line is NOT twosided.
 */
sector_t *P_GetNextSector(linedef_t *line, sector_t *sec)
{
    sector_t           *frontSec, *backSec;

    if(!sec || !line)
        return NULL;

    frontSec = P_GetPtrp(line, DMU_FRONT_SECTOR);
    backSec= P_GetPtrp(line, DMU_BACK_SECTOR);


    if(!frontSec || !backSec)
        return NULL;

    if(frontSec == sec)
        return backSec;

    return frontSec;
}

#define FELLF_MIN           0x1 // Get minimum. If not set, get maximum.

typedef struct findlightlevelparams_s {
    sector_t           *baseSec;
    byte                flags;
    float               val;
    sector_t           *foundSec;
} findlightlevelparams_t;

int findExtremalLightLevelInAdjacentSectors(void *ptr, void *context)
{
    linedef_t          *line = (linedef_t*) ptr;
    findlightlevelparams_t *params = (findlightlevelparams_t*) context;
    sector_t           *other;

    other = P_GetNextSector(line, params->baseSec);
    if(other)
    {
        float               lightLevel =
            P_GetFloatp(other, DMU_LIGHT_LEVEL);

        if(params->flags & FELLF_MIN)
        {
            if(lightLevel < params->val)
            {
                params->val = lightLevel;
                params->foundSec = other;
                if(params->val <= 0)
                    return 0; // Stop iteration. Can't get any darker.
            }
        }
        else
        {
            if(lightLevel > params->val)
            {
                params->val = lightLevel;
                params->foundSec = other;
                if(params->val >= 1)
                    return 0; // Stop iteration. Can't get any brighter.
            }
        }
    }

    return 1; // Continue iteration.
}

/**
 * Find the sector with the lowest light level in surrounding sectors.
 */
sector_t* P_FindSectorSurroundingLowestLight(sector_t *sec, float *val)
{
    findlightlevelparams_t params;

    params.val = DDMAXFLOAT;
    params.baseSec = sec;
    params.flags = FELLF_MIN;
    params.foundSec = NULL;
    P_Iteratep(sec, DMU_LINEDEF, &params,
               findExtremalLightLevelInAdjacentSectors);

    if(*val)
        *val = params.val;
    return params.foundSec;
}

/**
 * Find the sector with the highest light level in surrounding sectors.
 */
sector_t* P_FindSectorSurroundingHighestLight(sector_t *sec, float *val)
{
    findlightlevelparams_t params;

    params.val = DDMINFLOAT;
    params.baseSec = sec;
    params.flags = 0;
    params.foundSec = NULL;
    P_Iteratep(sec, DMU_LINEDEF, &params,
               findExtremalLightLevelInAdjacentSectors);

    if(val)
        *val = params.val;
    return params.foundSec;
}

#define FEPHF_MIN           0x1 // Get minium. If not set, get maximum.
#define FEPHF_FLOOR         0x2 // Get floors. If not set, get ceilings.

typedef struct findextremalplaneheightparams_s {
    sector_t           *baseSec;
    byte                flags;
    float               val;
    sector_t           *foundSec;
} findextremalplaneheightparams_t;

int findExtremalPlaneHeight(void *ptr, void *context)
{
    linedef_t          *ln = (linedef_t*) ptr;
    findextremalplaneheightparams_t *params =
        (findextremalplaneheightparams_t*) context;
    sector_t           *other;

    other = P_GetNextSector(ln, params->baseSec);
    if(other)
    {
        float               height =
            P_GetFloatp(other, ((params->flags & FEPHF_FLOOR)?
                                   DMU_FLOOR_HEIGHT : DMU_CEILING_HEIGHT));

        if(params->flags & FEPHF_MIN)
        {
            if(height < params->val)
            {
                params->val = height;
                params->foundSec = other;
            }
        }
        else
        {
            if(height > params->val)
            {
                params->val = height;
                params->foundSec = other;
            }
        }
    }

    return 1; // Continue iteration.
}

/**
 * Find the sector with the lowest floor height in surrounding sectors.
 */
sector_t* P_FindSectorSurroundingLowestFloor(sector_t* sec, float* val)
{
    findextremalplaneheightparams_t params;

    params.baseSec = sec;
    params.flags = FEPHF_MIN | FEPHF_FLOOR;
    params.val = DDMAXFLOAT;
    params.foundSec = NULL;
    P_Iteratep(sec, DMU_LINEDEF, &params, findExtremalPlaneHeight);

    if(val)
        *val = params.val;
    return params.foundSec;
}

/**
 * Find the sector with the highest floor height in surrounding sectors.
 */
sector_t* P_FindSectorSurroundingHighestFloor(sector_t* sec, float* val)
{
    findextremalplaneheightparams_t params;

    params.baseSec = sec;
    params.flags = FEPHF_FLOOR;
    params.val = DDMINFLOAT;
    params.foundSec = NULL;
    P_Iteratep(sec, DMU_LINEDEF, &params, findExtremalPlaneHeight);

    if(val)
        *val = params.val;
    return params.foundSec;
}

/**
 * Find lowest ceiling in the surrounding sector.
 */
sector_t* P_FindSectorSurroundingLowestCeiling(sector_t *sec, float *val)
{
    findextremalplaneheightparams_t params;

    params.baseSec = sec;
    params.flags = FEPHF_MIN;
    params.val = DDMAXFLOAT;
    params.foundSec = NULL;
    P_Iteratep(sec, DMU_LINEDEF, &params, findExtremalPlaneHeight);

    if(val)
        *val = params.val;
    return params.foundSec;
}

/**
 * Find highest ceiling in the surrounding sectors.
 */
sector_t* P_FindSectorSurroundingHighestCeiling(sector_t *sec, float *val)
{
    findextremalplaneheightparams_t params;

    params.baseSec = sec;
    params.flags = 0;
    params.val = DDMINFLOAT;
    params.foundSec = NULL;
    P_Iteratep(sec, DMU_LINEDEF, &params, findExtremalPlaneHeight);

    if(val)
        *val = params.val;
    return params.foundSec;
}

#define FNPHF_FLOOR             0x1 // Get floors, if not set get ceilings.
#define FNPHF_ABOVE             0x2 // Get next above, if not set get next below.

typedef struct findnextplaneheightparams_s {
    sector_t           *baseSec;
    float               baseHeight;
    byte                flags;
    float               val;
    sector_t           *foundSec;
} findnextplaneheightparams_t;

int findNextPlaneHeight(void *ptr, void *context)
{
    linedef_t          *li = (linedef_t*) ptr;
    findnextplaneheightparams_t *params =
        (findnextplaneheightparams_t*) context;
    sector_t           *other;

    other = P_GetNextSector(li, params->baseSec);
    if(other)
    {
        float               otherHeight =
            P_GetFloatp(other, ((params->flags & FNPHF_FLOOR)? DMU_FLOOR_HEIGHT : DMU_CEILING_HEIGHT));

        if(params->flags & FNPHF_ABOVE)
        {
            if(otherHeight < params->val && otherHeight > params->baseHeight)
            {
                params->val = otherHeight;
                params->foundSec = other;
            }
        }
        else
        {
            if(otherHeight > params->val && otherHeight < params->baseHeight)
            {
                params->val = otherHeight;
                params->foundSec = other;
            }
        }
    }

    return 1; // Continue iteration.
}

/**
 * Find the sector with the next highest floor in surrounding sectors.
 */
sector_t* P_FindSectorSurroundingNextHighestFloor(sector_t *sec,
                                                  float baseHeight,
                                                  float *val)
{
    findnextplaneheightparams_t params;

    params.baseSec = sec;
    params.baseHeight = baseHeight;
    params.flags = FNPHF_FLOOR | FNPHF_ABOVE;
    params.foundSec = NULL;
    params.val = DDMINFLOAT;
    P_Iteratep(sec, DMU_LINEDEF, &params, findNextPlaneHeight);

    if(val)
        *val = params.val;

    return params.foundSec;
}
