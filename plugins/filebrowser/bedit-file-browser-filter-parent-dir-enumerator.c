/*
 * bedit-file-browser-filter-parent-dir-enumerator.c
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

#include "bedit-file-browser-filter-parent-dir-enumerator.h"

#include <gio/gio.h>
#include <glib-object.h>
#include <gmodule.h>

#include "bedit/bedit-debug.h"

#include "bedit-file-browser-filter-dir-enumerator.h"
#include "bedit-file-browser-filter-match.h"

struct _BeditFileBrowserFilterParentDirEnumerator {
    GObject parent_instance;

    BeditFileBrowserFilterDirEnumerator *source;

    GHashTable *visited;
};

enum {
    PROP_0,
    PROP_SOURCE,
    LAST_PROP,
};

static void bedit_file_browser_filter_parent_dir_enumerator_iface_init(
    BeditFileBrowserFilterDirEnumeratorInterface *interface
);
static gboolean bedit_file_browser_filter_parent_dir_enumerator_iterate(
    BeditFileBrowserFilterDirEnumerator *enumerator,
    GFile **dir,
    gchar **path_markup,
    GCancellable *cancellable,
    GError **error
);

G_DEFINE_DYNAMIC_TYPE_EXTENDED(
    BeditFileBrowserFilterParentDirEnumerator,
    bedit_file_browser_filter_parent_dir_enumerator,
    G_TYPE_OBJECT, 0,
    G_IMPLEMENT_INTERFACE_DYNAMIC(
        BEDIT_TYPE_FILE_BROWSER_FILTER_DIR_ENUMERATOR,
        bedit_file_browser_filter_parent_dir_enumerator_iface_init
    )
)

static void bedit_file_browser_filter_parent_dir_enumerator_get_property(
    GObject *object, guint prop_id, GValue *value, GParamSpec *pspec
) {
    BeditFileBrowserFilterParentDirEnumerator *enumerator;

    enumerator = BEDIT_FILE_BROWSER_FILTER_PARENT_DIR_ENUMERATOR(object);

    switch (prop_id) {
    case PROP_SOURCE:
        g_value_take_object(value, enumerator->source);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void bedit_file_browser_filter_parent_dir_enumerator_set_property(
    GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec
) {
    BeditFileBrowserFilterParentDirEnumerator *enumerator;

    enumerator = BEDIT_FILE_BROWSER_FILTER_PARENT_DIR_ENUMERATOR(object);

    switch (prop_id) {
    case PROP_SOURCE:
        enumerator->source = BEDIT_FILE_BROWSER_FILTER_DIR_ENUMERATOR(
            g_value_dup_object(value)
        );
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void bedit_file_browser_filter_parent_dir_enumerator_class_init(
    BeditFileBrowserFilterParentDirEnumeratorClass *klass
) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->get_property = bedit_file_browser_filter_parent_dir_enumerator_get_property;
    object_class->set_property = bedit_file_browser_filter_parent_dir_enumerator_set_property;

    g_object_class_install_property(
        object_class, PROP_SOURCE,
        g_param_spec_object(
            "source", "Source directory enumerator",
            "Enumerator that yields directories to filter in",
            BEDIT_TYPE_FILE_BROWSER_FILTER_DIR_ENUMERATOR,
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY
        )
    );
}

static void bedit_file_browser_filter_parent_dir_enumerator_iface_init(
    BeditFileBrowserFilterDirEnumeratorInterface *interface
) {
    interface->iterate = bedit_file_browser_filter_parent_dir_enumerator_iterate;
}

static void bedit_file_browser_filter_parent_dir_enumerator_class_finalize(
    BeditFileBrowserFilterParentDirEnumeratorClass *klass
) {}

static void bedit_file_browser_filter_parent_dir_enumerator_init(
    BeditFileBrowserFilterParentDirEnumerator *enumerator
) {
    enumerator->source = NULL;
    enumerator->visited = g_hash_table_new_full(
        g_file_hash, (GEqualFunc) g_file_equal, g_object_unref, NULL
    );
}

BeditFileBrowserFilterParentDirEnumerator
*bedit_file_browser_filter_parent_dir_enumerator_new(
    BeditFileBrowserFilterDirEnumerator *source
) {
    return g_object_new(
        BEDIT_TYPE_FILE_BROWSER_FILTER_PARENT_DIR_ENUMERATOR,
        "source", source,
        NULL
    );
}

static gboolean bedit_file_browser_filter_parent_dir_enumerator_iterate(
    BeditFileBrowserFilterDirEnumerator *instance,
    GFile **out_dir,
    gchar **out_markup,
    GCancellable *cancellable,
    GError **error
) {
    BeditFileBrowserFilterParentDirEnumerator *enumerator;

    enumerator = BEDIT_FILE_BROWSER_FILTER_PARENT_DIR_ENUMERATOR(instance);

    if (g_cancellable_is_cancelled(cancellable)) {
        *error = g_error_new(
            G_IO_ERROR, G_IO_ERROR_CANCELLED,
            "Operation was cancelled"
        );
        return FALSE;
    }

    while (TRUE) {
        GFile *dir_file;
        gchar *dir_markup;
        GFile *parent_file;

        if (!bedit_file_browser_filter_dir_enumerator_iterate(
            enumerator->source,
            &dir_file,
            &dir_markup,
            cancellable, error
        )) {
            return FALSE;
        };

        parent_file = g_file_get_parent(dir_file);
        if (!g_hash_table_contains(enumerator->visited, parent_file)) {
            g_hash_table_add(enumerator->visited, parent_file);

            if (out_dir != NULL) {
                *out_dir = g_object_ref(parent_file);
            }

            if (out_markup != NULL) {
                *out_markup = g_strdup_printf("%s../", (gchar *) dir_markup);
            }

            g_object_unref(dir_file);
            g_free(dir_markup);

            return TRUE;
        }

        g_clear_object(&parent_file);

        g_object_unref(dir_file);
        g_free(dir_markup);
    }
}

void _bedit_file_browser_filter_parent_dir_enumerator_register_type(
    GTypeModule *type_module
) {
    bedit_file_browser_filter_parent_dir_enumerator_register_type(type_module);
}
