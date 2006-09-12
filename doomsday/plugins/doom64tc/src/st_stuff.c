/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2003-2005 Samuel Villarreal <svkaiser@gmail.com>
 *\author Copyright © 1993-1996 by id Software, Inc.
 *
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

/*
 * Status bar code.
 *  Does the face/direction indicator animatin.
 *  Does palette indicators as well (red pain/berserk, bright pickup)
 */

 // HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>

#include "doom64tc.h"

#include "d_net.h"
#include "st_lib.h"
#include "hu_stuff.h"

// MACROS ------------------------------------------------------------------

#define FMAKERGBA(r,g,b,a) ( (byte)(0xff*r) + ((byte)(0xff*g)<<8)           \
                            + ((byte)(0xff*b)<<16) + ((byte)(0xff*a)<<24) )

// Radiation suit, green shift.
#define RADIATIONPAL        13

// Frags pos.
#define ST_FRAGSX           138
#define ST_FRAGSY           171
#define ST_FRAGSWIDTH       2

// TYPES -------------------------------------------------------------------

typedef enum hotloc_e {
    HOT_TLEFT,
    HOT_TRIGHT,
    HOT_BRIGHT,
    HOT_BLEFT
} hotloc_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void    ST_Stop(void);

DEFCC(CCmdHUDShow);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern boolean hu_showallfrags; // in hu_stuff.c currently

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int hudHideTics;
static float hudHideAmount;

// fullscreen hud alpha value
static float hudalpha = 0.0f;
// whether to use alpha blending
static boolean st_blended = false;

// used to execute ST_Init() only once
static int veryfirsttime = 1;

// lump number for PLAYPAL
static int lu_palette;

// used for timing
static unsigned int st_clock;

// used for making messages go away
static int st_msgcounter = 0;

// used when in chat
static st_chatstateenum_t st_chatstate;

// whether in automap or first-person
static st_stateenum_t st_gamestate;

// whether left-side main status bar is active
static boolean st_statusbaron;

// whether status bar chat is active
static boolean st_chat;

// value of st_chat before message popped up
static boolean st_oldchat;

// whether chat window has the cursor on
static boolean st_cursoron;

// !deathmatch
static boolean st_notdeathmatch;

static boolean st_firsttime;

// 0-9, tall numbers
static dpatch_t tallnum[10];

// tall % sign
static dpatch_t tallpercent;

// 0-9, short, yellow (,different!) numbers
static dpatch_t shortnum[10];

// 3 key-cards, 3 skulls
static dpatch_t keys[NUMKEYS];

 // in deathmatch only, summary of frags stats
static st_number_t w_frags;

 // number of frags so far in deathmatch
static int st_fragscount;

// used to use appopriately pained face
static int st_oldhealth = -1;

static boolean st_fragson;

// used for evil grin
static boolean oldweaponsowned[NUMWEAPONS];

// holds key-type for each key box on bar
static int keyboxes[3];

// a random number per tick
static int st_randomnumber;

static boolean st_stopped = true;
static int st_palette = 0;

// CVARs for the HUD/Statusbar
cvar_t hudCVars[] =
{
    // HUD scale
    {"hud-scale", 0, CVT_FLOAT, &cfg.hudScale, 0.1f, 10},

    {"hud-status-size", CVF_PROTECTED, CVT_INT, &cfg.sbarscale, 1, 20},

    // HUD colour + alpha
    {"hud-color-r", 0, CVT_FLOAT, &cfg.hudColor[0], 0, 1},
    {"hud-color-g", 0, CVT_FLOAT, &cfg.hudColor[1], 0, 1},
    {"hud-color-b", 0, CVT_FLOAT, &cfg.hudColor[2], 0, 1},
    {"hud-color-a", 0, CVT_FLOAT, &cfg.hudColor[3], 0, 1},
    {"hud-icon-alpha", 0, CVT_FLOAT, &cfg.hudIconAlpha, 0, 1},

    // HUD icons
    {"hud-health", 0, CVT_BYTE, &cfg.hudShown[HUD_HEALTH], 0, 1},
    {"hud-armor", 0, CVT_BYTE, &cfg.hudShown[HUD_ARMOR], 0, 1},
    {"hud-ammo", 0, CVT_BYTE, &cfg.hudShown[HUD_AMMO], 0, 1},
    {"hud-keys", 0, CVT_BYTE, &cfg.hudShown[HUD_KEYS], 0, 1},
    {"hud-power", 0, CVT_BYTE, &cfg.hudShown[HUD_POWER], 0, 1},

    // HUD displays
    {"hud-frags", 0, CVT_BYTE, &cfg.hudShown[HUD_FRAGS], 0, 1},

    {"hud-frags-all", 0, CVT_BYTE, &hu_showallfrags, 0, 1},

    {"hud-timer", 0, CVT_FLOAT, &cfg.hudTimer, 0, 60},

    {"hud-unhide-damage", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_DAMAGE], 0, 1},
    {"hud-unhide-pickup-health", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_HEALTH], 0, 1},
    {"hud-unhide-pickup-armor", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_ARMOR], 0, 1},
    {"hud-unhide-pickup-powerup", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_POWER], 0, 1},
    {"hud-unhide-pickup-weapon", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_WEAPON], 0, 1},
    {"hud-unhide-pickup-ammo", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_AMMO], 0, 1},
    {"hud-unhide-pickup-key", 0, CVT_BYTE, &cfg.hudUnHide[HUE_ON_PICKUP_KEY], 0, 1},
    {NULL}
};

