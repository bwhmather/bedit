/*
 * bedit-menu-extension.h
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * Based on gedit-menu-extension.h from Gedit.
 *
 * Copyright (C) 2013 - Ignacio Casal Quinteiro
 * Copyright (C) 2014-2015 - Paolo Borelli
 * Copyright (C) 2016 - Sébastien Wilmet
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef BEDIT_MENU_EXTENSION_H
#define BEDIT_MENU_EXTENSION_H

#include <gio/gio.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define BEDIT_TYPE_MENU_EXTENSION (bedit_menu_extension_get_type())

G_DECLARE_FINAL_TYPE(
    BeditMenuExtension, bedit_menu_extension, BEDIT, MENU_EXTENSION, GObject
)

BeditMenuExtension *bedit_menu_extension_new(GMenu *menu);

void bedit_menu_extension_append_menu_item(
    BeditMenuExtension *menu, GMenuItem *item
);

void bedit_menu_extension_prepend_menu_item(
    BeditMenuExtension *menu, GMenuItem *item
);

void bedit_menu_extension_remove_items(BeditMenuExtension *menu);

G_END_DECLS

#endif /* BEDIT_MENU_EXTENSION_H */

