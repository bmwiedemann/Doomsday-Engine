/** @file mobjthinkerdata.cpp
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "doomsday/world/mobjthinkerdata.h"

using namespace de;

DENG2_PIMPL_NOREF(MobjThinkerData)
{};

MobjThinkerData::MobjThinkerData(mobj_t *mobj)
    : ThinkerData(&mobj->thinker)
    , d(new Instance)
{}

MobjThinkerData::MobjThinkerData(MobjThinkerData const &other)
    : ThinkerData(other)
    , d(new Instance)
{}

Thinker::IData *MobjThinkerData::duplicate() const
{
    return new MobjThinkerData(*this);
}

mobj_t *MobjThinkerData::mobj()
{
    return reinterpret_cast<mobj_t *>(&thinker());
}

mobj_t const *MobjThinkerData::mobj() const
{
    return reinterpret_cast<mobj_t const *>(&thinker());
}