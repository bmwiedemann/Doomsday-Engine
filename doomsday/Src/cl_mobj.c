
//**************************************************************************
//**
//** CL_MOBJ.C
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_system.h"
#include "de_console.h"
#include "de_network.h"
#include "de_play.h"
#include "de_audio.h"

// MACROS ------------------------------------------------------------------

// The client mobjs are stored into a hash to speed up the searching.
#define HASH_SIZE		256

// Convert 8.8/10.6 fixed point to 16.16.
#define UNFIXED8_8(x)	(((x) << 16) / 256)
#define UNFIXED10_6(x)	(((x) << 16) / 64)

// Milliseconds it takes for Unpredictable and Hidden mobjs to be 
// removed from the hash. Under normal circumstances, the special
// status should be removed fairly quickly (a matter of out-of-sequence
// frames or sounds playing before a mobj is sent).
#define	CLMOBJ_TIMEOUT	10000	// 10 seconds

// TYPES -------------------------------------------------------------------

typedef struct cmhash_s {
	clmobj_t *first, *last;
} cmhash_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int /*latest_frame_size,*/ gotframe;
extern int predicted_tics;

extern playerstate_t playerstate[MAXPLAYERS];

// PUBLIC DATA DEFINITIONS -------------------------------------------------

cmhash_t cmHash[HASH_SIZE];

//int	predict_tics = 70;

// Time skew tells whether client time is running before or after frame time.
//int net_timeskew = 0;			
//int net_skewdampen = 5;		// In frames.
//boolean net_showskew = false;

//boolean pred_forward = true;	// Predicting forward in time.

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static byte previous_time = 0;
//static int dampencount = 0;
static boolean buffers_ok = false;

// CODE --------------------------------------------------------------------

/*
 * Returns a pointer to the hash chain with the specified id.
 */
cmhash_t *Cl_MobjHash(thid_t id)
{
	return &cmHash[ (uint) id % HASH_SIZE ];
}

/*
 * Links the clmobj into the client mobj hash table.
 */
void Cl_LinkMobj(clmobj_t *cmo, thid_t id)
{
	cmhash_t *hash = Cl_MobjHash(id);

	// Set the ID.
	cmo->mo.thinker.id = id;
	cmo->next = NULL;

	if(hash->last)
	{
		hash->last->next = cmo;
		cmo->prev = hash->last;
	}
	hash->last = cmo;

	if(!hash->first)
	{
		hash->first = cmo;
	}
}

/*
 * Unlinks the clmobj from the client mobj hash table.
 */
void Cl_UnlinkMobj(clmobj_t *cmo)
{
	cmhash_t *hash = Cl_MobjHash(cmo->mo.thinker.id);

#ifdef _DEBUG
	if(cmo->flags & CLMF_HIDDEN)
	{
		Con_Printf("Cl_UnlinkMobj: Hidden mobj %i unlinked.\n", 
			cmo->mo.thinker.id);
	}
#endif

	if(hash->first == cmo) hash->first = cmo->next;
	if(hash->last == cmo) hash->last = cmo->prev;
	if(cmo->next) cmo->next->prev = cmo->prev;
	if(cmo->prev) cmo->prev->next = cmo->next;
}

/*
 * Searches through the client mobj hash table and returns the clmobj 
 * with the specified ID, if that exists.
 */
clmobj_t *Cl_FindMobj(thid_t id)
{
	cmhash_t *hash = Cl_MobjHash(id);
	clmobj_t *cmo;

	// Scan the existing client mobjs.
	for(cmo = hash->first; cmo; cmo = cmo->next)
	{
		if(cmo->mo.thinker.id == id) return cmo;
	}

	// Not found!
	return NULL;
}

/*
 * Aborts and returns false if the callback returns false.
 * Otherwise returns true.
 */
boolean Cl_MobjIterator(boolean (*callback)(clmobj_t*, void*), void *parm)
{
	clmobj_t *cmo;
	int i;

	for(i = 0; i < HASH_SIZE; i++)
	{
		for(cmo = cmHash[i].first; cmo; cmo = cmo->next)
		{
			if(!callback(cmo, parm)) return false;
		}
	}
	return true;
}

/*
 * Unlinks the thing from sectorlinks and if the object is solid, 
 * the blockmap.
 */
void Cl_UnsetThingPosition(clmobj_t *cmo)
{
	P_UnlinkThing(&cmo->mo);
}

