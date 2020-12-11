/*
 * bedit-file-browser-dir-cache.c
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

#include "bedit-file-browser-dir-cache.h"

#include <bedit/bedit-utils.h>
#include <gio/gio.h>
#include <glib/gi18n.h>
#include <string.h>

#include "bedit-file-browser-dir.h"

struct _BeditFileBrowserDirCache {
    GObject parent_instance;
    GFile *file;
    GHashTable *children;
};

G_DEFINE_DYNAMIC_TYPE(
    BeditFileBrowserDirCache, bedit_file_browser_dir_cache,
    G_TYPE_OBJECT
)

enum {
    PROP_0,
    PROP_FILE,
    PROP_IS_LOADING,
    LAST_PROP,
};

static void bedit_file_browser_dir_cache_add_child(
    BeditFileBrowserDirCache *dir_cache, GFile *file
);
static void bedit_file_browser_dir_cache_remove_child(
    BeditFileBrowserDirCache *dir_cache, GFile *file
);

static void bedit_file_browser_dir_cache_get_property(
    GObject *object, guint prop_id, GValue *value, GParamSpec *pspec
) {
    BeditFileBrowserDirCache *dir_cache = BEDIT_FILE_BROWSER_DIR_CACHE(object);

    switch (prop_id) {
    case PROP_FILE:
        g_value_take_object(value, bedit_file_browser_dir_cache_get_file(dir_cache));
        break;

    case PROP_IS_LOADING:
        g_value_set_boolean(value, bedit_file_browser_dir_cache_is_loading(dir_cache));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void bedit_file_browser_dir_cache_set_property(
    GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec
) {
    BeditFileBrowserDirCache *dir_cache = BEDIT_FILE_BROWSER_DIR_CACHE(object);

    switch (prop_id) {
    case PROP_FILE:
        bedit_file_browser_dir_cache_set_file(
            dir_cache, G_FILE(g_value_get_object(value))
        );
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void bedit_file_browser_dir_cache_class_init(BeditFileBrowserDirCacheClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->get_property = bedit_file_browser_dir_cache_get_property;
    object_class->set_property = bedit_file_browser_dir_cache_set_property;

    g_object_class_install_property(
        object_class, PROP_FILE,
        g_param_spec_object(
            "file", "File",
            "The GFile object that this dir_cacheectory wraps",
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

static void bedit_file_browser_dir_cache_class_finalize(
    BeditFileBrowserDirCacheClass *klass
) {}

void bedit_file_browser_dir_cache_init(BeditFileBrowserDirCache *dir_cache) {
    dir_cache->children = g_hash_table_new_full(
        g_file_hash, (GEqualFunc) g_file_equal, g_object_unref, g_object_unref
    );
}


BeditFileBrowserDirCache *bedit_file_browser_dir_cache_new(GFile *file) {
    return BEDIT_FILE_BROWSER_DIR_CACHE(
        g_object_new(BEDIT_TYPE_FILE_BROWSER_DIR_CACHE, "file", file, NULL)
    );
}




void bedit_file_browser_dir_cache_refresh(BeditFileBrowserDirCache *dir_cache) {
    g_return_if_fail(BEDIT_IS_FILE_BROWSER_DIR_CACHE(dir_cache));

    if (dir_cache->cancellable != NULL) {
        // TODO cancel.
    }

    if (dir_cache->children) {
        // TODO free the children.
    }

    // Create enumerator.

    // Trigger async op.

    // Set loading to true.
}


static void on_dir_cacheectory_monitor_event(
    GFileMonitor *monitor, GFile *file, GFile *other_file,
    GFileMonitorEvent event_type, BeditFileBrowserDirCache *dir_cache
) {
    g_return_if_fail(BEDIT_IS_FILE_BROWSER_DIR_CACHE(dir_cache));

    switch (event_type) {

    case G_FILE_MONITOR_EVENT_CREATED:
        bedit_file_browser_dir_cache_add_child(dir_cache, file);
        break;

    case G_FILE_MONITOR_EVENT_DELETED:
        bedit_file_browser_dir_cache_remove_child(dir_cache, file);
        break;

    default:
        break;
    }
}

/**
 * bedit_file_browser_dir_cache_get_file
 *
 * Returns: (transfer none): a pointer to the GFile object for this dir_cacheectory.
 */
GFile *bedit_file_browser_dir_cache_get_file(BeditFileBrowserDirCache *dir_cache) {
    g_return_val_if_fail(BEDIT_IS_FILE_BROWSER_DIR_CACHE(dir_cache), NULL);

    return dir_cache->file;
}

/**
 * bedit_file_browser_dir_cache_set_file
 * @GFile: (transfer none):
 *
 * Should only be called once to set the file to its permanent value.
 */
