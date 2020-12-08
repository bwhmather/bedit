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

#include "config.h"

#include <bedit/bedit-utils.h>
#include <gio/gio.h>
#include <glib/gi18n.h>
#include <string.h>

#include "bedit-file-browser-dir.h"

#define DIRECTORY_LOAD_ITEMS_PER_CALLBACK 100
#define STANDARD_ATTRIBUTE_TYPES                                            \
    G_FILE_ATTRIBUTE_STANDARD_TYPE ","                                      \
    G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN ","                                 \
    G_FILE_ATTRIBUTE_STANDARD_IS_BACKUP ","                                 \
    G_FILE_ATTRIBUTE_STANDARD_NAME ","                                      \
    G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","                              \
    G_FILE_ATTRIBUTE_STANDARD_SYMBOLIC_ICON

struct _BeditFileBrowserDir {
    GFile *file;
    GHashTable *children;

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

static void bedit_file_browser_dir_add_child(
    BeditFileBrowserDir *dir, GFile *file
);
static void bedit_file_browser_dir_remove_child(
    BeditFileBrowserDir *dir, GFile *file
);

static void bedit_file_browser_dir_get_property(
    GObject *object, guint prop_id, GValue *value, GParamSpec *pspec
) {
    BeditFileBrowserDir *dir = BEDIT_FILE_BROWSER_DIR(object);

    switch (prop_id) {
    case PROP_FILE:
        g_value_take_object(value, bedit_file_browser_dir_get_file(dir));
        break;

    case PROP_IS_LOADING:
        g_value_set_boolean(value, bedit_file_browser_dir_is_loading(dir));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void bedit_file_browser_dir_set_property(
    GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec
) {
    BeditFileBrowserDir *dir = BEDIT_FILE_BROWSER_DIR(object);

    switch (prop_id) {
    case PROP_FILE:
        bedit_file_browser_dir_set_file(
            dir, G_FILE(g_value_get_object(value))
        );
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void bedit_file_browser_dir_class_init(BeditFileBrowserDirClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->get_property = bedit_file_browser_dir_get_property;
    object_class->set_property = bedit_file_browser_dir_set_property;

    g_object_class_install_property(
        object_class, PROP_FILE,
        g_param_spec_object(
            "file", "File",
            "The GFile object that this directory wraps",
            G_TYPE_FILE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS
        )
    );

    g_object_class_install_property(
        object_class, PROP_FILE,
        g_param_spec_object(
            "loading", "Loading",
            "Indicates whether loading has completed",
            G_TYPE_FILE, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS
        )
    );
}

static void bedit_file_browser_dir_class_finalize(
    BeditFileBrowserDirClass *klass
) {}

void bedit_file_browser_dir_init(BeditFileBrowserDir *dir) {
    dir->children = g_hash_table_new_full(
        g_file_hash, (GEqualFunc) g_file_equal, g_object_unref, g_object_unref
    );
}


BeditFileBrowserDir *bedit_file_browser_dir_new(GFile *file) {
    return BEDIT_FILE_BROWSER_DIR(
        g_object_new(BEDIT_TYPE_FILE_BROWSER_DIR, "file", file, NULL)
    );
}




void bedit_file_browser_dir_refresh(BeditFileBrowserDir *dir) {
    g_return_if_fail(BEDIT_IS_FILE_BROWSER_DIR(dir));

    if (dir->cancellable != NULL) {
        // TODO cancel.
    }

    if (dir->children) {
        // TODO free the children.
    }

    // Create enumerator.

    // Trigger async op.

    // Set loading to true.
}


static void on_directory_monitor_event(
    GFileMonitor *monitor, GFile *file, GFile *other_file,
    GFileMonitorEvent event_type, BeditFileBrowserDir *dir
) {
    g_return_if_fail(BEDIT_IS_FILE_BROWSER_DIR(dir));

    switch (event_type) {

    case G_FILE_MONITOR_EVENT_CREATED:
        bedit_file_browser_dir_add_child(dir, file);
        break;

    case G_FILE_MONITOR_EVENT_DELETED:
        bedit_file_browser_dir_remove_child(dir, file);
        break;

    default:
        break;
    }
}

/**
 * bedit_file_browser_dir_get_file
 *
 * Returns: (transfer none): a pointer to the GFile object for this directory.
 */
GFile *bedit_file_browser_dir_get_file(BeditFileBrowserDir *dir) {
    g_return_val_if_fail(BEDIT_IS_FILE_BROWSER_DIR(dir), NULL);

    return dir->file;
}

/**
 * bedit_file_browser_dir_set_file
 * @GFile: (transfer none):
 *
 * Should only be called once to set the file to its permanent value.
 */
void bedit_file_browser_dir_set_file(BeditFileBrowserDir *dir, GFile *file) {
    g_return_if_fail(BEDIT_IS_FILE_BROWSER_DIR(dir));
    g_return_if_fail(G_IS_FILE(file));

    g_return_if_fail(dir->file == NULL || g_file_equal(dir->file, file));

    dir->file = g_object_ref(file);
    bedit_file_browser_dir_refresh(dir);
}

/**
 * bedit_file_browser_dir_iter_init
 * @iter:
 * @dir: (transfer none):
 *
 * |[<!-- language="C" -->
 * BeditFileBrowserDirIter iter;
 * GFile *file;
 * FFileInfo *info
 *
 * bedit_file_browser_dir_iter_init(&iter, dir);
 * while (bedit_file_browser_dir_iter_next(&iter) {
 *     file = bedit_file_browser_dir_iter_file(&iter);
 *     info = bedit_file_browser_dir_iter_info(&iter);
 *
 *     // Do something.
 * }
 * ]|
 */
void bedit_file_browser_dir_iter_init(
    BeditFileBrowserDirIter *iter, BeditFileBrowserDir *dir
) {
    g_return_if_fail(iter != NULL);
    g_return_if_fail(BEDIT_IS_FILE_BROWSER_DIR(dir));

    g_hash_table_iter_init(&iter->iter, dir->children);
    iter->file = NULL;
    iter->info = NULL;
}

/**
 * bedit_file_browser_dir_iter_next
 * @iter: an initialised #BeditFileBrowserDirIter.
 *
 * Advances iter, returning %TRUE if it still points inside the directory, and
 * %FALSE if it has reached the end.
 *
 * Returns: %FALSE if the end of the #BeditFileBrowserDir has been reached.
 */
gboolean bedit_file_browser_dir_iter_next(BeditFileBrowserDirIter *iter) {
    g_return_val_if_fail(iter != NULL, FALSE);

    return g_hash_table_iter_next(
        &iter->iter, (gpointer) &iter->file, (gpointer) &iter->info
    );
}

/**
 * bedit_file_browser_dir_iter_get_file
 * @iter: an initialised and valid #BeditFileBrowserDirIter.
 *
 * Should not be called before calling @bedit_file_browser_dir_iter_next, or
 * after a call to @bedit_file_browser_dir_iter_next returns %FALSE.
 *
 * Returns: (transfer none): the file pointed to by the iterator.
 */
GFile *bedit_file_browser_dir_iter_get_file(BeditFileBrowserDirIter *iter) {
    g_return_val_if_fail(iter != NULL, NULL);
    g_return_val_if_fail(G_IS_FILE(iter->file), NULL);

    return iter->file;
}

/**
 * bedit_file_browser_dir_iter_get_file
 * @iter: an initialised and valid #BeditFileBrowserDirIter.
 *
 * Should not be called before calling @bedit_file_browser_dir_iter_next, or
 * after a call to @bedit_file_browser_dir_iter_next returns %FALSE.
 *
 * Returns: (transfer none): a #GFileInfo object describing the file pointed to
 *     by the iterator.
 */
GFileInfo *bedit_file_browser_dir_iter_get_info(BeditFileBrowserDirIter *iter) {
    g_return_val_if_fail(iter != NULL, NULL);
    g_return_val_if_fail(G_IS_FILE_INFO(iter->file), NULL);

    return iter->info;
}


static void bedit_file_browser_dir_add_child(
    BeditFileBrowserDir *dir, GFile *file
) {
    GFileInfo *info;
    GError *error = NULL;

    g_return_if_fail(BEDIT_IS_FILE_BROWSER_DIR(dir));
    g_return_if_fail(G_IS_FILE(file));

    // TODO Make async.
    info = g_file_query_info(
        file, STANDARD_ATTRIBUTE_TYPES, G_FILE_QUERY_INFO_NONE, NULL, &error
    );

    if (info == NULL) {
        // TODO Should we add the file and defer error handling until later?
        g_warning("Error querying file info: %s", error->message);
        g_error_free(error);
    } else {
        g_hash_table_insert(
            dir->children, g_object_ref(file), g_object_ref(info)
        );

        // TODO Signal insertion.
    }
}

static void bedit_file_browser_dir_remove_child(
    BeditFileBrowserDir *dir, GFile *file
) {
    g_return_if_fail(BEDIT_IS_FILE_BROWSER_DIR(dir));
    g_return_if_fail(G_IS_FILE(file));

    g_hash_table_remove(dir->children, file);

    // TODO Signal deletion.
}

void _bedit_file_browser_dir_register_type(GTypeModule *type_module) {
    bedit_file_browser_dir_register_type(type_module);
}