// Console commands for the HUD/Status bar
ccmd_t  hudCCmds[] = {
    {"showhud",     CCmdHUDShow},
    {NULL}
};

// CODE --------------------------------------------------------------------

/*
 * Register CVARs and CCmds for the HUD/Status bar
 */
void ST_Register(void)
{
    int     i;

    for(i = 0; hudCVars[i].name; i++)
        Con_AddVariable(hudCVars + i);
    for(i = 0; hudCCmds[i].name; i++)
        Con_AddCommand(hudCCmds + i);
}

/*
 * Unhides the current HUD display if hidden.
 *
 * @param event         The HUD Update Event type to check for triggering.
 */
void ST_HUDUnHide(hueevent_t event)
{
    if(event < HUE_FORCE || event > NUMHUDUNHIDEEVENTS)
        return;

    if(event == HUE_FORCE || cfg.hudUnHide[event])
    {
        hudHideTics = (cfg.hudTimer * TICSPERSEC);
        hudHideAmount = 0;
    }
}


void ST_updateWidgets(void)
{
    static int largeammo = 1994;    // means "n/a"
    int     i;
    player_t *plr = &players[consoleplayer];

    // update keycard multiple widgets
    for(i = 0; i < 3; i++)
    {
        keyboxes[i] = plr->keys[i] ? i : -1;

        if(plr->keys[i + 3])
            keyboxes[i] = i + 3;
    }

    // used by the w_armsbg widget
    st_notdeathmatch = !deathmatch;

    // used by w_frags widget
    st_fragson = deathmatch && st_statusbaron;
    st_fragscount = 0;

    for(i = 0; i < MAXPLAYERS; i++)
    {
        if(!players[i].plr->ingame)
            continue;

        st_fragscount += plr->frags[i] * (i != consoleplayer ? 1 : -1);
    }

    // get rid of chat window if up because of message
    if(!--st_msgcounter)
        st_chat = st_oldchat;

}

void ST_Ticker(void)
{
    player_t *plyr = &players[consoleplayer];

    if(cfg.hudTimer == 0)
    {
        hudHideTics = hudHideAmount = 0;
    }
    else
    {
        if(hudHideTics > 0)
            hudHideTics--;
        if(hudHideTics == 0 && cfg.hudTimer > 0 && hudHideAmount < 1)
            hudHideAmount += 0.1f;
    }

    st_clock++;
    st_randomnumber = M_Random();
    ST_updateWidgets();
    st_oldhealth = plyr->health;
}

int R_GetFilterColor(int filter)
{
    int     rgba = 0;

    // We have to choose the right color and alpha.
    if(filter >= STARTREDPALS && filter < STARTREDPALS + NUMREDPALS)
        // Red.
        rgba = FMAKERGBA(1, 0, 0, filter / 9.0);
    else if(filter >= STARTBONUSPALS && filter < STARTBONUSPALS + NUMBONUSPALS)
        // Gold.
        rgba = FMAKERGBA(1, .8, .5, (filter - STARTBONUSPALS + 1) / 16.0);
    else if(filter == RADIATIONPAL)
        // Green.
        rgba = FMAKERGBA(0, .7, 0, .15f);
    else if(filter)
        Con_Error("R_GetFilterColor: Real strange filter number: %d.\n", filter);
    return rgba;
}