/*
 * Links the thing into sectorlinks and if the object is solid, the 
 * blockmap. Linking to sectorlinks makes the thing visible and linking  
 * to the blockmap makes it possible to interact with it (collide).
 * If the client mobj is Hidden, it will not be linked anywhere.
 */
void Cl_SetThingPosition(clmobj_t *cmo)
{
	mobj_t *thing = &cmo->mo;

	if(cmo->flags & (CLMF_HIDDEN | CLMF_UNPREDICTABLE) 
		|| thing->dplayer) 
	{
		// We do not yet have all the details about Hidden mobjs.
		// The server hasn't sent us a Create Mobj delta for them.
		// Client mobjs that belong to players remain unlinked.
		return;
	}

	P_LinkThing(thing, 
		(thing->ddflags & DDMF_DONTDRAW? 0 : DDLINK_SECTOR) 
		| (thing->ddflags & DDMF_SOLID? DDLINK_BLOCKMAP : 0));
}

/*
 * Change the state of a mobj.
 */
void Cl_SetThingState(mobj_t *mo, int stnum)
{
	if(stnum < 0) return;
	do 
	{
		P_SetState(mo, stnum);
		stnum = states[stnum].nextstate;
	}
	while(!mo->tics && stnum > 0);

	// Update mobj's type (this is not perfectly reliable...)
	// from the stateowners table.
	if(stateowners[stnum])
		mo->type = stateowners[stnum] - mobjinfo;
	else
		mo->type = 0;
}

/*
 * Updates floorz and ceilingz of the mobj.
 */
void Cl_CheckMobj(mobj_t *mo)
{
	boolean onFloor = false, inCeiling = false;

	if(mo->z == DDMININT) 
	{
		onFloor = true;
		mo->z = mo->floorz;
	}
	if(mo->z == DDMAXINT)
	{
		inCeiling = true;
		mo->z = mo->ceilingz - mo->height;
	}

	// Find out floor and ceiling z.
	P_CheckPosition2(mo, mo->x, mo->y, mo->z);	
	mo->floorz = tmfloorz;
	mo->ceilingz = tmceilingz;

	if(onFloor)
	{
		mo->z = mo->floorz;
	}
	if(inCeiling)
	{
		mo->z = mo->ceilingz - mo->height;
	}
}

/*
 * Make the real player mobj identical with the client mobj.
 * The client mobj is always unlinked. Only the *real* mobj is visible.
 * (The real mobj was created by the Game.)
 */
void Cl_UpdateRealPlayerMobj(mobj_t *mo, mobj_t *clmo, int flags)
{
#if _DEBUG
	if(!mo || !clmo)
	{
		Con_Message("Cl_UpdateRealPlayerMobj: mo=%p clmo=%p\n", mo, clmo);
		return;
	}
#endif

	if(flags & (MDF_POS_X | MDF_POS_Y))
	{
		// We have to unlink the real mobj before we move it.
		P_UnlinkThing(mo);
		mo->x = clmo->x;
		mo->y = clmo->y;
		P_LinkThing(mo, DDLINK_SECTOR | DDLINK_BLOCKMAP);
	}
	mo->z = clmo->z;
	mo->momx = clmo->momx;
	mo->momy = clmo->momy;
	mo->momz = clmo->momz;
	mo->angle = clmo->angle;
	mo->sprite = clmo->sprite;
	mo->frame = clmo->frame;
	//mo->nextframe = clmo->nextframe;
	mo->tics = clmo->tics;
	mo->state = clmo->state;
	//mo->nexttime = clmo->nexttime;
	mo->ddflags = clmo->ddflags;
	mo->radius = clmo->radius;
	mo->height = clmo->height;
	mo->floorclip = clmo->floorclip;
	mo->floorz = clmo->floorz;
	mo->ceilingz = clmo->ceilingz;
	mo->selector &= ~DDMOBJ_SELECTOR_MASK;
	mo->selector |= clmo->selector & DDMOBJ_SELECTOR_MASK;
	mo->visangle = clmo->angle >> 16;
	
	//if(flags & MDF_FLAGS) CON_Printf("Cl_RMD: ddf=%x\n", mo->ddflags);
}

