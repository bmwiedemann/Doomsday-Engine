/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2007 Daniel Swanson <danij@dengine.net>
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
 * h_refresh.c: - jHeretic specific.
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "jheretic.h"

#include "hu_stuff.h"
#include "hu_pspr.h"
#include "am_map.h"
#include "g_common.h"
#include "r_common.h"
#include "d_net.h"
#include "f_infine.h"
#include "x_hair.h"
#include "g_controls.h"
#include "p_mapsetup.h"

// MACROS ------------------------------------------------------------------

#define viewheight          Get(DD_VIEWWINDOW_HEIGHT)
#define SIZEFACT            4
#define SIZEFACT2           16

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void    R_SetAllDoomsdayFlags(void);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern boolean finalestage;

extern float lookOffset;

extern const float deffontRGB[];
extern const float deffontRGB2[];

// PUBLIC DATA DEFINITIONS -------------------------------------------------

player_t *viewplayer;

gamestate_t wipegamestate = GS_DEMOSCREEN;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * Creates the translation tables to map the green color ramp to gray,
 * brown, red.
 *
 * \note Assumes a given structure of the PLAYPAL. Could be read from a
 * lump instead.
 */
static void initTranslation(void)
{
    int         i;
    byte       *translationtables = (byte *)
                    DD_GetVariable(DD_TRANSLATIONTABLES_ADDRESS);

    // Fill out the translation tables.
    for(i = 0; i < 256; ++i)
    {
        if(i >= 225 && i <= 240)
        {
            translationtables[i] = 114 + (i - 225); // yellow
            translationtables[i + 256] = 145 + (i - 225); // red
            translationtables[i + 512] = 190 + (i - 225); // blue
        }
        else
        {
            translationtables[i] = translationtables[i + 256] =
                translationtables[i + 512] = i;
        }
    }
}

void R_Init(void)
{
    initTranslation();
}

/**
 * Draws a special filter over the screen.
 */
void R_DrawSpecialFilter(void)
{
    float       x, y, w, h;
    player_t   *player = &players[displayplayer];

    if(player->powers[PT_INVULNERABILITY] <= BLINKTHRESHOLD &&
       !(player->powers[PT_INVULNERABILITY] & 8))
        return;

    R_GetViewWindow(&x, &y, &w, &h);
    gl.Disable(DGL_TEXTURING);
    if(cfg.ringFilter == 1)
    {
        gl.Func(DGL_BLENDING, DGL_SRC_COLOR, DGL_SRC_COLOR);
        GL_DrawRect(x, y, w, h, .5f, .35f, .1f, 1);

        /*gl.Func(DGL_BLENDING, DGL_ZERO, DGL_SRC_COLOR);
           GL_DrawRect(x, y, w, h, 1, .7f, 0, 1);
           gl.Func(DGL_BLENDING, DGL_ONE, DGL_DST_COLOR);
           GL_DrawRect(x, y, w, h, .1f, 0, 0, 1); */

        /*gl.Func(DGL_BLENDING, DGL_ZERO, DGL_SRC_COLOR);
           GL_DrawRect(x, y, w, h, 0, .6f, 0, 1);
           gl.Func(DGL_BLENDING, DGL_ONE_MINUS_DST_COLOR, DGL_ONE_MINUS_SRC_COLOR);
           GL_DrawRect(x, y, w, h, 1, 0, 0, 1); */
    }
    else
    {
        gl.Func(DGL_BLENDING, DGL_DST_COLOR, DGL_SRC_COLOR);
        GL_DrawRect(x, y, w, h, 0, 0, .6f, 1);
    }

    // Restore the normal rendering state.
    gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE_MINUS_SRC_ALPHA);
    gl.Enable(DGL_TEXTURING);
}

void R_DrawLevelTitle(int x, int y, float alpha, dpatch_t *font,
                      boolean center)
{
    int         strX;
    char       *lname, *lauthor;

    lname = P_GetMapNiceName();
    if(lname)
    {
        strX = x;
        if(center)
            strX -= M_StringWidth(lname, font) / 2;

        M_WriteText3(strX, y, lname, font,
                     deffontRGB[0], deffontRGB[1], deffontRGB[2], alpha,
                     false, 0);
        y += 20;
    }

    lauthor = (char *) DD_GetVariable(DD_MAP_AUTHOR);
    if(lauthor && stricmp(lauthor, "raven software"))
    {
        strX = x;
        if(center)
            strX -= M_StringWidth(lauthor, hu_font_a) / 2;

        M_WriteText3(strX, y, lauthor, hu_font_a, .5f, .5f, .5f, alpha,
                     false, 0);
    }
}

