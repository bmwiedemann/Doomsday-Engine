/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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
 *
 * \bug Not 64bit clean: In function 'DS_SFX_CreateBuffer': cast to pointer from integer of different size
 * \bug Not 64bit clean: In function 'DS_SFX_DestroyBuffer': cast to pointer from integer of different size
 * \bug Not 64bit clean: In function 'Sys_LoadAudioDriver': cast to pointer from integer of different size
 * \bug Not 64bit clean: In function 'DS_SFX_Play': cast to pointer from integer of different size
 * \bug Not 64bit clean: In function 'DS_SFX_Stop': cast to pointer from integer of different size
 * \bug Not 64bit clean: In function 'DS_SFX_Refresh': cast to pointer from integer of different size
 * \bug Not 64bit clean: In function 'DS_SFX_Set': cast to pointer from integer of different size
 * \bug Not 64bit clean: In function 'DS_SFX_Setv': cast to pointer from integer of different size
 */

/**
 * driver_openal.c: OpenAL Doomsday Sfx Driver.
 *
 * Link with openal32.lib (and doomsday.lib).
 */

// HEADER FILES ------------------------------------------------------------

#ifdef WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

#ifdef MSVC
#  pragma warning (disable: 4244)
#endif

#ifndef FINK
   #include <AL/al.h>
   #include <AL/alc.h>
#else
   #include <openal/al.h>
   #include <openal/alc.h>
#endif
#include <string.h>
#include <math.h>

#include "doomsday.h"
#include "sys_audiod.h"
#include "sys_audiod_sfx.h"

// MACROS ------------------------------------------------------------------

#define PI          3.141592654

#define SRC(buf)    ((ALuint)buf->ptr3D)
#define BUF(buf)    ((ALuint)buf->ptr)

// TYPES -------------------------------------------------------------------

enum { VX, VY, VZ };

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

#ifdef WIN32
ALenum(*EAXGet) (const struct _GUID* propertySetID, ALuint prop,
                 ALuint source, ALvoid* value, ALuint size);
ALenum(*EAXSet) (const struct _GUID* propertySetID, ALuint prop,
                 ALuint source, ALvoid *value, ALuint size);
#endif

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

int         DS_Init(void);
void        DS_Shutdown(void);
void        DS_Event(int type);

int         DS_SFX_Init(void);
sfxbuffer_t* DS_SFX_CreateBuffer(int flags, int bits, int rate);
void        DS_SFX_DestroyBuffer(sfxbuffer_t* buf);
void        DS_SFX_Load(sfxbuffer_t* buf, struct sfxsample_s* sample);
void        DS_SFX_Reset(sfxbuffer_t* buf);
void        DS_SFX_Play(sfxbuffer_t* buf);
void        DS_SFX_Stop(sfxbuffer_t* buf);
void        DS_SFX_Refresh(sfxbuffer_t* buf);
void        DS_SFX_Set(sfxbuffer_t* buf, int prop, float value);
void        DS_SFX_Setv(sfxbuffer_t* buf, int prop, float* values);
void        DS_SFX_Listener(int prop, float value);
void        DS_SFX_Listenerv(int prop, float* values);
int         DS_SFX_Getv(int prop, void* values);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

#ifdef WIN32
// EAX 2.0 GUIDs
struct _GUID DSPROPSETID_EAX20_ListenerProperties =
    { 0x306a6a8, 0xb224, 0x11d2, {0x99, 0xe5, 0x0, 0x0, 0xe8, 0xd8, 0xc7,
                                  0x22}
};
struct _GUID DSPROPSETID_EAX20_BufferProperties =
    { 0x306a6a7, 0xb224, 0x11d2, {0x99, 0xe5, 0x0, 0x0, 0xe8, 0xd8, 0xc7,
                                  0x22}
};
#endif

boolean initOk = false, hasEAX = false;
int verbose;
float unitsPerMeter = 1;
float headYaw, headPitch; // In radians.
ALCdevice* device = 0;
ALCcontext* context = 0;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

static int error(const char* what, const char* msg)
{
    ALenum              code = alGetError();

    if(code == AL_NO_ERROR)
        return false;

    Con_Message("DS_%s(OpenAL): %s [%s]\n", what, msg, alGetString(code));
    return true;
}