//==========================================================================
// Cl_ReadMobjDelta
//	Reads a single mobj delta from the message buffer and applies
//	it to the client mobj in question.
//	For client mobjs that belong to players, updates the real player mobj.
//	Returns false only if the list of deltas ends.
//
//	THIS FUNCTION IS NOW OBSOLETE (only used with old psv_frame packets)
//==========================================================================
int Cl_ReadMobjDelta(void)
{
	thid_t id = Msg_ReadShort();	// Read the ID.
	clmobj_t *cmo;
	boolean linked = true;
	int df = 0;
	mobj_t *d;
	boolean justCreated = false;

	// Stop if end marker found.
	if(!id) return false;

	// Get a mobj for this.
	cmo = Cl_FindMobj(id);
	if(!cmo)
	{
		justCreated = true;

		// This is a new ID, allocate a new mobj.
		cmo = Z_Malloc(sizeof(*cmo), PU_LEVEL, 0);
		memset(cmo, 0, sizeof(*cmo));
		cmo->mo.ddflags |= DDMF_REMOTE;
		Cl_LinkMobj(cmo, id);
		P_SetMobjID(id, true); // Mark this ID as used.
		linked = false;
	}

	// This client mobj is alive.
	cmo->time = Sys_GetRealTime();

	// Flags.
	df = Msg_ReadShort();
	if(!df)
	{
		if(justCreated) //Con_Error("justCreated!\n");
			Con_Printf("CL_RMD: deleted justCreated id=%i\n", id);

		// A Null Delta. We should delete this mobj.
		if(cmo->mo.dplayer) 
		{
			playerstate[cmo->mo.dplayer - players].cmo = NULL;
		}
		Cl_DestroyMobj(cmo);
		return true; // Continue.
	}

#ifdef _DEBUG
	if(justCreated && (!(df & MDF_POS_X) || !(df & MDF_POS_Y)))
	{
		Con_Error("CL_ReadMobjDelta: Mobj is being created without X,Y.\n");
	}
#endif

	d = &cmo->mo;

	// Need to unlink? (Flags because DDMF_SOLID determines block-linking.)
	if(df & (MDF_POS_X | MDF_POS_Y | MDF_POS_Z | MDF_FLAGS) 
		&& linked
		&& !d->dplayer)
	{
		linked = false;
		Cl_UnsetThingPosition(cmo);
	}

	// Coordinates with three bytes.
	if(df & MDF_POS_X) 
		d->x = (Msg_ReadShort() << FRACBITS) | (Msg_ReadByte() << 8);
	if(df & MDF_POS_Y) 
		d->y = (Msg_ReadShort() << FRACBITS) | (Msg_ReadByte() << 8);
	if(df & MDF_POS_Z)
		d->z = (Msg_ReadShort() << FRACBITS) | (Msg_ReadByte() << 8);

#ifdef _DEBUG
	if(!d->x && !d->y)
	{
/*		if(d->type == 83)
		{
			d->type = d->type;
		}*/
		Con_Printf("CL_RMD: x,y zeroed t%i(%s)\n", d->type,
			defs.mobjs[d->type].id);
	}
#endif

	// Momentum using 8.8 fixed point.
	if(df & MDF_MOM_X) d->momx = (Msg_ReadShort() << 16)/256; //>> 8; 
	if(df & MDF_MOM_Y) d->momy = (Msg_ReadShort() << 16)/256; //>> 8; 
	if(df & MDF_MOM_Z) d->momz = (Msg_ReadShort() << 16)/256; //>> 8; 

	// Angles with 16-bit accuracy.
	if(df & MDF_ANGLE) d->angle = Msg_ReadShort() << 16;

	// MDF_SELSPEC is never used without MDF_SELECTOR.
	if(df & MDF_SELECTOR) d->selector = Msg_ReadPackedShort();
	if(df & MDF_SELSPEC) d->selector |= Msg_ReadByte() << 24;

	if(df & MDF_STATE) 
		Cl_SetThingState(d, (unsigned short) Msg_ReadPackedShort());

	// Pack flags into a word (3 bytes?).
	// FIXME: Do the packing!
	if(df & MDF_FLAGS) 
	{
		// Only the flags in the pack mask are affected.
		d->ddflags &= ~DDMF_PACK_MASK;
		d->ddflags |= DDMF_REMOTE | (Msg_ReadLong() & DDMF_PACK_MASK);
	}

	// Radius, height and floorclip are all bytes.
	if(df & MDF_RADIUS) d->radius = Msg_ReadByte() << FRACBITS;
	if(df & MDF_HEIGHT) d->height = Msg_ReadByte() << FRACBITS;
	if(df & MDF_FLOORCLIP) 
	{
		if(df & MDF_LONG_FLOORCLIP)
			d->floorclip = Msg_ReadPackedShort() << 14;
		else
			d->floorclip = Msg_ReadByte() << 14;
	}

	// Link again.
	if(!linked && !d->dplayer)	
		Cl_SetThingPosition(cmo);
	
	if(df & (MDF_POS_X | MDF_POS_Y | MDF_POS_Z))
	{
		// This'll update floorz and ceilingz.
		Cl_CheckMobj(d);
	}
	
	// Update players.
	if(d->dplayer) 
	{
		// Players have real mobjs. The client mobj is hidden (unlinked).
		Cl_UpdateRealPlayerMobj(d->dplayer->mo, d, df);
	}

	// Continue reading.
	return true;
}