/**
 * Do not really change anything here, because Doomsday might be in the
 * middle of a refresh. The change will take effect next refresh.
 */
void R_SetViewSize(int blocks, int detail)
{
    cfg.setsizeneeded = true;
    if(cfg.setblocks != blocks && blocks > 10 && blocks < 13)
    {   // When going fullscreen, force a hud show event (to reset the timer).
        ST_HUDUnHide(HUE_FORCE);
    }
    cfg.setblocks = blocks;
}

void D_Display(void)
{
    static boolean viewactivestate = false;
    static boolean menuactivestate = false;
    static gamestate_t oldgamestate = -1;
    player_t   *vplayer = &players[displayplayer];
    boolean     iscam = (vplayer->plr->flags & DDPF_CAMERA) != 0; // $democam
    float       x, y, w, h;
    boolean     mapHidesView;

    // $democam: can be set on every frame
    if(cfg.setblocks > 10 || iscam)
    {
        // Full screen.
        R_SetViewWindowTarget(0, 0, 320, 200);
    }
    else
    {
        int w = cfg.setblocks * 32;
        int h = cfg.setblocks * (200 - SBARHEIGHT * cfg.sbarscale / 20) / 10;
        R_SetViewWindowTarget(160 - (w / 2),
                              (200 - SBARHEIGHT * cfg.sbarscale / 20 - h) / 2,
                              w, h);
    }

    R_GetViewWindow(&x, &y, &w, &h);
    R_ViewWindow((int) x, (int) y, (int) w, (int) h);

    switch(G_GetGameState())
    {
    case GS_LEVEL:
        if(IS_CLIENT && (!Get(DD_GAME_READY) || !Get(DD_GOTFRAME)))
            break;

        if(!IS_CLIENT && leveltime < 2)
        {
            // Don't render too early; the first couple of frames
            // might be a bit unstable -- this should be considered
            // a bug, but since there's an easy fix...
            break;
        }

        mapHidesView =
            R_MapObscures(displayplayer, (int) x, (int) y, (int) w, (int) h);

        if(!(MN_CurrentMenuHasBackground() && MN_MenuAlpha() >= 1) &&
           !mapHidesView)
        {   // Draw the player view.
            int         viewAngleOffset =
                ANGLE_MAX * -G_GetLookOffset(displayplayer);
            boolean     isFullbright =
                (vplayer->powers[PT_INVULNERABILITY] > BLINKTHRESHOLD) ||
                               (vplayer->powers[PT_INVULNERABILITY] & 8);

            if(IS_CLIENT)
            {
                // Server updates mobj flags in NetSv_Ticker.
                R_SetAllDoomsdayFlags();
            }

            // The view angle offset.
            DD_SetVariable(DD_VIEWANGLE_OFFSET, &viewAngleOffset);
            GL_SetFilter(vplayer->plr->filter);

            // How about fullbright?
            Set(DD_FULLBRIGHT, isFullbright);

            // Render the view with possible custom filters.
            R_RenderPlayerView(vplayer->plr);

            R_DrawSpecialFilter();

            // Crosshair.
            if(!iscam)
                X_Drawer();
        }

        // Draw the automap?
        AM_Drawer(displayplayer);
        break;

    default:
        break;
    }

    menuactivestate = menuactive;
    viewactivestate = viewactive;
    oldgamestate = wipegamestate = G_GetGameState();
}

