/** @file submenuitem.h
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBAPPFW_UI_SUBMENUITEM_H
#define LIBAPPFW_UI_SUBMENUITEM_H

#include "../ui/defs.h"
#include "item.h"
#include "listdata.h"

namespace de {
namespace ui {

/**
 * UI context item that contains items for a submenu.
 */
class LIBAPPFW_PUBLIC SubmenuItem : public Item
{
public:
    SubmenuItem(String const &label, ui::Direction openingDirection)
        : Item(ShownAsButton, label), _dir(openingDirection) {}

    Data &items() { return _items; }
    Data const &items() const { return _items; }
    ui::Direction openingDirection() const { return _dir; }

private:
    ListData _items;
    ui::Direction _dir;
};

} // namespace ui
} // namespace de

#endif // LIBAPPFW_UI_SUBMENUITEM_H