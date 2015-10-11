/** @file driver_fluidsynth.cpp  FluidSynth music plugin.
 * @ingroup dsfluidsynth
 *
 * @authors Copyright � 2011-2013 Jaakko Ker�nen <jaakko.keranen@iki.fi>
 * @authors Copyright � 2015 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "driver_fluidsynth.h"

#include "doomsday.h"
#include "api_audiod.h"

#include <de/c_wrapper.h>
#include <cstdio>
#include <cstring>

static fluid_settings_t *fsConfig;
static fluid_synth_t *fsSynth;
static audiointerface_sfx_t *fsSfx;
static fluid_audio_driver_t *fsDriver;

fluid_synth_t *DMFluid_Synth()
{
    DENG2_ASSERT(fsSynth != nullptr);
    return fsSynth;
}

fluid_audio_driver_t *DMFluid_Driver()
{
    return fsDriver;
}

audiointerface_sfx_generic_t *DMFluid_Sfx()
{
    DENG_ASSERT(fsSfx != nullptr);
    return &fsSfx->gen;
}

/**
 * Initialize the FluidSynth sound driver.
 */
int DS_Init()
{
    // Already been here?
    if(fsSynth) return true;

    // Set up a reasonable configuration.
    ::fsConfig = new_fluid_settings();
    fluid_settings_setnum(::fsConfig, "synth.gain", MAX_SYNTH_GAIN);

    // Create the synthesizer.
    ::fsSynth = new_fluid_synth(::fsConfig);
    if(!::fsSynth)
    {
        App_Log(DE2_AUDIO_ERROR, "[FluidSynth] Failed to create synthesizer");
        return false;
    }

#ifndef FLUIDSYNTH_NOT_A_DLL
    // Create the output driver that will play the music.
    char driverName[50];
    if(!UnixInfo_GetConfigValue("defaults", "fluidsynth:driver", driverName, sizeof(driverName) - 1))
    {
        strcpy(driverName, FLUIDSYNTH_DEFAULT_DRIVER_NAME);
    }
    fluid_settings_setstr(::fsConfig, "audio.driver", driverName);
    ::fsDriver = new_fluid_audio_driver(::fsConfig, ::fsSynth);
    if(!::fsDriver)
    {
        App_Log(DE2_AUDIO_ERROR, "[FluidSynth] Failed to load audio driver '%s'", driverName);
        return false;
    }
#else
    fsDriver = nullptr;
#endif

    DSFLUIDSYNTH_TRACE("DS_Init: FluidSynth initialized.");
    return true;
}

/**
 * Shut everything down.
 */
void DS_Shutdown()
{
    // Already been here?
    if(!::fsSynth) return;

    DMFluid_Shutdown();

    DSFLUIDSYNTH_TRACE("DS_Shutdown.");

    if(::fsDriver)
    {
        delete_fluid_audio_driver(::fsDriver);
    }
    delete_fluid_synth(::fsSynth); fsSynth = nullptr;
    delete_fluid_settings(::fsConfig); fsConfig = nullptr;
}

/**
 * The Event function is called to tell the driver about certain critical
 * events like the beginning and end of an update cycle.
 */
void DS_Event(int type)
{
    if(!::fsSynth) return;

    if(type == SFXEV_END)
    {
        // End of frame, do an update.
        DMFluid_Update();
    }
}

int DS_Get(int prop, void *ptr)
{
    switch(prop)
    {
    case AUDIOP_IDENTITYKEY: {
        auto *idKey = reinterpret_cast<AutoStr *>(ptr);
        DENG2_ASSERT(idKey);
        if(idKey) Str_Set(idKey, "fluidsynth");
        return true; }

    case AUDIOP_TITLE: {
        auto *title = reinterpret_cast<AutoStr *>(ptr);
        DENG2_ASSERT(title);
        if(title) Str_Set(title, "FluidSynth");
        return true; }

    default: DSFLUIDSYNTH_TRACE("DS_Get: Unknown property " << prop); break;
    }
    return false;
}

int DS_Set(int prop, void const *ptr)
{
    switch(prop)
    {
    case AUDIOP_SOUNDFONT_FILENAME: {
        auto const *path = reinterpret_cast<char const *>(ptr);
        DSFLUIDSYNTH_TRACE("DS_Set: Soundfont = " << path);
        if(!path || !strlen(path))
        {
            // Use the default.
            path = 0;
        }
        DMFluid_SetSoundFont(path);
        return true; }

    case AUDIOP_SFX_INTERFACE:
        fsSfx = reinterpret_cast<audiointerface_sfx_t *>(ptr);
        DSFLUIDSYNTH_TRACE("DS_Set: iSFX = " << fsSfx);
        return true;

    default: DSFLUIDSYNTH_TRACE("DS_Set: Unknown property " << prop); break;
    }

    return false;
}

/**
 * Declares the type of the plugin so the engine knows how to treat it. Called
 * automatically when the plugin is loaded.
 */
DENG_EXTERN_C const char* deng_LibraryType(void)
{
    return "deng-plugin/audio";
}

DENG_DECLARE_API(Con);

DENG_API_EXCHANGE(
    DENG_GET_API(DE_API_CONSOLE, Con);
)