/*
 * bedit-file-browser-location.c
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

#include "config.h"

#include "bedit-file-browser-location.h"

#include "bedit-file-browser-bookmarks-store.h"
#include "bedit-file-browser-store.h"

#include <gtk/gtk.h>


struct _BeditFileBrowserLocation {
    GtkMenuButton parent_instance;

    BeditFileBrowserBookmarksStore *bookmarks_store;
    BeditFileBrowserStore *file_store;
};

enum {
    PROP_0,
    PROP_BOOKMARKS_STORE,
    PROP_FILE_STORE,
    LAST_PROP,
};

static GParamSpec *properties[LAST_PROP];

G_DEFINE_DYNAMIC_TYPE(
    BeditFileBrowserLocation, bedit_file_browser_location, GTK_TYPE_MENU_BUTTON
)

static void bedit_file_browser_location_get_property(
    GObject *object, guint prop_id, GValue *value, GParamSpec *pspec
) {
    BeditFileBrowserLocation *widget = BEDIT_FILE_BROWSER_LOCATION(object);

    switch (prop_id) {
    case PROP_BOOKMARKS_STORE:
        g_value_take_object(
            value, bedit_file_browser_location_get_bookmarks_store(widget)
        );
        break;

    case PROP_FILE_STORE:
        g_value_take_object(
            value, bedit_file_browser_location_get_file_store(widget)
        );
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void bedit_file_browser_location_set_property(
    GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec
) {
    BeditFileBrowserLocation *obj = BEDIT_FILE_BROWSER_LOCATION(object);

    switch (prop_id) {
    case PROP_BOOKMARKS_STORE:
        bedit_file_browser_location_set_bookmarks_store(
            obj, BEDIT_FILE_BROWSER_BOOKMARKS_STORE(g_value_dup_object(value))
        );
        break;

    case PROP_FILE_STORE:
        bedit_file_browser_location_set_file_store(
            obj, BEDIT_FILE_BROWSER_STORE(g_value_dup_object(value))
        );
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void bedit_file_browser_location_class_init(
    BeditFileBrowserLocationClass *klass
) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    object_class->get_property = bedit_file_browser_location_get_property;
    object_class->set_property = bedit_file_browser_location_set_property;

    properties[PROP_BOOKMARKS_STORE] = g_param_spec_object(
        "bookmarks-store", "Bookmarks Store",
        "Object that tracks bookmark state",
        BEDIT_TYPE_FILE_BROWSER_BOOKMARKS_STORE,
        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS
    );

    properties[PROP_FILE_STORE] = g_param_spec_object(
        "file-store", "File Store",
        "Object that tracks the current view of the filesystem",
        BEDIT_TYPE_FILE_BROWSER_STORE,
        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS
    );

    g_object_class_install_properties(object_class, LAST_PROP, properties);

    gtk_widget_class_set_template_from_resource(
        widget_class,
        "/com/bwhmather/bedit/plugins/file-browser/ui/"
        "bedit-file-browser-location.ui"
    );

    //gtk_widget_class_bind_template_child(
    //    widget_class, BeditFileBrowserLocation, search_entry
    //);

    //gtk_widget_class_bind_template_child(
    //    widget_class, BeditFileBrowserLocation, cancel_button
    //);
}

static void bedit_file_browser_location_class_finalize(
    BeditFileBrowserLocationClass *klass
) {}


static void bedit_file_browser_location_init(BeditFileBrowserLocation *widget) {
    gtk_widget_init_template(GTK_WIDGET(widget));
}


/**
 * bedit_file_browser_location_new:
 *
 * Creates a new #BeditFileBrowserLocation.
 *
 * Return value: the new #BeditFileBrowserLocation object
 **/
GtkWidget *bedit_file_browser_location_new(void) {
    return GTK_WIDGET(g_object_new(BEDIT_TYPE_FILE_BROWSER_LOCATION, NULL));
}

void bedit_file_browser_location_set_bookmarks_store(
    BeditFileBrowserLocation *widget, BeditFileBrowserBookmarksStore *store
) {
    gboolean updated;

    g_return_if_fail(BEDIT_IS_FILE_BROWSER_LOCATION(widget));
    g_return_if_fail(BEDIT_IS_FILE_BROWSER_BOOKMARKS_STORE(store));

    updated = widget->bookmarks_store != store;
    widget->bookmarks_store = store;

    if (updated) {
        g_object_notify(G_OBJECT(widget), "bookmarks-store");
    }
}

BeditFileBrowserBookmarksStore *bedit_file_browser_location_get_bookmarks_store(
    BeditFileBrowserLocation *widget
) {
    g_return_val_if_fail(BEDIT_IS_FILE_BROWSER_LOCATION(widget), NULL);

    if (widget->bookmarks_store != NULL) {
        g_object_ref(widget->bookmarks_store);
    }

    return widget->bookmarks_store;
}

void bedit_file_browser_location_set_file_store(
    BeditFileBrowserLocation *widget, BeditFileBrowserStore *store
) {
    gboolean updated;

    g_return_if_fail(BEDIT_IS_FILE_BROWSER_LOCATION(widget));
    g_return_if_fail(BEDIT_IS_FILE_BROWSER_STORE(store));

    updated = widget->file_store != store;
    widget->file_store = store;

    if (updated) {
        g_object_notify(G_OBJECT(widget), "file-store");
    }
}

BeditFileBrowserStore *bedit_file_browser_location_get_file_store(
    BeditFileBrowserLocation *widget
) {
    g_return_val_if_fail(BEDIT_IS_FILE_BROWSER_LOCATION(widget), NULL);

    if (widget->file_store != NULL) {
        g_object_ref(widget->file_store);
    }

    return widget->file_store;
}

void _bedit_file_browser_location_register_type(GTypeModule *type_module) {
    bedit_file_browser_location_register_type(type_module);
}
