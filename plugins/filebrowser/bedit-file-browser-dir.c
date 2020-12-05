/*
 * bedit-file-browser-dir.c
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
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

#include <bedit/bedit-utils.h>
#include <gio/gio.h>
#include <glib/gi18n.h>
#include <string.h>

#include "bedit-file-browser-dir.h"

struct _BeditFileBrowserDir {
    GFile *file;
    GSList *children;

    GCancellable *cancellable;
    GFileMonitor *monitor;
};

G_DEFINE_DYNAMIC_TYPE(
    BeditFileBrowserDir, bedit_file_browser_dir,
    G_TYPE_OBJECT
)

enum {
    PROP_0,
    PROP_FILE,
    PROP_IS_LOADING,
    LAST_PROP,
};



static void bedit_file_browser_dir_get_property(
    GObject *object, guint prop_id, GValue *value, GParamSpec *pspec
) {
    BeditQuickOpenWidget *obj = BEDIT_QUICK_OPEN_WIDGET(object);

    switch (prop_id) {
    case PROP_VIRTUAL_ROOT:
        g_value_take_object(
            value, bedit_file_browser_dir_get_virtual_root(obj)
        );
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void bedit_file_browser_dir_class_init(BeditQuickOpenWidgetClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->get_property = bedit_file_browser_dir_get_property;

    g_object_class_install_property(
        object_class, PROP_FILE,
        g_param_spec_object(
            "file", "File",
            "The GFile object that this directory wraps",
            G_TYPE_FILE, G_PARAM_READ | G_PARAM_STATIC_STRINGS
        )
    );

    g_object_class_install_property(
        object_class, PROP_FILE,
        g_param_spec_object(
            "loading", "Loading",
            "Indicates whether loading has completed",
            G_TYPE_FILE, G_PARAM_READ | G_PARAM_STATIC_STRINGS
        )
    );
}

