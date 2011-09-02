/**\file lumpinfo.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2011 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_FILESYS_LUMPINFO_H
#define LIBDENG_FILESYS_LUMPINFO_H

#include "dd_string.h"

/**
 * LumpInfo record. POD.
 * @ingroup fs
 */
typedef struct {
    lumpname_t name; /// Ends in '\0'. Used with WAD lumps.
    ddstring_t path; /// Full variable-length path.
    size_t baseOffset; /// Offset from start of owning package.
    size_t size; /// Size of the uncompressed file.
    size_t compressedSize; /// Size of the original file compressed.
    uint lastModified; /// Unix timestamp.
} lumpinfo_t;

#endif /* LIBDENG_FILESYS_LUMPINFO_H */