void ST_doPaletteStuff(void)
{
    int     palette;
    int     cnt;
    int     bzc;
    player_t *plyr = &players[consoleplayer];

    cnt = plyr->damagecount;

    if(plyr->powers[pw_strength])
    {
        // slowly fade the berzerk out
        bzc = 12 - (plyr->powers[pw_strength] >> 6);

        if(bzc > cnt)
            cnt = bzc;
    }

    if(cnt)
    {
        palette = (cnt + 7) >> 3;

        if(palette >= NUMREDPALS)
            palette = NUMREDPALS - 1;

        palette += STARTREDPALS;
    }

    else if(plyr->bonuscount)
    {
        palette = (plyr->bonuscount + 7) >> 3;

        if(palette >= NUMBONUSPALS)
            palette = NUMBONUSPALS - 1;

        palette += STARTBONUSPALS;
    }

    else if(plyr->powers[pw_ironfeet] > 4 * 32 ||
            plyr->powers[pw_ironfeet] & 8)
        palette = RADIATIONPAL;
    else
        palette = 0;

    if(palette != st_palette)
    {
        st_palette = palette;
        plyr->plr->filter = R_GetFilterColor(palette);  // $democam
    }
}

void ST_drawWidgets(boolean refresh)
{
    // used by w_frags widget
    st_fragson = deathmatch && st_statusbaron;

    STlib_updateNum(&w_frags, refresh);

}

void ST_doRefresh(void)
{
    st_firsttime = false;

    // and refresh all widgets
    ST_drawWidgets(true);
}

void ST_HUDSpriteSize(int sprite, int *w, int *h)
{
    spriteinfo_t sprInfo;

    R_GetSpriteInfo(sprite, 0, &sprInfo);
    *w = sprInfo.width;
    *h = sprInfo.height;

    if(sprite == SPR_ROCK)
    {
        // Must scale it a bit.
        *w /= 1.5;
        *h /= 1.5;
    }
}

void ST_drawHUDSprite(int sprite, int x, int y, int hotspot, float alpha)
{
    spriteinfo_t sprInfo;
    int     w, h;

    CLAMP(alpha, 0.0f, 1.0f);
    if(alpha == 0.0f)
        return;

    R_GetSpriteInfo(sprite, 0, &sprInfo);
    w = sprInfo.width;
    h = sprInfo.height;
    if(sprite == SPR_ROCK)
    {
        w /= 1.5;
        h /= 1.5;
    }
    switch (hotspot)
    {
    case HOT_BRIGHT:
        y -= h;

    case HOT_TRIGHT:
        x -= w;
        break;

    case HOT_BLEFT:
        y -= h;
        break;
    }
    gl.Color4f(1, 1, 1, alpha );
    GL_DrawPSprite(x, y, sprite == SPR_ROCK ? 1 / 1.5 : 1, false,
                   sprInfo.lump);
}

