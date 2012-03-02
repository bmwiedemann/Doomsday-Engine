/**
 * @file string.hh
 * C++ wrapper for ddstring_t. @ingroup base
 *
 * @authors Copyright © 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG_DDSTRING_CPP_WRAPPER_HH
#define LIBDENG_DDSTRING_CPP_WRAPPER_HH

#include "dd_string.h"
#include <assert.h>

namespace de {

/**
 * Minimal C++ wrapper for ddstring_t.
 */
class Str {
public:
    Str(const char* text = 0) : refs(1) {
        Str_InitStd(&str);
        if(text) {
            Str_Set(&str, text);
        }
    }
    ~Str() {
        // This should never be called directly.
        assert(!refs);
        Str_Free(&str);
    }
    void addRef() {
        refs++;
    }
    void release() {
        if(--refs == 0) delete this;
    }
    operator const char* (void) const {
        return str.str;
    }
    operator ddstring_t* (void) {
        return &str;
    }
    operator const ddstring_t* (void) const {
        return &str;
    }
private:
    ddstring_t str;
    int refs;
};

} // namespace de

#endif // LIBDENG_DDSTRING_CPP_WRAPPER_HH
