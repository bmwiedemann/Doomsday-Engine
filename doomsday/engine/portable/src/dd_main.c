/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2005-2006 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Jamie Jones <yagisan@dengine.net>
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
 * dd_main.c: Engine Core
 */

// HEADER FILES ------------------------------------------------------------

#include "de_platform.h"

#ifdef WIN32
#  include <direct.h>
#endif

#ifdef UNIX
#  include <ctype.h>
#  include <SDL.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#include "de_base.h"
#include "de_system.h"
#include "de_console.h"
#include "de_network.h"
#include "de_play.h"
#include "de_refresh.h"
#include "de_render.h"
#include "de_graphics.h"
#include "de_audio.h"
#include "de_misc.h"

#include "dd_pinit.h"

// MACROS ------------------------------------------------------------------

#define MAXWADFILES 4096

// TYPES -------------------------------------------------------------------

typedef struct ddvalue_s {
    int    *readPtr;
    int    *writePtr;
} ddvalue_t;

typedef struct autoload_s {
    boolean loadFiles;   // Should files be loaded right away.
    int count;           // Number of files loaded successfully.
} autoload_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void    G_CheckDemoStatus();
void    F_Drawer(void);
boolean F_Responder(event_t *ev);
void    S_InitScript(void);
void    Net_Drawer(void);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void HandleArgs(int state);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

#ifdef WIN32
extern HWND hWndMain;
extern HINSTANCE hInstDGL;
#endif

extern int renderTextures;
extern int skyflatnum;
extern char skyflatname[9];
extern fixed_t mapgravity;
extern int gotframe;
extern int monochrome;
extern int gamedataformat;
extern int gamedrawhud;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

directory_t ddRuntimeDir, ddBinDir;
int     verbose = 0;            // For debug messages (-verbose).
boolean DevMaps;                // true = Map development mode
char   *DevMapsDir = "";        // development maps directory
int     shareware;              // true if only episode 1 present
boolean debugmode;              // checkparm of -debug
boolean nofullscreen;           // checkparm of -nofullscreen
boolean cdrom;                  // true if cd-rom mode active
boolean cmdfrag;                // true if a CMD_FRAG packet should be sent out
boolean singletics;             // debug flag to cancel adaptiveness
int     isDedicated = false;
int     maxzone = 0x2000000;    // Default zone heap. (32meg)
boolean autostart;
FILE   *outFile;                // Output file for console messages.

char   *iwadlist[64];
char   *defaultWads = "";       /* A list of wad names, whitespace in between
                                   (in .cfg). */
filename_t configFileName;
filename_t defsFileName, topDefsFileName;
filename_t ddBasePath = "";     // Doomsday root directory is at...?

int     queryResult = 0;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static char *wadfiles[MAXWADFILES];

// CODE --------------------------------------------------------------------

/*
 * Adds the given IWAD to the list of default IWADs.
 */
void DD_AddIWAD(const char *path)
{
    int     i = 0;
    char    buf[256];

    while(iwadlist[i])
        i++;
    M_TranslatePath(path, buf);
    iwadlist[i] = calloc(strlen(buf) + 1, 1);   // This mem is not freed?
    strcpy(iwadlist[i], buf);
}

#define ATWSEPS ",; \t"
static void AddToWadList(char *list)
{
    int     len = strlen(list);
    char   *buffer = malloc(len + 1), *token;

    strcpy(buffer, list);
    token = strtok(buffer, ATWSEPS);
    while(token)
    {
        DD_AddStartupWAD(token /*, false */ );
        token = strtok(NULL, ATWSEPS);
    }
    free(buffer);
}

/*
 * (f_forall_func_t)
 */
static int autoDataAdder(const char *fileName, filetype_t type,
                         void *ptr)
{
    autoload_t *data = ptr;

    // Skip directories.
    if(type == FT_DIRECTORY)
        return true;

    if(data->loadFiles)
    {
        if(W_AddFile(fileName, false))
            ++data->count;
    }
    else
    {
        DD_AddStartupWAD(fileName);
    }

    // Continue searching.
    return true;
}

