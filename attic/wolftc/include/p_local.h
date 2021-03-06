/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2008 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1993-1996 by id Software, Inc.
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
 * p_local.h: Play functions, animation, global header.
 */

#ifndef __P_LOCAL__
#define __P_LOCAL__

#ifndef __JDOOM__
#  error "Using jDoom headers without __JDOOM__"
#endif

#ifndef __R_LOCAL__
#include "r_local.h"
#endif

#include "p_spec.h"
#include "p_start.h"
#include "p_actor.h"
#include "p_xg.h"

// Palette indices.
// For damage/bonus red-/gold-shifts
#define STARTREDPALS        1
#define STARTBONUSPALS      9
#define NUMREDPALS          8
#define NUMBONUSPALS        4

#define FLOATSPEED      4

#define MAXHEALTH       maxhealth  //100
#define VIEWHEIGHT      41

// player radius for movement checking
#define PLAYERRADIUS    16

// MAXRADIUS is for precalculated sector block boxes
// the spider demon is larger,
// but we do not have any moving sectors nearby
#define MAXRADIUS       32
#define MAXMOVE         30

#define USERANGE        64
#define MELEERANGE      64
#define MISSILERANGE    (32*64)

// follow a player exlusively for 3 seconds
#define BASETHRESHOLD       100


// GMJ 02/02/02
#define sentient(mobj) ((mobj)->health > 0 && (mobj)->info->seeState)

//
// P_TICK
//

#define thinkerCap      (*gi.thinkerCap)

extern int      TimerGame;         // tic countdown for deathmatch

//
// P_PSPR
//
void            P_SetupPsprites(player_t *curplayer);
void            P_MovePsprites(player_t *curplayer);
void            P_DropWeapon(player_t *player);
void            P_SetPsprite(player_t *player, int position, statenum_t stnum);

//
// P_USER
//
extern int      maxhealth, healthlimit, godmodehealth;
extern int      soulSphereHealth, soulSphereLimit, megaspherehealth;
extern int      armorpoints[4];    // Green, blue, IDFA and IDKFA points.
extern int      armorclass[4];     // Green and blue classes.

#define FRICTION_NORMAL     (0.90625f)
#define FRICTION_FLY        (0.91796875f)

#define ONFLOORZ        DDMINFLOAT
#define ONCEILINGZ      DDMAXFLOAT
#define FLOATRANDZ      (DDMAXFLOAT-1)

// Time interval for item respawning.
#define ITEMQUESIZE     128

extern int iquehead;
extern int iquetail;

boolean     P_MobjChangeState(mobj_t *mobj, statenum_t state);
void        P_MobjThinker(mobj_t *mobj);
void        P_RipperBlood(mobj_t *mo);
void        P_SetDoomsdayFlags(mobj_t *mo);
void        P_HitFloor(mobj_t *mo);

void        P_SpawnMapThing(spawnspot_t *th);
void        P_SpawnPlayer(spawnspot_t *mthing, int pnum);

//
// P_ENEMY
//

/**
 * Global state of boss brain.
 */
typedef struct braindata_s {
    int             easy;
    int             targetOn;
} braindata_t;

extern braindata_t brain;
extern mobj_t **brainTargets;
extern int numBrainTargets;
extern int numBrainTargetsAlloc;

extern boolean bossKilled;

void        P_SpawnBrainTargets(void);

void        P_NoiseAlert(mobj_t *target, mobj_t *emmiter);
int         P_Massacre(void);

//
// P_MAPUTL
//

#define OPENRANGE           (*(float*) DD_GetVariable(DD_OPENRANGE))
#define OPENTOP             (*(float*) DD_GetVariable(DD_OPENTOP))
#define OPENBOTTOM          (*(float*) DD_GetVariable(DD_OPENBOTTOM))
#define LOWFLOOR            (*(float*) DD_GetVariable(DD_LOWFLOOR))

//
// P_INTER
//
extern int      maxAmmo[NUM_AMMO_TYPES];
extern int      clipAmmo[NUM_AMMO_TYPES];

void            P_TouchSpecialMobj(mobj_t *special, mobj_t *toucher);

void            P_DamageMobj(mobj_t *target, mobj_t *inflictor, mobj_t *source,
                             int damage);
void            P_DamageMobj2(mobj_t *target, mobj_t *inflictor,
                              mobj_t *source, int damage, boolean stomping);

void            P_ExplodeMissile(mobj_t *mo);

#endif
