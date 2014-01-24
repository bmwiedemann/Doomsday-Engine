/** @file p_sound.cpp  id Tech 1 sound playback functionality.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1999 Activision
 * @authors Copyright © 1993-1996 id Software, Inc.
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

#include "common.h"
#include "p_sound.h"

#include "dmu_lib.h"
#include "hexlex.h"

void S_MapMusic(uint episode, uint map)
{
#ifdef __JHEXEN__
    int const defIndex = Def_Get(DD_DEF_MUSIC, "currentmap", 0);
    int const cdTrack  = P_MapInfo(map)->cdTrack;
    char const *lump   = P_GetMapSongLump(map);

    // Update the 'currentmap' music definition.

    App_Log(DE2_RES_VERBOSE, "S_MapMusic: Episode %i, map %i, lump %s", episode, map, lump);

    Def_Set(DD_DEF_MUSIC, defIndex, DD_LUMP,     lump);
    Def_Set(DD_DEF_MUSIC, defIndex, DD_CD_TRACK, &cdTrack);

    if(S_StartMusic("currentmap", true))
    {
        // Set the game status cvar for the map music
        gsvMapMusic = defIndex;
    }
#else
    Uri *mapUri = G_ComposeMapUri(episode, map);
    AutoStr *mapPath = Uri_Compose(mapUri);
    ddmapinfo_t mapInfo;
    if(Def_Get(DD_DEF_MAP_INFO, Str_Text(mapPath), &mapInfo))
    {
        if(S_StartMusicNum(mapInfo.music, true))
        {
            // Set the game status cvar for the map music.
            gsvMapMusic = mapInfo.music;
        }
    }
    Uri_Delete(mapUri);
#endif
}

void S_SectorSound(Sector *sec, int id)
{
    if(!sec) return;

    S_SectorStopSounds(sec);
    S_StartSound(id, (mobj_t *) P_GetPtrp(sec, DMU_EMITTER));
}

void S_SectorStopSounds(Sector *sec)
{
    if(!sec) return;

    // Stop other sounds playing from origins in this sector.
    /// @todo Add a compatibility option allowing origins to work independently?
    S_StopSound2(0, (mobj_t*) P_GetPtrp(sec, DMU_EMITTER), SSF_ALL_SECTOR);
}

void S_PlaneSound(Plane *pln, int id)
{
    if(!pln) return;

    S_SectorStopSounds((Sector *) P_GetPtrp(pln, DMU_SECTOR));
    S_StartSound(id, (mobj_t *) P_GetPtrp(pln, DMU_EMITTER));
}

#ifdef __JHEXEN__

int S_GetSoundID(char const *name)
{
    return Def_Get(DD_DEF_SOUND_BY_NAME, name, 0);
}

void SndInfoParser(Str const *path)
{
    AutoStr *script = M_ReadFileIntoString(path, 0);

    if(script && !Str_IsEmpty(script))
    {
        App_Log(DE2_RES_VERBOSE, "Parsing \"%s\"...", F_PrettyPath(Str_Text(path)));

        HexLex lexer(script, path);

        while(lexer.readToken())
        {
            if(!Str_CompareIgnoreCase(lexer.token(), "$archivepath")) // Unused.
            {
                // $archivepath string(local-directory)
                // Used in combination with the -devsnd command line argument to
                // redirect the loading of sounds to a directory in the local file
                // system.
                lexer.readString();
                continue;
            }
            if(!Str_CompareIgnoreCase(lexer.token(), "$map"))
            {
                // $map int(map-number) string(lump-name)
                // Associate a music lump to a map.
                uint const map          = lexer.readMapNumber();
                AutoStr const *lumpName = lexer.readLumpName();

                if(map >= 0 && map >= P_MapInfoCount())
                {
                    mapinfo_t *mapInfo = P_MapInfo(map);
                    strncpy(mapInfo->songLump, Str_Text(lumpName), sizeof(mapInfo->songLump));
                }
                continue;
            }
            if(Str_At(lexer.token(), 0) == '$')
            {
                // Ignore all other unknown commands. (Vanilla parser compat).
                continue;
            }

            lexer.unreadToken();

            // string(sound-id) string(lump-name | '?')
            // A sound definition.
            int const soundIndex    = lexer.readSoundIndex();
            AutoStr const *lumpName = lexer.readLumpName();

            if(soundIndex)
            {
                Def_Set(DD_DEF_SOUND, soundIndex,
                        DD_LUMP, Str_At(lumpName, 0) == '?' ? "default" : Str_Text(lumpName));
            }
        }
    }

    // All sounds left without a lumpname will use "DEFAULT".
    char buf[80];
    for(int i = 0; i < Get(DD_NUMSOUNDS); ++i)
    {
        /// @kludge This uses a kludge to traverse the entire sound list.
        /// @todo Implement a mechanism for walking the Def databases.
        Def_Get(DD_DEF_SOUND_LUMPNAME, (char *) &i, buf);
        if(!strcmp(buf, ""))
        {
            Def_Set(DD_DEF_SOUND, i, DD_LUMP, "default");
        }
    }

    if(gameMode == hexen_betademo)
    {
        // The WAD contains two lumps with the name CHAIN, one a sample and the
        // other a graphics lump.
        int soundId = Def_Get(DD_DEF_SOUND_BY_NAME, "AMBIENT12", 0);
        Def_Get(DD_DEF_SOUND_LUMPNAME, (char *) &soundId, buf);
        if(!strcasecmp(buf, "chain"))
        {
            Def_Set(DD_DEF_SOUND, soundId, DD_LUMP, "default");
        }
    }
}

#endif // __JHEXEN__