/*
 * Files with the extensions wad, lmp, pk3, zip and deh in the automatical data
 * directory are added to the wadfiles list.  Returns the number of new
 * files that were loaded.
 */
int DD_AddAutoData(boolean loadFiles)
{
    autoload_t data;
    const char *extensions[] = {
        "wad", "lmp", "pk3", "zip", "deh",
#ifdef UNIX
        "WAD", "LMP", "PK3", "ZIP", "DEH", // upper case alternatives
#endif
        NULL
    };
    char    pattern[256];
    int     i;

    data.loadFiles = loadFiles;
    data.count = 0;

    for(i = 0; extensions[i]; i++)
    {
        sprintf(pattern, "%sauto\\*.%s", R_GetDataPath(), extensions[i]);
        Dir_FixSlashes(pattern);
        F_ForAll(pattern, &data, autoDataAdder);
    }

    return data.count;
}

void DD_SetConfigFile(char *filename)
{
    strcpy(configFileName, filename);
    Dir_FixSlashes(configFileName);
}

/*
 * Set the primary DED file, which is included immediately after
 * Doomsday.ded.
 */
void DD_SetDefsFile(char *filename)
{
    sprintf(topDefsFileName, "%sdefs\\%s", ddBasePath, filename);
    Dir_FixSlashes(topDefsFileName);
}

/*
 * Sets the level of verbosity that was requested using the -verbose
 * option(s).
 */
void DD_Verbosity(void)
{
    int     i;

    for(i = 1, verbose = 0; i < Argc(); ++i)
        if(ArgRecognize("-verbose", Argv(i)))
            verbose++;
}

/*
 * Define Auto mappings for the runtime directory.
 */
void DD_DefineBuiltinVDM(void)
{
    filename_t dest;

    // Data files.
    sprintf(dest, "%sauto", R_GetDataPath());
    F_AddMapping("auto", dest);

    // Definition files.
    Def_GetAutoPath(dest);
    F_AddMapping("auto", dest);
}

/**
 * Looks for new files to autoload. Runs until there are no more files to
 * autoload.
 */
void DD_AutoLoad(void)
{
    int p;

    // Load files from the Auto directory.  (If already loaded, won't
    // be loaded again.)  This is done again because virtual files may
    // now exist in the Auto directory.  Repeated until no new files
    // found.
    while((p = DD_AddAutoData(true)) > 0)
    {
        VERBOSE(Con_Message("Autoload round completed with %i new files.\n",
                            p));
    }
}

/*
 * Engine and game initialization. When complete, starts the game loop.
 * What a mess...
 */