/*
 * Initialize clientside data.
 */
void Cl_InitClientMobjs()
{
	previous_time = gametic;
	
	// List of client mobjs.
	memset(cmHash, 0, sizeof(cmHash));

	Cl_InitPlayers();
}

/*
 * Called when the client is shut down. Unlinks everything from the 
 * sectors and the blockmap and clears the clmobj list.
 */
void Cl_DestroyClientMobjs()
{
	clmobj_t *cmo;
	int i;

	for(i = 0; i < HASH_SIZE; i++)
	{
		for(cmo = cmHash[i].first; cmo; cmo = cmo->next)
		{ 
			// Players' clmobjs are not linked anywhere.
			if(!cmo->mo.dplayer)
				Cl_UnsetThingPosition(cmo);
		}
	}

	Cl_Reset();
}

/*
 * Reset the client status. Called when the level changes.
 */
void Cl_Reset(void)
{
	Cl_ResetFrame();

	// The PU_LEVEL memory was freed, so just clear the hash.
	memset(cmHash, 0, sizeof(cmHash));

	// Clear player data, too, since we just lost all clmobjs.
	Cl_InitPlayers();
}

/*
 * The client mobj is moved linearly, with collision checking.
 */
void Cl_MoveThing(clmobj_t *cmo)
{
	mobj_t *mo = &cmo->mo;
	boolean collided = false;

	// First do XY movement.
	if(mo->momx || mo->momy)
	{
		// Move while doing collision checking.
		if(!P_StepMove(mo, mo->momx, mo->momy, 0))
		{
			// There was a collision!
			collided = true;
		}
	}

	if(mo->momz)
	{
		mo->z += mo->momz;

		if(mo->z < mo->floorz)
		{
			mo->z = mo->floorz;
			mo->momz = 0;
			collided = true;
		}
		if(mo->z + mo->height > mo->ceilingz)
		{
			mo->z = mo->ceilingz - mo->height;
			mo->momz = 0;
			collided = true;
		}
	}
		
	if(mo->z > mo->floorz)
	{
		// Gravity will affect the prediction.
		if(mo->ddflags & DDMF_LOWGRAVITY)
		{
			if(mo->momz == 0)
				mo->momz = -(mapgravity >> 3)*2;
			else
				mo->momz -= mapgravity >> 3;
		}
		else if(!(mo->ddflags & DDMF_NOGRAVITY))
		{
			if(mo->momz == 0)
				mo->momz = -mapgravity*2;
			else
				mo->momz -= mapgravity;
		}
	}

	if(collided)
	{
		// Missiles are immediately removed. 
		// (An explosion should follow shortly.)
		if(mo->ddflags & DDMF_MISSILE)
		{
			// We don't know how to proceed from this, but merely
			// stopping the mobj is not enough. Let's hide it until
			// next delta arrives.
			cmo->flags |= CLMF_UNPREDICTABLE;
			Cl_UnsetThingPosition(cmo);
		}
		// [Kaboom!]
	}	

#if 0
	mobj_t *mo = &cmo->mo;
	boolean needlink = false;

	// Is the thing moving at all?
	if(!mobj->momx && !mobj->momy && !mobj->momz) 
	{
		// No movement; no need to do anything.
		return;
	}

	// Does it need to be un/linked?
	if(mobj->momx || mobj->momy) needlink = true;

	// Link out of lists if necessary.
	if(needlink) Cl_UnsetThingPosition(mobj);

	// Do the predicted move.
/*	if(pred_forward)
	{*/
		mobj->x += mobj->momx;
		mobj->y += mobj->momy;
		mobj->z += mobj->momz;
/*	}
	else // Backwards in time.
	{
		mobj->x -= mobj->momx;
		mobj->y -= mobj->momy;
		mobj->z -= mobj->momz;
	}*/
	
	// Link back to lists if necessary.
	if(needlink) Cl_SetThingPosition(mobj);
#endif
}