void ST_doFullscreenStuff(void)
{
    player_t *plr = &players[displayplayer];
    char    buf[20];
    int     w, h, pos = 0, spr, i;
    int     h_width = 320 / cfg.hudScale, h_height = 200 / cfg.hudScale;
    float textalpha = hudalpha - hudHideAmount - ( 1 - cfg.hudColor[3]);
    float iconalpha = hudalpha - hudHideAmount - ( 1 - cfg.hudIconAlpha);
    int     ammo_sprite[NUMAMMO] = {
        SPR_AMMO,
        SPR_SBOX,
        SPR_CELL,
        SPR_ROCK
    };

    CLAMP(textalpha, 0.0f, 1.0f);
    CLAMP(iconalpha, 0.0f, 1.0f);

    if(IS_NETGAME && deathmatch && cfg.hudShown[HUD_FRAGS])
    {
        // Display the frag counter.
        i = 199 - 8;
        if(cfg.hudShown[HUD_HEALTH] || cfg.hudShown[HUD_AMMO])
        {
            i -= 18 * cfg.hudScale;
        }
        sprintf(buf, "FRAGS:%i", st_fragscount);
        M_WriteText2(2, i, buf, hu_font_a, cfg.hudColor[0], cfg.hudColor[1],
                     cfg.hudColor[2], textalpha);
    }

    // Setup the scaling matrix.
    gl.MatrixMode(DGL_MODELVIEW);
    gl.PushMatrix();
    gl.Scalef(cfg.hudScale, cfg.hudScale, 1);

    // draw the visible HUD data, first health
    if(cfg.hudShown[HUD_HEALTH])
    {
        ST_drawHUDSprite(SPR_STIM, 2, h_height - 2, HOT_BLEFT, iconalpha);
        ST_HUDSpriteSize(SPR_STIM, &w, &h);
        sprintf(buf, "%i%%", plr->health);
        M_WriteText2(w + 4, h_height - 14, buf, hu_font_b, cfg.hudColor[0],
                     cfg.hudColor[1], cfg.hudColor[2], textalpha);
        pos = 60;
    }

    if(cfg.hudShown[HUD_AMMO])
    {
        ammotype_t ammotype;

        // TODO: Only supports one type of ammo per weapon.
        // for each type of ammo this weapon takes.
        for(ammotype=0; ammotype < NUMAMMO; ++ammotype)
        {
            if(!weaponinfo[plr->readyweapon][plr->class].mode[0].ammotype[ammotype])
                continue;

            spr = ammo_sprite[ammotype];
            ST_drawHUDSprite(spr, pos + 2, h_height - 2, HOT_BLEFT, iconalpha);
            ST_HUDSpriteSize(spr, &w, &h);
            sprintf(buf, "%i", plr->ammo[ammotype]);
            M_WriteText2(pos + w + 4, h_height - 14, buf, hu_font_b,
                         cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textalpha);
            break;
        }
    }

    pos = h_width - 1;
    if(cfg.hudShown[HUD_ARMOR])
    {
        sprintf(buf, "%i%%", plr->armorpoints);
        spr = plr->armortype == 2 ? SPR_ARM2 : SPR_ARM1;
        ST_drawHUDSprite(spr, h_width - 49, h_height - 2, HOT_BRIGHT, iconalpha);
        ST_HUDSpriteSize(spr, &w, &h);

        M_WriteText2(h_width - M_StringWidth(buf, hu_font_b) - 2, h_height - 14, buf, hu_font_b,
                     cfg.hudColor[0], cfg.hudColor[1], cfg.hudColor[2], textalpha);
        pos = h_width - w - 52;
    }

    // Keys  | use a bit of extra scale
    if(cfg.hudShown[HUD_KEYS])
    {
Draw_BeginZoom(0.75f, pos , h_height - 2);
        for(i = 0; i < 3; i++)
        {
            spr = 0;
            if(plr->
               keys[i == 0 ? it_redcard : i ==
                     1 ? it_yellowcard : it_bluecard])
                spr = i == 0 ? SPR_RKEY : i == 1 ? SPR_YKEY : SPR_BKEY;
            if(plr->
               keys[i == 0 ? it_redskull : i ==
                     1 ? it_yellowskull : it_blueskull])
                spr = i == 0 ? SPR_RSKU : i == 1 ? SPR_YSKU : SPR_BSKU;
            if(spr)
            {
                ST_drawHUDSprite(spr, pos, h_height - 2, HOT_BRIGHT,iconalpha);
                ST_HUDSpriteSize(spr, &w, &h);
                pos -= w + 2;
            }
        }
Draw_EndZoom();
    }

    // d64tc > Laser artifacts
    if(cfg.hudShown[HUD_POWER])
    {
        if(plr->lasericon1 == 1)
        {
            ST_drawHUDSprite(SPR_POW1, 2, h_height - 24, HOT_BLEFT, iconalpha);
            ST_HUDSpriteSize(SPR_POW1, &w, &h);
            pos = 64;
        }
        if(plr->lasericon2 == 1)
        {
            ST_drawHUDSprite(SPR_POW2, 2, h_height - 64, HOT_BLEFT, iconalpha);
            ST_HUDSpriteSize(SPR_POW2, &w, &h);
            pos = 64;
        }
        if(plr->lasericon3 == 1)
        {
            ST_drawHUDSprite(SPR_POW3, 2, h_height - 104, HOT_BLEFT, iconalpha);
            ST_HUDSpriteSize(SPR_POW3, &w, &h);
            pos = 64;
        }

        if(plr->artifacts[it_helltime])
        {
            ST_drawHUDSprite(SPR_POW5, 2, h_height - 144, HOT_BLEFT, iconalpha);
            ST_HUDSpriteSize(SPR_POW5, &w, &h);
            if(plr->helltime)
            {
                for(i = 0; i < plr->helltime; ++i)
                {
                    ST_drawHUDSprite(SPR_STHT, 48 + i, h_height - 24, HOT_BLEFT,
                                     iconalpha);
                }
            }
            pos = 64;
        }

        if(plr->artifacts[it_float])
        {
            ST_drawHUDSprite(SPR_POW4, 2, h_height - 184, HOT_BLEFT, iconalpha);
            ST_HUDSpriteSize(SPR_POW4, &w, &h);

            if(plr->devicetime &&
               ((plr->outcastcycle == 2 && plr->artifacts[it_float]) ||
                (plr->outcastcycle == 1 && plr->artifacts[it_float] &&
                 !(plr->artifacts[it_helltime]))))
            {
                for(i = 0; i < plr->devicetime; ++i)
                {
                    ST_drawHUDSprite(SPR_STDT, 48 + i, h_height - 32, HOT_BLEFT,
                                     iconalpha);
                }
            }
            pos = 64;
        }

        if(plr->powers[pw_unsee])
        {
            ST_drawHUDSprite(SPR_SEEA, 2, h_height - 300, HOT_BLEFT, iconalpha);
            ST_HUDSpriteSize(SPR_SEEA, &w, &h);
            pos = 64;
        }
    }
    // < d64tc

    gl.MatrixMode(DGL_MODELVIEW);
    gl.PopMatrix();
}