void DD_Main(void)
{
    int     p;
    char    buff[10];
    char   *outfilename = "doomsday.out";
    boolean userdir_ok = true;

    DD_Verbosity();

#ifdef UNIX
    #ifndef MACOSX
        if (getenv("HOME")) {
            filename_t homeDir;
            sprintf(homeDir, "%s/.deng", getenv("HOME"));
            M_CheckPath(homeDir);
            Dir_MakeDir(homeDir, &ddRuntimeDir);
            userdir_ok = Dir_ChDir(&ddRuntimeDir);
        }
    #endif
#endif

    // The -userdir option sets the working directory.
    if(ArgCheckWith("-userdir", 1))
    {
        Dir_MakeDir(ArgNext(), &ddRuntimeDir);
        userdir_ok = Dir_ChDir(&ddRuntimeDir);
    }

    // We'll redirect stdout to a log file.
    DD_CheckArg("-out", &outfilename);
    outFile = fopen(outfilename, "w");
    if(!outFile)
    {
        DD_ErrorBox(false, "Couldn't open message output file.");
    }
    setbuf(outFile, NULL);      // Don't buffer much.

    // The current working directory is the runtime dir.
    Dir_GetDir(&ddRuntimeDir);

#ifdef UNIX
    // The base path is always the same and depends on the build
    // configuration.  Usually this is something like
    // "/usr/share/deng/".
#ifdef MACOSX
    strcpy(ddBasePath, "./");
#else
    strcpy(ddBasePath, DENG_BASE_DIR);
#endif
#endif

#ifdef WIN32
    // The standard base directory is two levels upwards.
    if(ArgCheck("-stdbasedir"))
    {
        strcpy(ddBasePath, "..\\..\\");
    }
#endif

    if(ArgCheckWith("-basedir", 1))
    {
        strcpy(ddBasePath, ArgNext());
        Dir_ValidDir(ddBasePath);
    }

    Dir_MakeAbsolute(ddBasePath);
    Dir_ValidDir(ddBasePath);

    // We need to get the console initialized. Otherwise Con_Message() will
    // crash the system (yikes).
    Con_Init();
    Con_Message("Con_Init: Initializing the console.\n");

    // Create the startup messages window.
    SW_Init();

    Con_Message("Executable: " DOOMSDAY_VERSIONTEXT ".\n");

    // Print the used command line.
    if(verbose)
    {
        Con_Message("Command line (%i strings):\n", Argc());
        for(p = 0; p < Argc(); p++)
            Con_Message("  %i: %s\n", p, Argv(p));
    }

    F_InitMapping();

    // Initialize the key mappings.
    DD_InitInput();

    // Any startup hooks?
    Plug_DoHook(HOOK_STARTUP, 0, 0);

    DD_AddStartupWAD("}data\\doomsday.pk3");
    R_InitExternalResources();

    // The name of the .cfg will invariably be overwritten by the Game.
    strcpy(configFileName, "doomsday.cfg");
    sprintf(defsFileName, "%sdefs\\doomsday.ded", ddBasePath);

    // Was the change to userdir OK?
    if(!userdir_ok)
        Con_Message("--(!)-- User directory not found " "(check -userdir).\n");

    bamsInit();                 // Binary angle calculations.

    // Initialize the zip file database.
    Zip_Init();

    Def_Init();

    if(ArgCheck("-dedicated"))
    {
        SW_Shutdown();
        isDedicated = true;
        Sys_ConInit();
    }

    autostart = false;
    shareware = false;          // Always false for Hexen

    HandleArgs(0);              // Everything but WADs.

    novideo = ArgCheck("-novideo") || isDedicated;

    // Register the engine's binding classes
    B_RegisterBindClasses();

    if(gx.PreInit)
        gx.PreInit();

    // Automatically create an Auto mapping in the runtime directory.
    DD_DefineBuiltinVDM();

    // Initialize subsystems
    Net_Init();                 // Network before anything else.

    // Now we can hide the mouse cursor for good.
    Sys_HideMouse();

    // Load defaults before initing other systems
    Con_Message("Parsing configuration files.\n");
    // Check for a custom config file.
    if(ArgCheckWith("-config", 1))
    {
        // This will override the default config file.
        strcpy(configFileName, ArgNext());
        Con_Message("Custom config file: %s\n", configFileName);
    }

    // This'll be the default config file.
    Con_ParseCommands(configFileName, true);

    // Parse additional files (that should be parsed BEFORE init).
    if(ArgCheckWith("-cparse", 1))
    {
        for(;;)
        {
            char   *arg = ArgNext();

            if(!arg || arg[0] == '-')
                break;
            Con_Message("Parsing: %s\n", arg);
            Con_ParseCommands(arg, false);
        }
    }

    if(defaultWads)
        AddToWadList(defaultWads);  // These must take precedence.
    HandleArgs(1);              // Only the WADs.

    Con_Message("W_Init: Init WADfiles.\n");

    // Add real files from the Auto directory to the wadfiles list.
    DD_AddAutoData(false);

    W_InitMultipleFiles(wadfiles);
    F_InitDirec();

    // Load help resources. Now virtual files are available as well.
    if(!isDedicated)
        DD_InitHelp();

    // Final autoload round.
    DD_AutoLoad();

    // No more WADs will be loaded in startup mode after this point.
    W_EndStartup();

    VERBOSE(W_PrintMapList());

    // Execute the startup script (Startup.cfg).
    Con_ParseCommands("startup.cfg", false);

    // Now the game can identify the game mode.
    gx.UpdateState(DD_GAME_MODE);

    // Now that we've read the WADs we can initialize definitions.
    Def_Read();

#ifdef WIN32
    if(ArgCheck("-nowsk"))      // No Windows system keys?
    {
        // Disable Alt-Tab, Alt-Esc, Ctrl-Alt-Del.
        // A bit of a hack, I'm afraid...
        SystemParametersInfo(SPI_SETSCREENSAVERRUNNING, TRUE, 0, 0);
        Con_Message("Windows system keys disabled.\n");
    }
#endif

    if(ArgCheckWith("-dumplump", 1))
    {
        char   *arg = ArgNext();
        char    fname[100];
        FILE   *file;
        int     lump = W_GetNumForName(arg);
        byte   *lumpPtr = W_CacheLumpNum(lump, PU_STATIC);

        sprintf(fname, "%s.dum", arg);
        file = fopen(fname, "wb");
        if(!file)
        {
            Con_Error("Couldn't open %s for writing. %s\n", fname,
                      strerror(errno));
        }
        fwrite(lumpPtr, 1, lumpinfo[lump].size, file);
        fclose(file);
        Con_Error("%s dumped to %s.\n", arg, fname);
    }

    if(ArgCheck("-dumpwaddir"))
    {
        printf("Lumps (%d total):\n", numlumps);
        for(p = 0; p < numlumps; p++)
        {
            strncpy(buff, lumpinfo[p].name, 8);
            buff[8] = 0;
            printf("%04i - %-8s (hndl: %p, pos: %i, size: %i)\n", p, buff,
                   lumpinfo[p].handle, lumpinfo[p].position, lumpinfo[p].size);
        }
        Con_Error("---End of lumps---\n");
    }

    Con_Message("Sys_Init: Setting up machine state.\n");
    Sys_Init();

    Con_Message("R_Init: Init the refresh daemon.\n");
    R_Init();

    Con_Message("Net_InitGame: Initializing game data.\n");
    Net_InitGame();
    Demo_Init();

    // Engine initialization is complete. Now start the GL driver and go
    // briefly to the Console Startup mode, so the results of the game
    // init, the config file parsing and possible netgame connection will
    // be visible.
    SW_Shutdown();              // The message window can be closed.
    if(!isDedicated)
    {
        Sys_ShowWindow(true);   // Show the main window (was created hidden).
        GL_Init();
        GL_InitRefresh(true, true);
    }

    // Start printing messages in the startup.
    Con_StartupInit();
    Con_Message("Con_StartupInit: Init startup screen.\n");

    if(gx.PostInit)
        gx.PostInit();

    // Now the defs have been read we can init the map format info
    P_InitData();

    // Try to load the autoexec file. This is done here to make sure
    // everything is initialized: the user can do here anything that
    // s/he'd be able to do in the game.
    Con_ParseCommands("autoexec.cfg", false);

    // Parse additional files.
    if(ArgCheckWith("-parse", 1))
    {
        for(;;)
        {
            char   *arg = ArgNext();

            if(!arg || arg[0] == '-')
                break;
            Con_Message("Parsing: %s\n", arg);
            Con_ParseCommands(arg, false);
        }
    }

    // A console command on the command line?
    for(p = 1; p < Argc() - 1; p++)
    {
        if(stricmp(Argv(p), "-command") && stricmp(Argv(p), "-cmd"))
            continue;
        for(++p; p < Argc(); p++)
        {
            char   *arg = Argv(p);

            if(arg[0] == '-')
            {
                p--;
                break;
            }
            Con_Execute(CMDS_CMDLINE, arg, false);
        }
    }

    // In dedicated mode the console must be opened, so all input events
    // will be handled by it.
    if(isDedicated)
        Con_Open(true);

    Plug_DoHook(HOOK_INIT, 0, 0);   // Any initialization hooks?
    Con_UpdateKnownWords();         // For word completion (with Tab).

    // Client connection command.
    if(ArgCheckWith("-connect", 1))
        Con_Executef(CMDS_CMDLINE, false, "connect %s", ArgNext());

    // Server start command.
    // (shortcut for -command "net init tcpip; net server start").
    if(ArgExists("-server"))
    {
        if(!N_InitService(true))
            Con_Message("Can't start server: network init failed.\n");
        else
            Con_Executef(CMDS_CMDLINE, false, "net server start");
    }

    DD_GameLoop();              // Never returns...
}