/*
 * Decrement tics counter and changes the state of the thing if necessary.
 */
void Cl_AnimateThing(mobj_t *mo)
{
	if(!mo->state || mo->tics < 0) return; // In stasis.

	mo->tics--;
	if(mo->tics <= 0)
	{
		// Go to next state, if possible.
		if(mo->state->nextstate >= 0)
		{
			Cl_SetThingState(mo, mo->state->nextstate);

			// Players have both client mobjs and regular mobjs. This
			// routine modifies the *client* mobj, so for players we need
			// to update the real, visible mobj as well.
			if(mo->dplayer) 
				Cl_UpdateRealPlayerMobj(mo->dplayer->mo, mo, 0);
		}
		else
		{
			// Freeze it; the server will probably to remove it soon.
			mo->tics = -1;
		}
	}
}

/*
 * All client mobjs are moved and animated using the data we have.  
 * 'forward' parameter is currently unused.
 */
void Cl_PredictMovement(void)
{
	clmobj_t *cmo, *next = NULL;
	int i;
	int moCount = 0;
	uint nowTime = Sys_GetRealTime();

	// Only predict up to a certain number of tics.
/*	if(predicted_tics && predicted_tics >= predict_tics) 
	{
		return;
	}*/

	predicted_tics++;

	// Move all client mobjs.
	for(i = 0; i < HASH_SIZE; i++)
	{
		for(cmo = cmHash[i].first; cmo; cmo = next)
		{
			next = cmo->next;
			moCount++;

			if(cmo->flags & (CLMF_UNPREDICTABLE | CLMF_HIDDEN))
			{
				// Has this mobj timed out?
				if(nowTime - cmo->time > CLMOBJ_TIMEOUT)
				{
					// Too long. The server will probably never send anything
					// for this mobj, so get rid of it. (Both unpredictable
					// and hidden mobjs are not visible or bl/seclinked.)
					Cl_DestroyMobj(cmo);
				}
				// We can't predict what Hidden and Unpredictable things do.
				// Must wait for the next delta to arrive. This mobj won't be 
				// visible until then.
				continue;		
			}

			// The local player gets a bit more complex prediction.
			if(cmo->mo.dplayer) 
			{
				Cl_MovePlayer(cmo->mo.dplayer);			
			}
			else 
			{
				// Linear movement prediction with collisions, then.
				Cl_MoveThing(cmo);
			}

			// Tic away.
			Cl_AnimateThing(&cmo->mo);

			// Remove mobjs who have reached the NULL state, from whose
			// bourn no traveller returns.
			if(cmo->mo.state == states)
			{
#ifdef _DEBUG
				if(!cmo->mo.thinker.id) Con_Error("No clmobj id!!!!\n");
#endif
				Cl_DestroyMobj(cmo);
				continue;
			}

			// Update the visual angle of the mobj (no SRVO action).
			cmo->mo.visangle = cmo->mo.angle >> 16;
		}
	}
#ifdef _DEBUG
	{
		static timer = 0;
		if(++timer > 5*35)
		{
			timer = 0;
			Con_Printf("moCount=%i\n", moCount);
		}
	}
#endif
}

/*
 * Create a new client mobj.
 */
clmobj_t *Cl_CreateMobj(thid_t id)
{
	clmobj_t *cmo = Z_Calloc(sizeof(*cmo), PU_LEVEL, 0);

	cmo->mo.ddflags |= DDMF_REMOTE;
	cmo->time = Sys_GetRealTime();
	Cl_LinkMobj(cmo, id);
	P_SetMobjID(id, true); // Mark this ID as used.

	return cmo;
}

/*
 * Destroys the client mobj.
 */
void Cl_DestroyMobj(clmobj_t *cmo)
{
	// Stop any sounds originating from this mobj.
	S_StopSound(0, &cmo->mo);

	// The ID is free once again.
	P_SetMobjID(cmo->mo.thinker.id, false);
	Cl_UnsetThingPosition(cmo);
	Cl_UnlinkMobj(cmo);
	Z_Free(cmo);
}