int DS_Init(void)
{
    if(initOk)
        return true;

    // Are we in verbose mode?
    if((verbose = ArgExists("-verbose")))
        Con_Message("DS_Init(OpenAL): Starting OpenAL...\n");

    // Open device.
    if(!(device = alcOpenDevice((ALubyte *) "DirectSound3D")))
    {
        Con_Message("Failed to initialize OpenAL (DS3D).\n");
        return false;
    }
    // Create a context.
    alcMakeContextCurrent(context = alcCreateContext(device, NULL));

    // Clear error message.
    alGetError();

#ifdef WIN32
    // Check for EAX 2.0.
    if((hasEAX = alIsExtensionPresent((ALubyte *) "EAX2.0")))
    {
        if(!(EAXGet = alGetProcAddress("EAXGet")))
            hasEAX = false;
        if(!(EAXSet = alGetProcAddress("EAXSet")))
            hasEAX = false;
    }

    if(hasEAX && verbose)
        Con_Message("DS_Init(OpenAL): EAX 2.0 available.\n");
#else
    hasEAX = false;
#endif

    alListenerf(AL_GAIN, 1);
    alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);
    headYaw = headPitch = 0;
    unitsPerMeter = 36;

    // Everything is OK.
    initOk = true;
    return true;
}

void DS_Shutdown(void)
{
    if(!initOk)
        return;

    alcMakeContextCurrent(NULL);
    alcDestroyContext(context);
    alcCloseDevice(device);

    context = NULL;
    device = NULL;
    initOk = false;
}

void DS_Event(int type)
{
    // Not supported.
}

int DS_SFX_Init(void)
{
    return true;
}

sfxbuffer_t* DS_SFX_CreateBuffer(int flags, int bits, int rate)
{
    sfxbuffer_t*        buf;
    ALuint              bufName, srcName;

    // Create a new buffer and a new source.
    alGenBuffers(1, &bufName);
    if(error("CreateBuffer", "GenBuffers"))
        return NULL;

    alGenSources(1, &srcName);
    if(error("CreateBuffer", "GenSources"))
    {
        alDeleteBuffers(1, &bufName);
        return NULL;
    }

    // Attach the buffer to the source.
    alSourcei(srcName, AL_BUFFER, bufName);
    error("CreateBuffer", "Source BUFFER");

    if(!(flags & SFXBF_3D))
    {   // 2D sounds are around the listener.
        alSourcei(srcName, AL_SOURCE_RELATIVE, AL_TRUE);
        alSourcef(srcName, AL_ROLLOFF_FACTOR, 0);
    }

    // Create the buffer object.
    buf = Z_Calloc(sizeof(*buf), PU_STATIC, 0);

    buf->ptr = (void *) bufName;
    buf->ptr3D = (void *) srcName;
    buf->bytes = bits / 8;
    buf->rate = rate;
    buf->flags = flags;
    buf->freq = rate; // Modified by calls to Set(SFXBP_FREQUENCY).

    return buf;
}

void DS_SFX_DestroyBuffer(sfxbuffer_t* buf)
{
    ALuint             srcName, bufName;

    if(!buf)
        return;

    srcName = SRC(buf);
    bufName = BUF(buf);

    alDeleteSources(1, &srcName);
    alDeleteBuffers(1, &bufName);

    Z_Free(buf);
}

void DS_SFX_Load(sfxbuffer_t* buf, struct sfxsample_s* sample)
{
    if(!buf || !sample)
        return;

    // Does the buffer already have a sample loaded?
    if(buf->sample)
    {
        // Is the same one?
        if(buf->sample->id == sample->id)
            return; // No need to reload.
    }

    alBufferData(BUF(buf),
                 sample->bytesPer == 1 ? AL_FORMAT_MONO8 : AL_FORMAT_MONO16,
                 sample->data, sample->size, sample->rate);

    error("Load", "BufferData");
    buf->sample = sample;
}

/**
 * Stops the buffer and makes it forget about its sample.
 */
void DS_SFX_Reset(sfxbuffer_t* buf)
{
    if(!buf)
        return;

    DS_SFX_Stop(buf);
    buf->sample = NULL;
}

void DS_SFX_Play(sfxbuffer_t* buf)
{
    ALuint              source;

    // Playing is quite impossible without a sample.
    if(!buf || !buf->sample)
        return;

    source = SRC(buf);

/*#if _DEBUG
alGetSourcei(source, AL_BUFFER, &bn);
Con_Message("Buffer = %x\n", bn);
if(bn != BUF(buf))
    Con_Message("Not the same!\n");
#endif*/

    alSourcei(source, AL_BUFFER, BUF(buf));
    alSourcei(source, AL_LOOPING, (buf->flags & SFXBF_REPEAT) != 0);
    alSourcePlay(source);
    error("Play", "SourcePlay");
    error("Play", "Get state");

    // The buffer is now playing.
    buf->flags |= SFXBF_PLAYING;
}

void DS_SFX_Stop(sfxbuffer_t* buf)
{
    if(!buf || !buf->sample)
        return;

    alSourceRewind(SRC(buf));
    buf->flags &= ~SFXBF_PLAYING;
}