static void HandleArgs(int state)
{
    int     order, p;

    if(state == 0)
    {
        debugmode = ArgExists("-debug");
        nofullscreen = ArgExists("-nofullscreen") | ArgExists("-window");
        renderTextures = !ArgExists("-notex");
    }

    // Process all -iwad and -file options. -iwad options are processed
    // first so that the loading order remains correct.
    if(state)
    {
        for(order = 0; order < 2; order++)
        {
            for(p = 0; p < Argc(); p++)
            {
                if((order == 1 &&
                    (stricmp(Argv(p), "-f") && stricmp(Argv(p), "-file"))) ||
                   (order == 0 &&
                    stricmp(Argv(p), "-iwad")))
                    continue;
                while(++p != Argc() && !ArgIsOption(p))
                    DD_AddStartupWAD(Argv(p));
                p--;/* For ArgIsOption(p) necessary, for p==Argc() harmless */
            }
        }
    }
}

void DD_CheckTimeDemo(void)
{
    static boolean checked = false;

    if(!checked)
    {
        checked = true;
        if(ArgCheckWith("-timedemo", 1) // Timedemo mode.
           || ArgCheckWith("-playdemo", 1)) // Play-once mode.
        {
            char    buf[200];

            sprintf(buf, "playdemo %s", ArgNext());
            Con_Execute(CMDS_CMDLINE, buf, false);
        }
    }
}

