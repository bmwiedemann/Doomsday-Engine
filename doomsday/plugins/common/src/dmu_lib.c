/**\file dmu_lib.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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
 * Helper routines for accessing the DMU API.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <string.h>

#include "common.h"
#include "dmu_lib.h"
#include "p_terraintype.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct taglist_s {
    iterlist_t* list;
    int tag;
} taglist_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

iterlist_t* linespecials = 0; /// For surfaces that tick eg wall scrollers.

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static taglist_t* lineTagLists = 0;
static uint numLineTagLists = 0;

static taglist_t* sectorTagLists = 0;
static uint numSectorTagLists = 0;

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

LineDef* P_AllocDummyLine(void)
{
    xline_t* extra = Z_Calloc(sizeof(xline_t), PU_GAMESTATIC, 0);
    return P_AllocDummy(DMU_LINEDEF, extra);
}

void P_FreeDummyLine(LineDef* line)
{
    Z_Free(P_DummyExtraData(line));
    P_FreeDummy(line);
}

SideDef* P_AllocDummySideDef(void)
{
    return P_AllocDummy(DMU_SIDEDEF, 0);
}

void P_FreeDummySideDef(SideDef* sideDef)
{
    P_FreeDummy(sideDef);
}

void P_CopyLine(LineDef* dest, LineDef* src)
{
    SideDef* sidefrom, *sideto;
    xline_t* xsrc = P_ToXLine(src);
    xline_t* xdest = P_ToXLine(dest);
    int i, sidx;

    if(src == dest)
        return; // no point copying self

    // Copy the built-in properties
    for(i = 0; i < 2; ++i) // For each side
    {
        sidx = (i==0? DMU_SIDEDEF0 : DMU_SIDEDEF1);

        sidefrom = P_GetPtrp(src, sidx);
        sideto = P_GetPtrp(dest, sidx);

        if(!sidefrom || !sideto)
            continue;

#if 0
        // P_Copyp is not implemented in Doomsday yet.
        P_Copyp(DMU_TOP_MATERIAL_OFFSET_XY, sidefrom, sideto);
        P_Copyp(DMU_TOP_MATERIAL, sidefrom, sideto);
        P_Copyp(DMU_TOP_COLOR, sidefrom, sideto);

        P_Copyp(DMU_MIDDLE_MATERIAL, sidefrom, sideto);
        P_Copyp(DMU_MIDDLE_COLOR, sidefrom, sideto);
        P_Copyp(DMU_MIDDLE_BLENDMODE, sidefrom, sideto);

        P_Copyp(DMU_BOTTOM_MATERIAL, sidefrom, sideto);
        P_Copyp(DMU_BOTTOM_COLOR, sidefrom, sideto);
#else
        {
        float temp[4];
        coord_t itemp[2];

        P_SetPtrp(sideto, DMU_TOP_MATERIAL, P_GetPtrp(sidefrom, DMU_TOP_MATERIAL));
        P_GetDoublepv(sidefrom, DMU_TOP_MATERIAL_OFFSET_XY, itemp);
        P_SetDoublepv(sideto, DMU_TOP_MATERIAL_OFFSET_XY, itemp);
        P_GetFloatpv(sidefrom, DMU_TOP_COLOR, temp);
        P_SetFloatpv(sideto, DMU_TOP_COLOR, temp);

        P_SetPtrp(sideto, DMU_MIDDLE_MATERIAL, P_GetPtrp(sidefrom, DMU_MIDDLE_MATERIAL));
        P_GetDoublepv(sidefrom, DMU_MIDDLE_MATERIAL_OFFSET_XY, itemp);
        P_SetDoublepv(sideto, DMU_MIDDLE_MATERIAL_OFFSET_XY, itemp);
        P_SetFloatpv(sideto, DMU_MIDDLE_COLOR, temp);
        P_SetIntp(sideto, DMU_MIDDLE_BLENDMODE, P_GetIntp(sidefrom, DMU_MIDDLE_BLENDMODE));

        P_SetPtrp(sideto, DMU_BOTTOM_MATERIAL, P_GetPtrp(sidefrom, DMU_BOTTOM_MATERIAL));
        P_GetDoublepv(sidefrom, DMU_BOTTOM_MATERIAL_OFFSET_XY, itemp);
        P_SetDoublepv(sideto, DMU_BOTTOM_MATERIAL_OFFSET_XY, itemp);
        P_GetFloatpv(sidefrom, DMU_BOTTOM_COLOR, temp);
        P_SetFloatpv(sideto, DMU_BOTTOM_COLOR, temp);
        }
#endif
    }

    // Copy the extended properties too
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    xdest->special = xsrc->special;
    if(xsrc->xg && xdest->xg)
        memcpy(xdest->xg, xsrc->xg, sizeof(*xdest->xg));
    else
        xdest->xg = NULL;
#else
    xdest->special = xsrc->special;
    xdest->arg1 = xsrc->arg1;
    xdest->arg2 = xsrc->arg2;
    xdest->arg3 = xsrc->arg3;
    xdest->arg4 = xsrc->arg4;
    xdest->arg5 = xsrc->arg5;
#endif
}

void P_CopySector(Sector* dest, Sector* src)
{
    xsector_t* xsrc = P_ToXSector(src);
    xsector_t* xdest = P_ToXSector(dest);

    if(src == dest)
        return; // no point copying self.

    // Copy the built-in properties.
#if 0
    // P_Copyp is not implemented in Doomsday yet.
    P_Copyp(DMU_LIGHT_LEVEL, src, dest);
    P_Copyp(DMU_COLOR, src, dest);

    P_Copyp(DMU_FLOOR_HEIGHT, src, dest);
    P_Copyp(DMU_FLOOR_MATERIAL, src, dest);
    P_Copyp(DMU_FLOOR_COLOR, src, dest);
    P_Copyp(DMU_FLOOR_MATERIAL_OFFSET_XY, src, dest);
    P_Copyp(DMU_FLOOR_SPEED, src, dest);
    P_Copyp(DMU_FLOOR_TARGET_HEIGHT, src, dest);

    P_Copyp(DMU_CEILING_HEIGHT, src, dest);
    P_Copyp(DMU_CEILING_MATERIAL, src, dest);
    P_Copyp(DMU_CEILING_COLOR, src, dest);
    P_Copyp(DMU_CEILING_MATERIAL_OFFSET_XY, src, dest);
    P_Copyp(DMU_CEILING_SPEED, src, dest);
    P_Copyp(DMU_CEILING_TARGET_HEIGHT, src, dest);
#else
    {
    float ftemp[4];
    coord_t dtemp[2];

    P_SetFloatp(dest, DMU_LIGHT_LEVEL, P_GetFloatp(src, DMU_LIGHT_LEVEL));
    P_GetFloatpv(src, DMU_COLOR, ftemp);
    P_SetFloatpv(dest, DMU_COLOR, ftemp);

    P_SetDoublep(dest, DMU_FLOOR_HEIGHT, P_GetDoublep(src, DMU_FLOOR_HEIGHT));
    P_SetPtrp(dest, DMU_FLOOR_MATERIAL, P_GetPtrp(src, DMU_FLOOR_MATERIAL));
    P_GetFloatpv(src, DMU_FLOOR_COLOR, ftemp);
    P_SetFloatpv(dest, DMU_FLOOR_COLOR, ftemp);
    P_GetDoublepv(src, DMU_FLOOR_MATERIAL_OFFSET_XY, dtemp);
    P_SetDoublepv(dest, DMU_FLOOR_MATERIAL_OFFSET_XY, dtemp);
    P_SetIntp(dest, DMU_FLOOR_SPEED, P_GetIntp(src, DMU_FLOOR_SPEED));
    P_SetDoublep(dest, DMU_FLOOR_TARGET_HEIGHT, P_GetFloatp(src, DMU_FLOOR_TARGET_HEIGHT));

    P_SetDoublep(dest, DMU_CEILING_HEIGHT, P_GetDoublep(src, DMU_CEILING_HEIGHT));
    P_SetPtrp(dest, DMU_CEILING_MATERIAL, P_GetPtrp(src, DMU_CEILING_MATERIAL));
    P_GetFloatpv(src, DMU_CEILING_COLOR, ftemp);
    P_SetFloatpv(dest, DMU_CEILING_COLOR, ftemp);
    P_GetDoublepv(src, DMU_CEILING_MATERIAL_OFFSET_XY, dtemp);
    P_SetDoublepv(dest, DMU_CEILING_MATERIAL_OFFSET_XY, dtemp);
    P_SetIntp(dest, DMU_CEILING_SPEED, P_GetIntp(src, DMU_CEILING_SPEED));
    P_SetDoublep(dest, DMU_CEILING_TARGET_HEIGHT, P_GetFloatp(src, DMU_CEILING_TARGET_HEIGHT));
    }
#endif

    // Copy the extended properties too
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    xdest->special = xsrc->special;
    xdest->soundTraversed = xsrc->soundTraversed;
    xdest->soundTarget = xsrc->soundTarget;
#if __JHERETIC__
    xdest->seqType = xsrc->seqType;
#endif
    xdest->SP_floororigheight = xsrc->SP_floororigheight;
    xdest->SP_ceilorigheight = xsrc->SP_ceilorigheight;
    xdest->origLight = xsrc->origLight;
    memcpy(xdest->origRGB, xsrc->origRGB, sizeof(float) * 3);
    if(xsrc->xg && xdest->xg)
        memcpy(xdest->xg, xsrc->xg, sizeof(*xdest->xg));
    else
        xdest->xg = NULL;
#else
    xdest->special = xsrc->special;
    xdest->soundTraversed = xsrc->soundTraversed;
    xdest->soundTarget = xsrc->soundTarget;
    xdest->seqType = xsrc->seqType;
#endif
}

void P_DestroyLineTagLists(void)
{
    if(numLineTagLists == 0)
        return;

    { uint i;
    for(i = 0; i < numLineTagLists; ++i)
    {
        IterList_Empty(lineTagLists[i].list);
        IterList_Destruct(lineTagLists[i].list);
    }}

    free(lineTagLists);
    lineTagLists = NULL;
    numLineTagLists = 0;
}

iterlist_t* P_GetLineIterListForTag(int tag, boolean createNewList)
{
    taglist_t* tagList;
    uint i;

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

    return (tagList->list = IterList_ConstructDefault());
}

void P_DestroySectorTagLists(void)
{
    if(numSectorTagLists == 0)
        return;

    { uint i;
    for(i = 0; i < numSectorTagLists; ++i)
    {
        IterList_Empty(sectorTagLists[i].list);
        IterList_Destruct(sectorTagLists[i].list);
    }}

    free(sectorTagLists);
    sectorTagLists = NULL;
    numSectorTagLists = 0;
}

iterlist_t* P_GetSectorIterListForTag(int tag, boolean createNewList)
{
    taglist_t* tagList;
    uint i;

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

    return (tagList->list = IterList_ConstructDefault());
}

Sector* P_GetNextSector(LineDef* line, Sector* sec)
{
    Sector* frontSec;
    if(!sec || !line)
        return 0;
    frontSec = P_GetPtrp(line, DMU_FRONT_SECTOR);
    if(!frontSec)
        return 0;
    if(frontSec == sec)
        return P_GetPtrp(line, DMU_BACK_SECTOR);
    return frontSec;
}

int findExtremalLightLevelInAdjacentSectors(void* ptr, void* context)
{
    findlightlevelparams_t* params = (findlightlevelparams_t*) context;
    Sector* other = P_GetNextSector((LineDef*) ptr, params->baseSec);
    float lightLevel;

    if(!other)
        return false; // Continue iteration.

    lightLevel = P_GetFloatp(other, DMU_LIGHT_LEVEL);
    if(params->flags & FELLF_MIN)
    {
        if(lightLevel < params->val)
        {
            params->val = lightLevel;
            params->foundSec = other;
            if(params->val <= 0)
                return true; // Stop iteration. Can't get any darker.
        }
    }
    else if(lightLevel > params->val)
    {
        params->val = lightLevel;
        params->foundSec = other;
        if(params->val >= 1)
            return true; // Stop iteration. Can't get any brighter.
    }
    return false; // Continue iteration.
}

Sector* P_FindSectorSurroundingLowestLight(Sector* sec, float* val)
{
    findlightlevelparams_t params;
    params.flags = FELLF_MIN;
    params.val = DDMAXFLOAT;
    params.baseSec = sec;
    params.foundSec = 0;
    P_Iteratep(sec, DMU_LINEDEF, &params, findExtremalLightLevelInAdjacentSectors);
    if(*val)
        *val = params.val;
    return params.foundSec;
}

Sector* P_FindSectorSurroundingHighestLight(Sector* sec, float* val)
{
    findlightlevelparams_t params;

    params.flags = 0;
    params.val = DDMINFLOAT;
    params.baseSec = sec;
    params.foundSec = 0;
    P_Iteratep(sec, DMU_LINEDEF, &params, findExtremalLightLevelInAdjacentSectors);
    if(val)
        *val = params.val;
    return params.foundSec;
}

int findNextLightLevel(void* ptr, void* context)
{
    findnextlightlevelparams_t *params = (findnextlightlevelparams_t*) context;
    LineDef* li = (LineDef*) ptr;
    Sector* other = P_GetNextSector(li, params->baseSec);
    float otherLight;

    if(!other)
        return false; // Continue iteration.

    otherLight = P_GetFloatp(other, DMU_LIGHT_LEVEL);
    if(params->flags & FNLLF_ABOVE)
    {
        if(otherLight < params->val && otherLight > params->baseLight)
        {
            params->val = otherLight;
            params->foundSec = other;

            if(!(params->val > 0))
                return true; // Stop iteration. Can't get any darker.
        }
    }
    else if(otherLight > params->val && otherLight < params->baseLight)
    {
        params->val = otherLight;
        params->foundSec = other;

        if(!(params->val < 1))
            return true; // Stop iteration. Can't get any brighter.
    }
    return false; // Continue iteration.
}

Sector* P_FindSectorSurroundingNextLowestLight(Sector* sec, float baseLight, float* val)
{
    findnextlightlevelparams_t params;
    params.flags = 0;
    params.val = DDMINFLOAT;
    params.baseSec = sec;
    params.baseLight = baseLight;
    params.foundSec = 0;
    P_Iteratep(sec, DMU_LINEDEF, &params, findNextLightLevel);
    if(*val)
        *val = params.val;
    return params.foundSec;
}

Sector* P_FindSectorSurroundingNextHighestLight(Sector* sec, float baseLight, float* val)
{
    findnextlightlevelparams_t params;
    params.flags = FNLLF_ABOVE;
    params.val = DDMAXFLOAT;
    params.baseSec = sec;
    params.baseLight = baseLight;
    params.foundSec = 0;
    P_Iteratep(sec, DMU_LINEDEF, &params, findNextLightLevel);
    if(*val)
        *val = params.val;
    return params.foundSec;
}

int findExtremalPlaneHeight(void* ptr, void* context)
{
    findextremalplaneheightparams_t* params = (findextremalplaneheightparams_t*) context;
    Sector* other = P_GetNextSector((LineDef*) ptr, params->baseSec);
    coord_t height;

    if(!other)
        return false; // Continue iteration.

    height = P_GetDoublep(other, ((params->flags & FEPHF_FLOOR)? DMU_FLOOR_HEIGHT : DMU_CEILING_HEIGHT));
    if(params->flags & FEPHF_MIN)
    {
        if(height < params->val)
        {
            params->val = height;
            params->foundSec = other;
        }
    }
    else if(height > params->val)
    {
        params->val = height;
        params->foundSec = other;
    }

    return false; // Continue iteration.
}

Sector* P_FindSectorSurroundingLowestFloor(Sector* sec, coord_t max, coord_t* val)
{
    findextremalplaneheightparams_t params;
    params.flags = FEPHF_MIN | FEPHF_FLOOR;
    params.val = max;
    params.baseSec = sec;
    params.foundSec = 0;
    P_Iteratep(sec, DMU_LINEDEF, &params, findExtremalPlaneHeight);
    if(val)
        *val = params.val;
    return params.foundSec;
}

Sector* P_FindSectorSurroundingHighestFloor(Sector* sec, coord_t min, coord_t* val)
{
    findextremalplaneheightparams_t params;
    params.flags = FEPHF_FLOOR;
    params.val = min;
    params.baseSec = sec;
    params.foundSec = 0;
    P_Iteratep(sec, DMU_LINEDEF, &params, findExtremalPlaneHeight);
    if(val)
        *val = params.val;
    return params.foundSec;
}

Sector* P_FindSectorSurroundingLowestCeiling(Sector* sec, coord_t max, coord_t* val)
{
    findextremalplaneheightparams_t params;
    params.flags = FEPHF_MIN;
    params.val = max;
    params.baseSec = sec;
    params.foundSec = 0;
    P_Iteratep(sec, DMU_LINEDEF, &params, findExtremalPlaneHeight);
    if(val)
        *val = params.val;
    return params.foundSec;
}

Sector* P_FindSectorSurroundingHighestCeiling(Sector* sec, coord_t min, coord_t* val)
{
    findextremalplaneheightparams_t params;
    params.flags = 0;
    params.val = min;
    params.baseSec = sec;
    params.foundSec = 0;
    P_Iteratep(sec, DMU_LINEDEF, &params, findExtremalPlaneHeight);
    if(val)
        *val = params.val;
    return params.foundSec;
}

int findNextPlaneHeight(void* ptr, void* context)
{
    findnextplaneheightparams_t* params = (findnextplaneheightparams_t*) context;
    Sector* other = P_GetNextSector((LineDef*) ptr, params->baseSec);
    coord_t otherHeight;

    if(!other)
        return false; // Continue iteration.

    otherHeight = P_GetDoublep(other, ((params->flags & FNPHF_FLOOR)? DMU_FLOOR_HEIGHT : DMU_CEILING_HEIGHT));
    if(params->flags & FNPHF_ABOVE)
    {
        if(otherHeight < params->val && otherHeight > params->baseHeight)
        {
            params->val = otherHeight;
            params->foundSec = other;
        }
    }
    else if(otherHeight > params->val && otherHeight < params->baseHeight)
    {
        params->val = otherHeight;
        params->foundSec = other;
    }
    return false; // Continue iteration.
}

Sector* P_FindSectorSurroundingNextHighestFloor(Sector* sec, coord_t baseHeight, coord_t* val)
{
    findnextplaneheightparams_t params;

    params.flags = FNPHF_FLOOR | FNPHF_ABOVE;
    params.val = DDMAXFLOAT;
    params.baseSec = sec;
    params.baseHeight = baseHeight;
    params.foundSec = 0;
    P_Iteratep(sec, DMU_LINEDEF, &params, findNextPlaneHeight);
    if(val)
        *val = params.val;
    return params.foundSec;
}

Sector* P_FindSectorSurroundingNextHighestCeiling(Sector* sec, coord_t baseHeight, coord_t* val)
{
    findnextplaneheightparams_t params;
    params.flags = FNPHF_ABOVE;
    params.val = DDMAXFLOAT;
    params.baseSec = sec;
    params.baseHeight = baseHeight;
    params.foundSec = 0;
    P_Iteratep(sec, DMU_LINEDEF, &params, findNextPlaneHeight);
    if(val)
        *val = params.val;
    return params.foundSec;
}

Sector* P_FindSectorSurroundingNextLowestFloor(Sector* sec, coord_t baseHeight, coord_t* val)
{
    findnextplaneheightparams_t params;
    params.flags = FNPHF_FLOOR;
    params.val = DDMINFLOAT;
    params.baseSec = sec;
    params.baseHeight = baseHeight;
    params.foundSec = 0;
    P_Iteratep(sec, DMU_LINEDEF, &params, findNextPlaneHeight);
    if(val)
        *val = params.val;
    return params.foundSec;
}

Sector* P_FindSectorSurroundingNextLowestCeiling(Sector* sec, coord_t baseHeight, coord_t* val)
{
    findnextplaneheightparams_t params;
    params.flags = 0;
    params.val = DDMINFLOAT;
    params.baseSec = sec;
    params.baseHeight = baseHeight;
    params.foundSec = 0;
    P_Iteratep(sec, DMU_LINEDEF, &params, findNextPlaneHeight);
    if(val)
        *val = params.val;
    return params.foundSec;
}

float P_SectorLight(Sector* sector)
{
    return P_GetFloatp(sector, DMU_LIGHT_LEVEL);
}

void P_SectorSetLight(Sector* sector, float level)
{
    P_SetFloatp(sector, DMU_LIGHT_LEVEL, level);
}

void P_SectorModifyLight(Sector* sector, float value)
{
    float level = MINMAX_OF(0.f, P_SectorLight(sector) + value, 1.f);
    P_SectorSetLight(sector, level);
}

void P_SectorModifyLightx(Sector* sector, fixed_t value)
{
    P_SetFloatp(sector, DMU_LIGHT_LEVEL, P_SectorLight(sector) + FIX2FLT(value) / 255.0f);
}

void* P_SectorOrigin(Sector* sec)
{
    return P_GetPtrp(sec, DMU_BASE);
}

const terraintype_t* P_PlaneMaterialTerrainType(Sector* sec, int plane)
{
    return P_TerrainTypeForMaterial(P_GetPtrp(sec, (plane? DMU_CEILING_MATERIAL : DMU_FLOOR_MATERIAL)));
}
