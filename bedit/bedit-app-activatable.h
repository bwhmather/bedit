/*
 * bedit-app-activatable.h
 * This file is part of bedit
 *
 * Copyright (C) 2010 - Steve Frécinaux
 * Copyright (C) 2010 - Jesse van den Kieboom
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

#ifndef BEDIT_APP_ACTIVATABLE_H
#define BEDIT_APP_ACTIVATABLE_H

#include <bedit/bedit-menu-extension.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define BEDIT_TYPE_APP_ACTIVATABLE (bedit_app_activatable_get_type())

G_DECLARE_INTERFACE(
    BeditAppActivatable, bedit_app_activatable, BEDIT, APP_ACTIVATABLE, GObject)

struct _BeditAppActivatableInterface {
    GTypeInterface g_iface;

    /* Virtual public methods */
    void (*activate)(BeditAppActivatable *activatable);
    void (*deactivate)(BeditAppActivatable *activatable);
};

void bedit_app_activatable_activate(BeditAppActivatable *activatable);
void bedit_app_activatable_deactivate(BeditAppActivatable *activatable);

/**
 * bedit_app_activatable_extend_menu:
 * @activatable: A #BeditAppActivatable.
 * @extension_point: the extension point section of the menu to get.
 *
 * Gets the #BeditMenuExtension for the menu @extension_point. Note that
 * the extension point could be in different menus (gear menu, app menu, etc)
 * depending on the platform.
 *
 * Returns: (transfer full): a #BeditMenuExtension for the specific section
 * or %NULL if not found.
 */
BeditMenuExtension *bedit_app_activatable_extend_menu(
    BeditAppActivatable *activatable, const gchar *extension_point);

G_END_DECLS

#endif /* BEDIT_APP_ACTIVATABLE_H */