/*
 * This is a 'public' WAD file addition routine. The caller can put a
 * greater-than character (>) in front of the name to prepend the base
 * path to the file name (providing it's a relative path).
 */
void DD_AddStartupWAD(const char *file)
{
    int     i;
    char   *new, temp[300];

    i = 0;
    while(wadfiles[i])
        i++;
    M_TranslatePath(file, temp);
    new = calloc(strlen(temp) + 1, 1);  // This is never freed?
    strcat(new, temp);
    wadfiles[i] = new;
}

/*
 * Queries are a (poor) way to extend the API without adding new functions.
 */
void DD_CheckQuery(int query, int parm)
{
    switch (query)
    {
    case DD_TEXTURE_HEIGHT_QUERY:
        queryResult = textures[parm]->height << FRACBITS;
        break;
#if 0
    // Unused
    case DD_NET_QUERY:
        switch (parm)
        {
        case DD_PROTOCOL:
            queryResult = (int) N_GetProtocolName();
            break;
        }
        break;
#endif
    default:
        break;
    }
}

/* *INDENT-OFF* */
ddvalue_t ddValues[DD_LAST_VALUE - DD_FIRST_VALUE - 1] = {
    {&glScreenWidth, 0},
    {&glScreenHeight, 0},
    {&netgame, 0},
    {&isServer, 0},                         // An *open* server?
    {&isClient, 0},
    {&allowFrames, &allowFrames},
    {&skyflatnum, 0},
    {0, 0},
    {&viewwindowx, &viewwindowx},
    {&viewwindowy, &viewwindowy},
    {&viewwidth, &viewwidth},
    {&viewheight, &viewheight},
    {&viewpw, 0},
    {&viewph, 0},
    {&viewx, &viewx},
    {&viewy, &viewy},
    {&viewz, &viewz},
    {&viewxOffset, &viewxOffset},
    {&viewyOffset, &viewyOffset},
    {&viewzOffset, &viewzOffset},
    {&viewangle, &viewangle},
    {&viewangleoffset, &viewangleoffset},
    {&consoleplayer, &consoleplayer},
    {&displayplayer, &displayplayer},
    {0, 0},
    {&mipmapping, 0},
    {&linearRaw, 0},
    {&defResX, &defResX},
    {&defResY, &defResY},
    {&skyDetail, 0},
    {&sfx_volume, &sfx_volume},
    {&mus_volume, &mus_volume},
    {&mouseInverseY, &mouseInverseY},
    {&usegamma, 0},
    {&queryResult, 0},
    {&LevelFullBright, &LevelFullBright},
    {&CmdReturnValue, 0},
    {&gameReady, &gameReady},
    {&openrange, 0},
    {&opentop, 0},
    {&openbottom, 0},
    {&lowfloor, 0},
    {&isDedicated, 0},
    {(int *) &novideo, 0},
    {&defs.count.mobjs.num, 0},
    {&mapgravity, &mapgravity},
    {&gotframe, 0},
    {&playback, 0},
    {&defs.count.sounds.num, 0},
    {&defs.count.music.num, 0},
    {&numlumps, 0},
    {&send_all_players, &send_all_players},
    {&pspOffX, &pspOffX},
    {&pspOffY, &pspOffY},
    {&psp_move_speed, &psp_move_speed},
    {&cplr_thrust_mul, &cplr_thrust_mul},
    {(int *) &clientPaused, (int *) &clientPaused},
    {&weaponOffsetScaleY, &weaponOffsetScaleY},
    {&monochrome, &monochrome},
    {&gamedataformat, &gamedataformat},
    {&gamedrawhud, 0}
};
/* *INDENT-ON* */