/*
 * Call for Hidden client mobjs to make then visible.
 * If a sound is waiting, it's now played.
 */
void Cl_RevealMobj(clmobj_t *cmo)
{
#ifdef _DEBUG
	Con_Printf("Cl_RMD2: Mo %i Hidden status lifted.\n", cmo->mo.thinker.id);
#endif

	cmo->flags &= ~CLMF_HIDDEN;

	// Start a sound that has been queued for playing at the time
	// of unhiding. Sounds are queued if a sound delta arrives for an
	// object ID we don't know (yet).
	if(cmo->flags & CLMF_SOUND)
	{
		cmo->flags &= ~CLMF_SOUND;
		S_StartSoundAtVolume(cmo->sound, &cmo->mo, cmo->volume);
	}
}

/*
 * Reads a single mobj psv_frame2 delta from the message buffer and 
 * applies it to the client mobj in question.
 *
 * For client mobjs that belong to players, updates the real player mobj.
 */
void Cl_ReadMobjDelta2(boolean allowCreate)
{
	boolean linked = true, justCreated = false;
	clmobj_t *cmo;
	mobj_t *d;
	int df = 0;
	byte moreFlags = 0, fastMom = false;
	short mom;
	thid_t id = Msg_ReadShort();	// Read the ID.

	// Get a mobj for this.
	cmo = Cl_FindMobj(id);
	if(!cmo)
	{
		// This is a new ID, allocate a new mobj.
		cmo = Cl_CreateMobj(id);
		justCreated = true;
		linked = false;

		if(!allowCreate)
		{
			// This clmobj should be hidden until the Create mobj
			// delta arrives. (created, but not linked to the world)
			cmo->flags |= CLMF_HIDDEN;		

#ifdef _DEBUG
			Con_Printf("Cl_RMD2: Mobj %i created as Hidden.\n", id);
#endif
		}
	}

	if(!(cmo->flags & CLMF_NULLED))
	{
		// Now that we've received a delta, the mobj's Predictable again.
		cmo->flags &= ~CLMF_UNPREDICTABLE;

		// This clmobj is evidently alive.
		cmo->time = Sys_GetRealTime();
	}

	// Flags.
	df = Msg_ReadShort();
#ifdef _DEBUG
	if(!df)
	{	
		Con_Error("Cl_RMD2: Mobj %i delta is void.\n", id);
	}
/*	if(justCreated && (!(df & MDF_POS_X) || !(df & MDF_POS_Y)))
	{
		Con_Printf("Cl_ReadMobjDelta2: Mo %i created without X/Y.\n", id);
	}*/
#endif
	
	// More flags?
	if(df & MDF_MORE_FLAGS)
	{
		moreFlags = Msg_ReadByte();

		// Fast momentum uses 10.6 fixed point instead of the normal 8.8.
		if(moreFlags & MDFE_FAST_MOM) fastMom = true;
	}

	d = &cmo->mo;

	// Need to unlink? (Flags because DDMF_SOLID determines block-linking.)
	if(df & (MDF_POS_X | MDF_POS_Y | MDF_POS_Z | MDF_FLAGS) 
		&& linked
		&& !d->dplayer)
	{
		linked = false;
		Cl_UnsetThingPosition(cmo);
	}

	// Coordinates with three bytes.
	if(df & MDF_POS_X) 
	{
		d->x = (Msg_ReadShort() << FRACBITS) | (Msg_ReadByte() << 8);
	}
	if(df & MDF_POS_Y) 
	{
		d->y = (Msg_ReadShort() << FRACBITS) | (Msg_ReadByte() << 8);
	}
	if(df & MDF_POS_Z)
	{
		d->z = (Msg_ReadShort() << FRACBITS) | (Msg_ReadByte() << 8);
	}

	// When these flags are set, the normal Z coord is not included.
	if(moreFlags & MDFE_Z_FLOOR)
	{
		d->z = DDMININT;
	}
	if(moreFlags & MDFE_Z_CEILING)
	{
		d->z = DDMAXINT;
	}

#ifdef _DEBUG
	if(!d->x && !d->y)
	{
		/*Con_Printf("Cl_RMD2: %i: x,y zeroed t%i(%s)\n", id, d->type,
			defs.mobjs[d->type].id);*/
	}
#endif

	// Momentum using 8.8 fixed point.
	if(df & MDF_MOM_X) 
	{
		mom = Msg_ReadShort();
		d->momx = (fastMom? UNFIXED10_6(mom) : UNFIXED8_8(mom));
	}
	if(df & MDF_MOM_Y) 
	{
		mom = Msg_ReadShort();
		d->momy = (fastMom? UNFIXED10_6(mom) : UNFIXED8_8(mom));
	}
	if(df & MDF_MOM_Z) 
	{
		mom = Msg_ReadShort();
		d->momz = (fastMom? UNFIXED10_6(mom) : UNFIXED8_8(mom));
	}

	// Angles with 16-bit accuracy.
	if(df & MDF_ANGLE) d->angle = Msg_ReadShort() << 16;

	// MDF_SELSPEC is never used without MDF_SELECTOR.
	if(df & MDF_SELECTOR) d->selector = Msg_ReadPackedShort();
	if(df & MDF_SELSPEC) d->selector |= Msg_ReadByte() << 24;

	if(df & MDF_STATE) 
		Cl_SetThingState(d, (unsigned short) Msg_ReadPackedShort());

	// Pack flags into a word (3 bytes?).
	// FIXME: Do the packing!
	if(df & MDF_FLAGS) 
	{
		// Only the flags in the pack mask are affected.
		d->ddflags &= ~DDMF_PACK_MASK;
		d->ddflags |= DDMF_REMOTE | (Msg_ReadLong() & DDMF_PACK_MASK);
	}

	// Radius, height and floorclip are all bytes.
	if(df & MDF_RADIUS) d->radius = Msg_ReadByte() << FRACBITS;
	if(df & MDF_HEIGHT) d->height = Msg_ReadByte() << FRACBITS;
	if(df & MDF_FLOORCLIP) 
	{
		if(df & MDF_LONG_FLOORCLIP)
			d->floorclip = Msg_ReadPackedShort() << 14;
		else
			d->floorclip = Msg_ReadByte() << 14;
	}

	if(moreFlags & MDFE_TRANSLUCENCY) d->translucency = Msg_ReadByte();

	// Is it time to remove the Hidden status?
	if(allowCreate && cmo->flags & CLMF_HIDDEN)
	{
		// Now it can be displayed.
		Cl_RevealMobj(cmo);
	}

	// If the clmobj is Hidden, it will not be linked back to the world
	// until it's officially Created. (Otherwise, partially updated mobjs
	// may be visible for a while.)
	if(!(cmo->flags & CLMF_HIDDEN))
	{
		// Link again.
		if(!linked && !d->dplayer)	
		{
			Cl_SetThingPosition(cmo);
		}
		
		if(df & (MDF_POS_X | MDF_POS_Y | MDF_POS_Z)
			|| moreFlags & (MDFE_Z_FLOOR | MDFE_Z_CEILING))
		{
			// This'll update floorz and ceilingz.
			Cl_CheckMobj(d);
		}
		
		// Update players.
		if(d->dplayer) 
		{
			// Players have real mobjs. The client mobj is hidden (unlinked).
			Cl_UpdateRealPlayerMobj(d->dplayer->mo, d, df);
		}
	}
}