void DS_SFX_Refresh(sfxbuffer_t* buf)
{
    ALint               state;

    if(!buf || !buf->sample)
        return;

    alGetSourcei(SRC(buf), AL_SOURCE_STATE, &state);
    if(state == AL_STOPPED)
    {
        buf->flags &= ~SFXBF_PLAYING;
    }
}

/**
 * @param yaw           Yaw in radians.
 * @param pitch         Pitch in radians.
 * @param front         Ptr to front vector, can be @c NULL.
 * @param up            Ptr to up vector, can be @c NULL.
 */
static void vectors(float yaw, float pitch, float* front, float* up)
{
    if(!front && !up)
        return; // Nothing to do.

    if(front)
    {
        front[VX] = (float) (cos(yaw) * cos(pitch));
        front[VZ] = (float) (sin(yaw) * cos(pitch));
        front[VY] = (float) sin(pitch);
    }

    if(up)
    {
        up[VX] = (float) (-cos(yaw) * sin(pitch));
        up[VZ] = (float) (-sin(yaw) * sin(pitch));
        up[VY] = (float) cos(pitch);
    }
}

/**
 * Pan is linear, from -1 to 1. 0 is in the middle.
 */
static void setPan(ALuint source, float pan)
{
    float               pos[3];

    vectors((float) (headYaw - pan * PI / 2), headPitch, pos, 0);
    alSourcefv(source, AL_POSITION, pos);
}

void DS_SFX_Set(sfxbuffer_t* buf, int prop, float value)
{
    unsigned int        dw;
    ALuint              source;

    if(!buf)
        return;

    source = SRC(buf);

    switch(prop)
    {
    case SFXBP_VOLUME:
        alSourcef(source, AL_GAIN, value);
        break;

    case SFXBP_FREQUENCY:
        dw = (int) (buf->rate * value);
        if(dw != buf->freq) // Don't set redundantly.
        {
            buf->freq = dw;
            alSourcef(source, AL_PITCH, value);
        }
        break;

    case SFXBP_PAN:
        setPan(source, value);
        break;

    case SFXBP_MIN_DISTANCE:
        alSourcef(source, AL_REFERENCE_DISTANCE, value / unitsPerMeter);
        break;

    case SFXBP_MAX_DISTANCE:
        alSourcef(source, AL_MAX_DISTANCE, value / unitsPerMeter);
        break;

    case SFXBP_RELATIVE_MODE:
        alSourcei(source, AL_SOURCE_RELATIVE, value ? AL_TRUE : AL_FALSE);
        break;

    default:
        break;
    }
}

void DS_SFX_Setv(sfxbuffer_t* buf, int prop, float* values)
{
    ALuint              source;

    if(!buf || !values)
        return;

    source = SRC(buf);

    switch(prop)
    {
    case SFXBP_POSITION:
        alSource3f(source, AL_POSITION, values[VX] / unitsPerMeter,
                   values[VZ] / unitsPerMeter, values[VY] / unitsPerMeter);
        break;

    case SFXBP_VELOCITY:
        alSource3f(source, AL_VELOCITY, values[VX] / unitsPerMeter,
                   values[VZ] / unitsPerMeter, values[VY] / unitsPerMeter);
        break;

    default:
        break;
    }
}

void DS_SFX_Listener(int prop, float value)
{
    switch(prop)
    {
    case SFXLP_UNITS_PER_METER:
        unitsPerMeter = value;
        break;

    case SFXLP_DOPPLER:
        alDopplerFactor(value);
        break;

    default:
        break;
    }
}

void DS_SFX_Listenerv(int prop, float* values)
{
    float               ori[6];

    if(!values)
        return;

    switch(prop)
    {
    case SFXLP_PRIMARY_FORMAT:
        // No need to concern ourselves with this kind of things...
        break;

    case SFXLP_POSITION:
        alListener3f(AL_POSITION, values[VX] / unitsPerMeter,
                     values[VZ] / unitsPerMeter, values[VY] / unitsPerMeter);
        break;

    case SFXLP_VELOCITY:
        alListener3f(AL_VELOCITY, values[VX] / unitsPerMeter,
                     values[VZ] / unitsPerMeter, values[VY] / unitsPerMeter);
        break;

    case SFXLP_ORIENTATION:
        vectors(headYaw = (float) (values[VX] / 180 * PI),
                headPitch = (float) (values[VY] / 180 * PI),
                ori, ori + 3);
        alListenerfv(AL_ORIENTATION, ori);
        break;

    case SFXLP_REVERB: // Not supported.
        break;

    default:
        DS_SFX_Listener(prop, 0);
        break;
    }
}

int DS_SFX_Getv(int prop, void* values)
{
    // Stub.
    return 0;
}