/*
 * Get a 32-bit integer value.
 */
int DD_GetInteger(int ddvalue)
{
    if(ddvalue >= DD_LAST_VALUE || ddvalue <= DD_FIRST_VALUE)
    {
        // How about some specials?
        switch (ddvalue)
        {
        case DD_SECTOR_COUNT:
            return numsectors;

        case DD_LINE_COUNT:
            return numlines;

        case DD_SIDE_COUNT:
            return numsides;

        case DD_VERTEX_COUNT:
            return numvertexes;

        case DD_POLYOBJ_COUNT:
            return po_NumPolyobjs;

        case DD_SEG_COUNT:
            return numsegs;

        case DD_SUBSECTOR_COUNT:
            return numsubsectors;

        case DD_NODE_COUNT:
            return numnodes;

        case DD_THING_COUNT:
            return numthings;

        case DD_BLOCKMAP_WIDTH:
            return bmapwidth;

        case DD_BLOCKMAP_HEIGHT:
            return bmapheight;

        case DD_GAME_EXPORTS:
            ASSERT_NOT_64BIT();
            return (int) &gx;

        case DD_DYNLIGHT_TEXTURE:
            return lightingTexNames[LST_DYNAMIC];

        case DD_TRACE_ADDRESS:
            ASSERT_NOT_64BIT();
            return (int) &trace;

        case DD_TRANSLATIONTABLES_ADDRESS:
            ASSERT_NOT_64BIT();
            return (int) translationtables;

        case DD_MAP_NAME:
            ASSERT_NOT_64BIT();
            if(mapinfo && mapinfo->name[0])
                return (int) mapinfo->name;
            break;

        case DD_MAP_AUTHOR:
            ASSERT_NOT_64BIT();
            if(mapinfo && mapinfo->author[0])
                return (int) mapinfo->author;
            break;

        case DD_MAP_MUSIC:
            if(mapinfo)
                return Def_GetMusicNum(mapinfo->music);
            return -1;

#ifdef WIN32
        case DD_WINDOW_HANDLE:
            ASSERT_NOT_64BIT();
            return (int) hWndMain;
#endif
        }
        return 0;
    }

    // We have to separately calculate the 35 Hz ticks.
    if(ddvalue == DD_GAMETIC)
        return SECONDS_TO_TICKS(gameTime);

    if(ddValues[ddvalue].readPtr == NULL)
        return 0;

    return *ddValues[ddvalue].readPtr;
}

/*
 * Set a 32-bit integer value.
 */