void bedit_file_browser_dir_cache_set_file(BeditFileBrowserDirCache *dir_cache, GFile *file) {
    g_return_if_fail(BEDIT_IS_FILE_BROWSER_DIR_CACHE(dir_cache));
    g_return_if_fail(G_IS_FILE(file));

    g_return_if_fail(dir_cache->file == NULL || g_file_equal(dir_cache->file, file));

    dir_cache->file = g_object_ref(file);
    bedit_file_browser_dir_cache_refresh(dir_cache);
}

/**
 * bedit_file_browser_dir_cache_iter_init
 * @iter:
 * @dir_cache: (transfer none):
 *
 * |[<!-- language="C" -->
 * BeditFileBrowserDirCacheIter iter;
 * GFile *file;
 * FFileInfo *info
 *
 * bedit_file_browser_dir_cache_iter_init(&iter, dir_cache);
 * while (bedit_file_browser_dir_cache_iter_next(&iter) {
 *     file = bedit_file_browser_dir_cache_iter_file(&iter);
 *     info = bedit_file_browser_dir_cache_iter_info(&iter);
 *
 *     // Do something.
 * }
 * ]|
 */
void bedit_file_browser_dir_cache_iter_init(
    BeditFileBrowserDirCacheIter *iter, BeditFileBrowserDirCache *dir_cache
) {
    g_return_if_fail(iter != NULL);
    g_return_if_fail(BEDIT_IS_FILE_BROWSER_DIR_CACHE(dir_cache));

    g_hash_table_iter_init(&iter->iter, dir_cache->children);
    iter->file = NULL;
    iter->info = NULL;
}

/**
 * bedit_file_browser_dir_cache_iter_next
 * @iter: an initialised #BeditFileBrowserDirCacheIter.
 *
 * Advances iter, returning %TRUE if it still points inside the dir_cacheectory, and
 * %FALSE if it has reached the end.
 *
 * Returns: %FALSE if the end of the #BeditFileBrowserDirCache has been reached.
 */
gboolean bedit_file_browser_dir_cache_iter_next(BeditFileBrowserDirCacheIter *iter) {
    g_return_val_if_fail(iter != NULL, FALSE);

    return g_hash_table_iter_next(
        &iter->iter, (gpointer) &iter->file, (gpointer) &iter->info
    );
}

/**
 * bedit_file_browser_dir_cache_iter_get_file
 * @iter: an initialised and valid #BeditFileBrowserDirCacheIter.
 *
 * Should not be called before calling @bedit_file_browser_dir_cache_iter_next, or
 * after a call to @bedit_file_browser_dir_cache_iter_next returns %FALSE.
 *
 * Returns: (transfer none): the file pointed to by the iterator.
 */
GFile *bedit_file_browser_dir_cache_iter_get_file(BeditFileBrowserDirCacheIter *iter) {
    g_return_val_if_fail(iter != NULL, NULL);
    g_return_val_if_fail(G_IS_FILE(iter->file), NULL);

    return iter->file;
}

/**
 * bedit_file_browser_dir_cache_iter_get_file
 * @iter: an initialised and valid #BeditFileBrowserDirCacheIter.
 *
 * Should not be called before calling @bedit_file_browser_dir_cache_iter_next, or
 * after a call to @bedit_file_browser_dir_cache_iter_next returns %FALSE.
 *
 * Returns: (transfer none): a #GFileInfo object describing the file pointed to
 *     by the iterator.
 */
GFileInfo *bedit_file_browser_dir_cache_iter_get_info(BeditFileBrowserDirCacheIter *iter) {
    g_return_val_if_fail(iter != NULL, NULL);
    g_return_val_if_fail(G_IS_FILE_INFO(iter->file), NULL);

    return iter->info;
}


static void bedit_file_browser_dir_cache_add_child(
    BeditFileBrowserDirCache *dir_cache, GFile *file
) {
    GFileInfo *info;
    GError *error = NULL;

    g_return_if_fail(BEDIT_IS_FILE_BROWSER_DIR_CACHE(dir_cache));
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
            dir_cache->children, g_object_ref(file), g_object_ref(info)
        );

        // TODO Signal insertion.
    }
}

static void bedit_file_browser_dir_cache_remove_child(
    BeditFileBrowserDirCache *dir_cache, GFile *file
) {
    g_return_if_fail(BEDIT_IS_FILE_BROWSER_DIR_CACHE(dir_cache));
    g_return_if_fail(G_IS_FILE(file));

    g_hash_table_remove(dir_cache->children, file);

    // TODO Signal deletion.
}

void _bedit_file_browser_dir_cache_register_type(GTypeModule *type_module) {
    bedit_file_browser_dir_cache_register_type(type_module);
}
