/*
 * bedit-window-activatable.h
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * Based on gedit-window-activatable.h from Gedit.
 *
 * Copyright (C) 2010 - Steve Frécinaux
 * Copyright (C) 2010-2013 - Ignacio Casal Quinteiro
 * Copyright (C) 2014 - Daniel Mustieles
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

#ifndef BEDIT_WINDOW_ACTIVATABLE_H
#define BEDIT_WINDOW_ACTIVATABLE_H

#include <glib-object.h>

G_BEGIN_DECLS

#define BEDIT_TYPE_WINDOW_ACTIVATABLE (bedit_window_activatable_get_type())

G_DECLARE_INTERFACE(
    BeditWindowActivatable, bedit_window_activatable,
    BEDIT, WINDOW_ACTIVATABLE,
    GObject
)

struct _BeditWindowActivatableInterface {
    GTypeInterface g_iface;

    /* Virtual public methods */
    void (*activate)(BeditWindowActivatable *activatable);
    void (*deactivate)(BeditWindowActivatable *activatable);
    void (*update_state)(BeditWindowActivatable *activatable);
};

void bedit_window_activatable_activate(BeditWindowActivatable *activatable);
void bedit_window_activatable_deactivate(BeditWindowActivatable *activatable);
void bedit_window_activatable_update_state(BeditWindowActivatable *activatable);

G_END_DECLS

#endif /* BEDIT_WINDOW_ACTIVATABLE_H */

