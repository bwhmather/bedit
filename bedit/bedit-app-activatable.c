/*
 * bedit-app-activatable.c
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * Based on gedit-app-activatable.c from Gedit.
 *
 * Copyright (C) 2010 - Steve Frécinaux
 * Copyright (C) 2010 - Jesse van den Kieboom
 * Copyright (C) 2014 - Paolo Borelli, Robert Roth
 * Copyright (C) 2015 - Garrett Regier
 * Copyright (C) 2015-2019 - Sébastien Wilmet
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

#include "config.h"

#include "bedit-app-activatable.h"
#include "bedit-app-private.h"
#include "bedit-app.h"

/**
 * SECTION:bedit-app-activatable
 * @short_description: Interface for activatable extensions on apps
 * @see_also: #PeasExtensionSet
 *
 * #BeditAppActivatable is an interface which should be implemented by
 * extensions that should be activated on a bedit application.
 **/

G_DEFINE_INTERFACE(BeditAppActivatable, bedit_app_activatable, G_TYPE_OBJECT)

static void bedit_app_activatable_default_init(
    BeditAppActivatableInterface *iface) {
    /**
     * BeditAppActivatable:app:
     *
     * The app property contains the bedit app for this
     * #BeditAppActivatable instance.
     */
    g_object_interface_install_property(
        iface,
        g_param_spec_object(
            "app", "App", "The bedit app", BEDIT_TYPE_APP,
            G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                G_PARAM_STATIC_STRINGS));
}

/**
 * bedit_app_activatable_activate:
 * @activatable: A #BeditAppActivatable.
 *
 * Activates the extension on the application.
 */
void bedit_app_activatable_activate(BeditAppActivatable *activatable) {
    BeditAppActivatableInterface *iface;

    g_return_if_fail(BEDIT_IS_APP_ACTIVATABLE(activatable));

    iface = BEDIT_APP_ACTIVATABLE_GET_IFACE(activatable);

    if (iface->activate != NULL) {
        iface->activate(activatable);
    }
}

/**
 * bedit_app_activatable_deactivate:
 * @activatable: A #BeditAppActivatable.
 *
 * Deactivates the extension from the application.
 *
 */
void bedit_app_activatable_deactivate(BeditAppActivatable *activatable) {
    BeditAppActivatableInterface *iface;

    g_return_if_fail(BEDIT_IS_APP_ACTIVATABLE(activatable));

    iface = BEDIT_APP_ACTIVATABLE_GET_IFACE(activatable);

    if (iface->deactivate != NULL) {
        iface->deactivate(activatable);
    }
}

BeditMenuExtension *bedit_app_activatable_extend_menu(
    BeditAppActivatable *activatable, const gchar *extension_point) {
    BeditApp *app;
    BeditMenuExtension *ext;

    g_return_val_if_fail(BEDIT_IS_APP_ACTIVATABLE(activatable), NULL);

    g_object_get(G_OBJECT(activatable), "app", &app, NULL);
    ext = _bedit_app_extend_menu(app, extension_point);
    g_object_unref(app);

    return ext;
}