void ST_Drawer(int fullscreenmode, boolean refresh)
{
    st_firsttime = st_firsttime || refresh;
    st_statusbaron = (fullscreenmode < 2) || (automapactive &&
                     (cfg.automapHudDisplay == 0 || cfg.automapHudDisplay == 2));

    // Do palette shifts
    ST_doPaletteStuff();

    // Either slide the status bar in or fade out the fullscreen hud
    if(st_statusbaron)
    {
        if(hudalpha > 0.0f)
        {
            st_statusbaron = 0;
            hudalpha-=0.1f;
        }
    }
    else
    {
        if(fullscreenmode == 3)
        {
            if(hudalpha > 0.0f)
            {
                hudalpha-=0.1f;
                fullscreenmode = 2;
            }
        }
        else
        {
            if(hudalpha < 1.0f)
                hudalpha+=0.1f;
        }
    }

    // Always try to render statusbar with alpha in fullscreen modes
    if(fullscreenmode)
        st_blended = 1;
    else
        st_blended = 0;

    ST_doFullscreenStuff();
}

void ST_loadGraphics(void)
{
    int     i;
    char    namebuf[9];

    // Load the numbers, tall and short
    for(i = 0; i < 10; i++)
    {
        sprintf(namebuf, "STTNUM%d", i);
        R_CachePatch(&tallnum[i], namebuf);

        sprintf(namebuf, "STYSNUM%d", i);
        R_CachePatch(&shortnum[i], namebuf);
    }

    // Load percent key.
    // Note: why not load STMINUS here, too?
    R_CachePatch(&tallpercent, "STTPRCNT");

    // key cards
    for(i = 0; i < NUMKEYS; i++)
    {
        sprintf(namebuf, "STKEYS%d", i);
        R_CachePatch(&keys[i], namebuf);
    }
}

void ST_updateGraphics(void)
{
    // Nothing to do
}

void ST_loadData(void)
{
    lu_palette = W_GetNumForName("PLAYPAL");
    ST_loadGraphics();
}

void ST_initData(void)
{
    int     i;
    player_t *plyr = &players[consoleplayer];

    st_firsttime = true;

    st_clock = 0;
    st_chatstate = StartChatState;
    st_gamestate = FirstPersonState;

    st_statusbaron = true;
    st_oldchat = st_chat = false;
    st_cursoron = false;

    st_palette = -1;

    st_oldhealth = -1;

    for(i = 0; i < NUMWEAPONS; i++)
    {
        oldweaponsowned[i] = plyr->weaponowned[i];
    }

    for(i = 0; i < 3; i++)
    {
        keyboxes[i] = -1;
    }

    STlib_init();

    ST_HUDUnHide(HUE_FORCE);
}

void ST_createWidgets(void)
{
    // frags sum
    STlib_initNum(&w_frags, ST_FRAGSX, ST_FRAGSY, tallnum, &st_fragscount,
                  &st_fragson, ST_FRAGSWIDTH, &hudalpha);
}

void ST_Start(void)
{
    if(!st_stopped)
        ST_Stop();

    ST_initData();
    ST_createWidgets();
    st_stopped = false;

}

void ST_Stop(void)
{
    if(st_stopped)
        return;
    st_stopped = true;
}

void ST_Init(void)
{
    veryfirsttime = 0;
    ST_loadData();
}

/*
 * Console command to show the hud if hidden.
 */
DEFCC(CCmdHUDShow)
{
    ST_HUDUnHide(HUE_FORCE);
    return true;
}