void DD_SetInteger(int ddvalue, int parm)
{
    if(ddvalue <= DD_FIRST_VALUE || ddvalue >= DD_LAST_VALUE)
    {
        DD_CheckQuery(ddvalue, parm);
        // How about some special values?
        if(ddvalue == DD_POLYOBJ_COUNT)
        {
            po_NumPolyobjs = parm;
        }
        else if(ddvalue == DD_SKYFLAT_NAME)
        {
            // Dude!  This is not 64-bit safe.
            ASSERT_NOT_64BIT();
            memset(skyflatname, 0, 9);
            strncpy(skyflatname, (char *) parm, 9);
        }
        else if(ddvalue == DD_TRANSLATED_SPRITE_TEXTURE)
        {
            // See DD_TSPR_PARM in dd_share.h.
            int     lump = parm & 0xffffff;
            int     cls = (parm >> 24) & 0xf;
            int     table = (parm >> 28) & 0xf;

            if(table)
                GL_SetTranslatedSprite(lump, table, cls);
            else
                GL_SetSprite(lump, 0);
        }
        else if(ddvalue == DD_TEXTURE_GLOW)
        {
            // See DD_TGLOW_PARM in dd_share.h.
            int     tnum = parm & 0xffff;
            int     istex = (parm & 0x80000000) != 0;
            int     glowstate = (parm & 0x10000) != 0;

            if(istex)
            {
                if(glowstate)
                    textures[tnum]->flags |= TXF_GLOW;
                else
                    textures[tnum]->flags &= ~TXF_GLOW;
            }
            else
            {
                flat_t *fl = R_GetFlat(tnum);

                if(glowstate)
                    fl->flags |= TXF_GLOW;
                else
                    fl->flags &= ~TXF_GLOW;
            }

            // Update all surfaces which may be using this texture.
            R_UpdateAllSurfaces(false);
        }
        return;
    }
    if(ddValues[ddvalue].writePtr)
        *ddValues[ddvalue].writePtr = parm;
}

/*
 * Get a pointer to the value of a variable. Not all variables support
 * this. Added for 64-bit support.
 */
void* DD_GetVariable(int ddvalue)
{
    if(ddvalue >= DD_LAST_VALUE || ddvalue <= DD_FIRST_VALUE)
    {
        // How about some specials?
        switch (ddvalue)
        {
        case DD_GAME_EXPORTS:
            return &gx;

        case DD_TRACE_ADDRESS:
            return &trace;

        case DD_TRANSLATIONTABLES_ADDRESS:
            return translationtables;

        case DD_MAP_NAME:
            if(mapinfo && mapinfo->name[0])
                return mapinfo->name;
            break;

        case DD_MAP_AUTHOR:
            if(mapinfo && mapinfo->author[0])
                return mapinfo->author;
            break;

        case DD_BLOCKMAP_ORIGIN_X:
            return &bmaporgx;

        case DD_BLOCKMAP_ORIGIN_Y:
            return &bmaporgy;

#ifdef WIN32
        case DD_WINDOW_HANDLE:
            return hWndMain;
#endif
        }
        return 0;
    }

    // Other values not supported.
    return ddValues[ddvalue].writePtr;
}

/*
 * Set the value of a variable. The pointer can point to any data, its
 * interpretation depends on the variable. Added for 64-bit support.
 */
void DD_SetVariable(int ddvalue, void *parm)
{
    if(ddvalue <= DD_FIRST_VALUE || ddvalue >= DD_LAST_VALUE)
    {
        if(ddvalue == DD_SKYFLAT_NAME)
        {
            memset(skyflatname, 0, 9);
            strncpy(skyflatname, parm, 9);
        }
        return;
    }
}

/*
 * Gets the data of a player.
 */
ddplayer_t *DD_GetPlayer(int number)
{
    return (ddplayer_t *) &players[number];
}

#ifdef UNIX
/*
 * Some routines are not available on the *nix platform.
 */
char *strupr(char *string)
{
    char   *ch = string;

    for(; *ch; ch++)
        *ch = toupper(*ch);
    return string;
}

char *strlwr(char *string)
{
    char   *ch = string;

    for(; *ch; ch++)
        *ch = tolower(*ch);
    return string;
}
#endif
