/*
 * bedit-file-browser-search-root-dir-enumerator.c
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

#include "bedit-file-browser-search-root-dir-enumerator.h"

#include <gio/gio.h>
#include <glib-object.h>
#include <gmodule.h>

#include "bedit/bedit-debug.h"

#include "bedit-file-browser-search-dir-enumerator.h"


struct _BeditFileBrowserSearchRootDirEnumerator {
    GObject parent_instance;

    GFile *root;
    gchar *prefix;
    gboolean consumed;
};

enum {
    PROP_0,
    PROP_ROOT,
    PROP_PREFIX,
    LAST_PROP,
};

static void bedit_file_browser_search_root_dir_enumerator_iface_init(
    BeditFileBrowserSearchDirEnumeratorInterface *interface
);
static gboolean bedit_file_browser_search_root_dir_enumerator_iterate(
    BeditFileBrowserSearchDirEnumerator *enumerator,
    GFile **dir,
    gchar **path_markup,
    GCancellable *cancellable,
    GError **error
);

G_DEFINE_DYNAMIC_TYPE_EXTENDED(
    BeditFileBrowserSearchRootDirEnumerator,
    bedit_file_browser_search_root_dir_enumerator,
    G_TYPE_OBJECT, 0,
    G_IMPLEMENT_INTERFACE_DYNAMIC(
        BEDIT_TYPE_FILE_BROWSER_SEARCH_DIR_ENUMERATOR,
        bedit_file_browser_search_root_dir_enumerator_iface_init
    )
)


static void bedit_file_browser_search_root_dir_enumerator_get_property(
    GObject *object, guint prop_id, GValue *value, GParamSpec *pspec
) {
    BeditFileBrowserSearchRootDirEnumerator *enumerator;

    enumerator = BEDIT_FILE_BROWSER_SEARCH_ROOT_DIR_ENUMERATOR(object);

    switch (prop_id) {
    case PROP_ROOT:
        g_value_take_object(value, enumerator->root);
        break;

    case PROP_PREFIX:
        g_value_take_string(value, enumerator->prefix);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void bedit_file_browser_search_root_dir_enumerator_set_property(
    GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec
) {
    BeditFileBrowserSearchRootDirEnumerator *enumerator;

    enumerator = BEDIT_FILE_BROWSER_SEARCH_ROOT_DIR_ENUMERATOR(object);

    switch (prop_id) {
    case PROP_ROOT:
        enumerator->root = G_FILE(g_value_dup_object(value));
        break;

    case PROP_PREFIX:
        enumerator->prefix = g_value_dup_string(value);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void bedit_file_browser_search_root_dir_enumerator_class_init(
    BeditFileBrowserSearchRootDirEnumeratorClass *klass
) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->get_property = bedit_file_browser_search_root_dir_enumerator_get_property;
    object_class->set_property = bedit_file_browser_search_root_dir_enumerator_set_property;

    g_object_class_install_property(
        object_class, PROP_ROOT,
        g_param_spec_object(
            "root", "Root",
            "The location in the filesystem that this enumerator will yield",
            G_TYPE_FILE,
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY
        )
    );

    g_object_class_install_property(
        object_class, PROP_PREFIX,
        g_param_spec_string(
            "prefix", "Prefix",
            "Markup string describing the location of the root",
            "",
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY
        )
    );
}

static void bedit_file_browser_search_root_dir_enumerator_iface_init(
    BeditFileBrowserSearchDirEnumeratorInterface *interface
) {
    interface->iterate = bedit_file_browser_search_root_dir_enumerator_iterate;
}

static void bedit_file_browser_search_root_dir_enumerator_class_finalize(
    BeditFileBrowserSearchRootDirEnumeratorClass *klass
) {}

static void bedit_file_browser_search_root_dir_enumerator_init(
    BeditFileBrowserSearchRootDirEnumerator *enumerator
) {
    enumerator->root = NULL;
    enumerator->prefix = NULL;
    enumerator->consumed = FALSE;
}

/**
 * bedit_file_browser_search_root_dir_enumerator_new:
 *
 * Creates a new #BeditFileBrowserSearchRootDirEnumerator.
 *
 * Return value: the new #BeditFileBrowserSearchRootDirEnumerator object
 **/
BeditFileBrowserSearchRootDirEnumerator
*bedit_file_browser_search_root_dir_enumerator_new(
    GFile *root, gchar const *prefix
) {
    return g_object_new(
        BEDIT_TYPE_FILE_BROWSER_SEARCH_ROOT_DIR_ENUMERATOR,
        "root", root, "prefix", prefix,
        NULL
    );
}

static gboolean bedit_file_browser_search_root_dir_enumerator_iterate(
    BeditFileBrowserSearchDirEnumerator *enumerator,
    GFile **dir,
    gchar **path_markup,
    GCancellable *cancellable,
    GError **error
) {
    BeditFileBrowserSearchRootDirEnumerator *root_enumerator;

    root_enumerator = BEDIT_FILE_BROWSER_SEARCH_ROOT_DIR_ENUMERATOR(enumerator);

    if (root_enumerator->consumed) {
        return FALSE;
    }

    if (dir != NULL) {
        *dir = g_object_ref(root_enumerator->root);
    }

    if (path_markup != NULL) {
        *path_markup = g_strdup(root_enumerator->prefix);
    }

    root_enumerator->consumed = TRUE;
    return TRUE;
}

void _bedit_file_browser_search_root_dir_enumerator_register_type(
    GTypeModule *type_module
) {
    bedit_file_browser_search_root_dir_enumerator_register_type(type_module);
}
