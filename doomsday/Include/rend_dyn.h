#ifndef __DOOMSDAY_DYNLIGHTS_H__
#define __DOOMSDAY_DYNLIGHTS_H__

#include "rend_list.h"

#define DYN_ASPECT		1.1f			// 1.2f is just too round for Doom.

// Lumobj Flags.
#define LUMF_USED		0x1
#define LUMF_RENDERED	0x2
#define LUMF_CLIPPED	0x4				// Hidden by world geometry.
#define LUMF_NOHALO		0x100

typedef struct lumobj_s					// For dynamic lighting.
{
	struct lumobj_s *next;				// Next in the same DL block, or NULL.
	struct lumobj_s *ssnext;			// Next in the same subsector, or NULL.
	int		flags;
	mobj_t	*thing;
	float	center; 					// Offset to center from mobj Z.
	int		radius, patch, distance;	// Radius: lights are spheres.
	int		flaresize;					// Radius for this light source.
	byte	rgb[3];						// The color.
	float	xoff;
	float	xyscale;					// 1.0 if there's no modeldef.
	DGLuint	tex;						// Lightmap texture.
	DGLuint	floortex, ceiltex;			// Lightmaps for floor/ceil.
	char	flaretex;					// Zero = automatical.
} 
lumobj_t;

/*
 * The data of a projected dynamic light is stored in this structure.
 * A list of these is associated with all the lit segs/planes in a frame.
 */
typedef struct dynlight_s
{
	struct dynlight_s *next, *nextused;
	int		flags;
	float	s[2], t[2];
	byte	color[3];
	DGLuint texture;
	uint	index;
}
dynlight_t;

// Flags for projected dynamic lights.
#define DYNF_REPEAT_S	0x1
#define DYNF_REPEAT_T	0x2

extern boolean	dlInited;
extern lumobj_t	*luminousList;
extern lumobj_t	**dlSubLinks;
extern int		numLuminous;
extern int		useDynLights;
extern int		maxDynLights, dlBlend, clipLights, dlMaxRad;
extern float	dlRadFactor, dlFactor;//, dlContract;
extern int		useWallGlow, glowHeight;
extern int		rend_info_lums;

extern dynlight_t **floorLightLinks;
extern dynlight_t **ceilingLightLinks;

// Setup.
void DL_InitLinks();
void DL_Clear();	// 'Physically' destroy the tables.

// Action.
void DL_ClearForFrame();
void DL_InitForNewFrame();
int DL_NewLuminous(void);
lumobj_t *DL_GetLuminous(int index);
void DL_ProcessSubsector(subsector_t *ssec);
void DL_ProcessWallSeg(lumobj_t *lum, seg_t *seg, sector_t *frontsector);
dynlight_t *DL_GetSegLightLinks(int seg, int whichpart);

// Helpers.
boolean DL_RadiusIterator(fixed_t x, fixed_t y, fixed_t radius, boolean (*func)(lumobj_t*,fixed_t));
boolean DL_BoxIterator(fixed_t box[4], void *ptr, boolean (*func)(lumobj_t*,void*));

int Rend_SubsectorClipper(fvertex_t *out, subsector_t *sub, float x, float y, float radius);

#endif