/*
 * Null mobjs deltas have their own type in a psv_frame2 packet.
 * Here we remove the mobj in question.
 */
void Cl_ReadNullMobjDelta2(void)
{
	clmobj_t *cmo;
	thid_t id;

	// The delta only contains an ID.
	id = Msg_ReadShort();

#ifdef _DEBUG
	Con_Printf("Cl_ReadNullMobjDelta2: Null %i\n", id);
#endif

	cmo = Cl_FindMobj(id);
/*#ifdef _DEBUG
	if(!cmo)
	{
		Con_Error("Cl_ReadNullMobjDelta2: Null %i, doesn't exist!\n", id);
	}
#endif*/
	if(!cmo) 
	{
		// Wasted bandwidth...
		return;
	}

	// Get rid of this mobj.
	if(!cmo->mo.dplayer) 
	{
		Cl_UnsetThingPosition(cmo);
	}
	else
	{
		// The clmobjs of players aren't linked.
		playerstate[cmo->mo.dplayer - players].cmo = NULL;
	}
	
	// Cl_DestroyMobj(cmo);

	// This'll allow playing sounds from the mobj for a little while.
	// The mobj will soon time out and be permanently removed.
	cmo->time = Sys_GetRealTime();
	cmo->flags |= CLMF_UNPREDICTABLE | CLMF_NULLED;
}
