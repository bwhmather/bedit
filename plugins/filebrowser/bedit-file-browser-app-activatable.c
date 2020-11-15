/*
 * bedit-file_browser-app-activatable.c
 * This file is part of bedit
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * bedit is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * bedit is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with bedit. If not, see <http://www.gnu.org/licenses/>.
 */

#include "bedit-file-browser-app-activatable.h"
#include <bedit/bedit-app-activatable.h>
#include <bedit/bedit-app.h>
#include <glib/gi18n.h>
#include <libpeas/peas-object-module.h>

struct _BeditFileBrowserAppActivatable {
    GObject parent;
    BeditApp *app;
} BeditFileBrowserAppActivatablePrivate;

enum { PROP_0, PROP_APP };

static void bedit_app_activatable_iface_init(
    BeditAppActivatableInterface *iface
);

G_DEFINE_DYNAMIC_TYPE_EXTENDED(
    BeditFileBrowserAppActivatable,
    bedit_file_browser_app_activatable,
    G_TYPE_OBJECT, 0,
    G_IMPLEMENT_INTERFACE_DYNAMIC(
        BEDIT_TYPE_APP_ACTIVATABLE, bedit_app_activatable_iface_init
    )
)

static void bedit_file_browser_app_activatable_init(
    BeditFileBrowserAppActivatable *self
) {}

static void bedit_file_browser_app_activatable_set_property(
    GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec
) {
    BeditFileBrowserAppActivatable *activatable;

    activatable = BEDIT_FILE_BROWSER_APP_ACTIVATABLE(object);

    switch (prop_id) {
    case PROP_APP:
        activatable->app = BEDIT_APP(g_value_dup_object(value));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void bedit_file_browser_app_activatable_get_property(
    GObject *object, guint prop_id, GValue *value, GParamSpec *pspec
) {
    BeditFileBrowserAppActivatable *activatable;

    activatable = BEDIT_FILE_BROWSER_APP_ACTIVATABLE(object);

    switch (prop_id) {
    case PROP_APP:
        g_value_set_object(value, activatable->app);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void bedit_file_browser_app_activatable_class_init(
    BeditFileBrowserAppActivatableClass *klass
) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->set_property = bedit_file_browser_app_activatable_set_property;
    object_class->get_property = bedit_file_browser_app_activatable_get_property;

    g_object_class_override_property(object_class, PROP_APP, "app");
}

static void bedit_file_browser_app_activatable_class_finalize(
    BeditFileBrowserAppActivatableClass *klass
) {}

static void bedit_file_browser_app_activatable_activate(
    BeditAppActivatable *app_activatable
) {
    BeditFileBrowserAppActivatable *activatable;
    gchar const *accels[] = {"<Primary><Shift>O", NULL};

    activatable = BEDIT_FILE_BROWSER_APP_ACTIVATABLE(app_activatable);

    gtk_application_set_accels_for_action(
        GTK_APPLICATION(activatable->app), "win.file-browser", accels
    );
}

static void bedit_file_browser_app_activatable_deactivate(
    BeditAppActivatable *app_activatable
) {
    BeditFileBrowserAppActivatable *activatable;
    gchar const *accels[] = {NULL};

    activatable = BEDIT_FILE_BROWSER_APP_ACTIVATABLE(app_activatable);

    gtk_application_set_accels_for_action(
        GTK_APPLICATION(activatable->app), "win.file-browser", accels
    );
}

static void bedit_app_activatable_iface_init(
    BeditAppActivatableInterface *iface
) {
    iface->activate = bedit_file_browser_app_activatable_activate;
    iface->deactivate = bedit_file_browser_app_activatable_deactivate;
}


void _bedit_file_browser_app_activatable_register_type(GTypeModule *type_module) {
    bedit_file_browser_app_activatable_register_type(type_module);
}
