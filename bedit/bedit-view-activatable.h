/*
 * bedit-view-activatable.h
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * Based on gedit-view-activatable.h from Gedit.
 *
 * Copyright (C) 2010 - Ignacio Casal Quinteiro, Steve Frécinaux
 * Copyright (C) 2015 - Paolo Borelli
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

#ifndef BEDIT_VIEW_ACTIVATABLE_H
#define BEDIT_VIEW_ACTIVATABLE_H

#include <glib-object.h>

G_BEGIN_DECLS

#define BEDIT_TYPE_VIEW_ACTIVATABLE (bedit_view_activatable_get_type())

G_DECLARE_INTERFACE(
    BeditViewActivatable, bedit_view_activatable, BEDIT, VIEW_ACTIVATABLE,
    GObject)

struct _BeditViewActivatableInterface {
    GTypeInterface g_iface;

    /* Virtual public methods */
    void (*activate)(BeditViewActivatable *activatable);
    void (*deactivate)(BeditViewActivatable *activatable);
};

void bedit_view_activatable_activate(BeditViewActivatable *activatable);
void bedit_view_activatable_deactivate(BeditViewActivatable *activatable);

G_END_DECLS

#endif /* BEDIT_VIEW_ACTIVATABLE_H */