void D_Display2(void)
{
    switch(G_GetGameState())
    {
    case GS_LEVEL:
        if(IS_CLIENT && (!Get(DD_GAME_READY) || !Get(DD_GOTFRAME)))
            break;

        if(!IS_CLIENT && leveltime < 2)
        {
            // Don't render too early; the first couple of frames
            // might be a bit unstable -- this should be considered
            // a bug, but since there's an easy fix...
            break;
        }

        // These various HUD's will be drawn unless Doomsday advises not to.
        if(DD_GetInteger(DD_GAME_DRAW_HUD_HINT))
        {
            boolean     redrawsbar = false;

            // Draw HUD displays only visible when the automap is open.
            if(AM_IsMapActive(displayplayer))
                HU_DrawMapCounters();

            // Level information is shown for a few seconds in the
            // beginning of a level.
            if(cfg.levelTitle || actual_leveltime <= 6 * TICSPERSEC)
            {
                int         x, y;
                float       alpha = 1;

                if(actual_leveltime < 35)
                    alpha = actual_leveltime / 35.0f;
                if(actual_leveltime > 5 * 35)
                    alpha = 1 - (actual_leveltime - 5 * 35) / 35.0f;

                x = SCREENWIDTH / 2;
                y = 13;
                Draw_BeginZoom((1 + cfg.hudScale)/2, x, y);
                R_DrawLevelTitle(x, y, alpha, hu_font_b, true);
                Draw_EndZoom();
            }

            if((viewheight != 200))
                redrawsbar = true;

            // Do we need to render a full status bar at this point?
            if(!(AM_IsMapActive(displayplayer) && cfg.automapHudDisplay == 0))
            {
                player_t   *player = &players[displayplayer];
                boolean     iscam = (player->plr->flags & DDPF_CAMERA) != 0; // $democam

                if(!iscam)
                {
                    int         viewmode =
                        ((viewheight == 200)? (cfg.setblocks - 10) : 0);

                    ST_Drawer(viewmode, redrawsbar); // $democam
                }
            }

            HU_Drawer();
        }
        break;

    case GS_INTERMISSION:
        IN_Drawer();
        break;

    case GS_WAITING:
        // Clear the screen while waiting, doesn't mess up the menu.
        gl.Clear(DGL_COLOR_BUFFER_BIT);
        break;

    default:
        break;
    }

    // Draw pause pic (but not if InFine active).
    if(paused && !fi_active)
    {
        GL_DrawPatch(160, 4, W_GetNumForName("PAUSED"));
    }

    // InFine is drawn whenever active.
    FI_Drawer();

    // The menu is drawn whenever active.
    M_Drawer();
}

/**
 * Updates the mobj flags used by Doomsday with the state of our local flags
 * for the given mobj.
 */
void R_SetDoomsdayFlags(mobj_t *mo)
{
    // Client mobjs can't be set here.
    if(IS_CLIENT && mo->ddflags & DDMF_REMOTE)
        return;

    // Reset the flags for a new frame.
    mo->ddflags &= DDMF_CLEAR_MASK;

    // Local objects aren't sent to clients.
    if(mo->flags & MF_LOCAL)
        mo->ddflags |= DDMF_LOCAL;
    if(mo->flags & MF_SOLID)
        mo->ddflags |= DDMF_SOLID;
    if(mo->flags & MF_NOGRAVITY)
        mo->ddflags |= DDMF_NOGRAVITY;
    if(mo->flags2 & MF2_FLOATBOB)
        mo->ddflags |= DDMF_NOGRAVITY | DDMF_BOB;
    if(mo->flags & MF_MISSILE)
    {   // Mace death balls are controlled by the server.
        //if(mo->type != MT_MACEFX4)
        mo->ddflags |= DDMF_MISSILE;
    }
    if(mo->info && (mo->info->flags2 & MF2_ALWAYSLIT))
        mo->ddflags |= DDMF_ALWAYSLIT;

    if(mo->flags2 & MF2_FLY)
        mo->ddflags |= DDMF_FLY | DDMF_NOGRAVITY;

    // $democam: cameramen are invisible.
    if(P_IsCamera(mo))
        mo->ddflags |= DDMF_DONTDRAW;

    if((mo->flags & MF_CORPSE) && cfg.corpseTime && mo->corpsetics == -1)
        mo->ddflags |= DDMF_DONTDRAW;

    // Choose which ddflags to set.
    if(mo->flags2 & MF2_DONTDRAW)
    {
        mo->ddflags |= DDMF_DONTDRAW;
        return; // No point in checking the other flags.
    }

    if(mo->flags2 & MF2_LOGRAV)
        mo->ddflags |= DDMF_LOWGRAVITY;

    if(mo->flags & MF_BRIGHTSHADOW)
        mo->ddflags |= DDMF_BRIGHTSHADOW;
    else if(mo->flags & MF_SHADOW)
        mo->ddflags |= DDMF_ALTSHADOW;

    if(((mo->flags & MF_VIEWALIGN) && !(mo->flags & MF_MISSILE)) ||
       (mo->flags & MF_FLOAT) ||
       ((mo->flags & MF_MISSILE) && !(mo->flags & MF_VIEWALIGN)))
        mo->ddflags |= DDMF_VIEWALIGN;

    mo->ddflags |= (mo->flags & MF_TRANSLATION);
}

/**
 * Updates the status flags for all visible things.
 */
void R_SetAllDoomsdayFlags(void)
{
    uint        i;
    mobj_t     *iter;

    // Only visible things are in the sector thinglists, so this is good.
    for(i = 0; i < numsectors; ++i)
    {
        iter = P_GetPtr(DMU_SECTOR, i, DMT_MOBJS);

        while(iter)
        {
            R_SetDoomsdayFlags(iter);
            iter = iter->snext;
        }
    }
}
