/** @file doomsdayinfo.cpp  Information about Doomsday Engine and its plugins.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de/shell/DoomsdayInfo"
#include <QDir>

namespace de {
namespace shell {

static struct
{
    char const *name;
    char const *mode;
}
gameModes[] =
{
    //{ "None",                                   "" },

    { "Shareware DOOM",                         "doom1-share" },
    { "DOOM",                                   "doom1" },
    { "Ultimate DOOM",                          "doom1-ultimate" },
    { "DOOM II",                                "doom2" },
    { "Final DOOM: Plutonia Experiment",        "doom2-plut" },
    { "Final DOOM: TNT Evilution",              "doom2-tnt" },
    { "Chex Quest",                             "chex" },
    { "HacX",                                   "hacx" },

    { "Shareware Heretic",                      "heretic-share" },
    { "Heretic",                                "heretic" },
    { "Heretic: Shadow of the Serpent Riders",  "heretic-ext" },

    { "Hexen v1.1",                             "hexen" },
    { "Hexen v1.0",                             "hexen-v10" },
    { "Hexen: Death Kings of Dark Citadel",     "hexen-dk" },
    { "Hexen Demo",                             "hexen-demo" },

    { 0, 0 }
};

QList<DoomsdayInfo::GameMode> DoomsdayInfo::allGameModes()
{
    QList<GameMode> modes;
    for(int i = 0; gameModes[i].name; ++i)
    {
        GameMode mod;
        mod.title  = gameModes[i].name;
        mod.option = gameModes[i].mode;
        modes.append(mod);
    }
    return modes;
}

NativePath DoomsdayInfo::defaultServerRuntimeFolder()
{
#ifdef MACOSX
    return QDir::home().filePath("Library/Application Support/Doomsday Engine/server-runtime");
#else
    return NativePath(QDir::home().filePath(".doomsday")) / "server-runtime";
#endif
}

} // namespace shell
} // namespace de
