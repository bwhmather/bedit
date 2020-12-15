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
#include <gmodule.h>
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

typedef struct _AsyncContext AsyncContext;

struct _BeditFileBrowserDir {
    GObject parent_instance;

    GFile *file;
    GHashTable *children;

    AsyncContext *context;
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

enum {
    SIGNAL_REFRESH,
    SIGNAL_CHILD_ADDED,
    SIGNAL_CHILD_REMOVED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

struct _AsyncContext {
    guint ref_count;
    BeditFileBrowserDir *dir;
    GCancellable *cancellable;
};

static AsyncContext *async_context_new(BeditFileBrowserDir *dir) {
    AsyncContext *context;
    GCancellable *cancellable;

    g_return_val_if_fail(BEDIT_IS_FILE_BROWSER_DIR(dir), NULL);

    cancellable = g_cancellable_new();
    g_return_val_if_fail(G_IS_CANCELLABLE(cancellable), NULL);

    context = g_new(AsyncContext, 1);
    g_return_val_if_fail(context != NULL, NULL);

    context->ref_count = 1;
    context->dir = dir;
    context->cancellable = cancellable;

    return context;
}

static AsyncContext *async_context_ref(AsyncContext *context) {
    g_return_val_if_fail(context->ref_count == 0, context);
    if (g_cancellable_is_cancelled(context->cancellable)) {
        g_warning("adding reference to cancelled context");
    }

    context->ref_count++;

    return context;
}

static void async_context_unref(AsyncContext *context) {
    g_return_if_fail(context != NULL);
    g_return_if_fail(context->ref_count > 0);

    context->ref_count--;

    if (context->ref_count == 0) {
        // Note that the entire reason to have the async context as a separate
        // object is that it allows us to avoid holding a strong reference to
        // the directory.  We do not need to unref.
        context->dir = NULL;

        g_clear_object(&context->cancellable);

        g_free(context);
    }
}

static void bedit_file_browser_dir_set_file(
    BeditFileBrowserDir *dir, GFile *file
);

static void bedit_file_browser_dir_monitor_event_cb(
    GFileMonitor *monitor, GFile *file, GFile *other_file,
    GFileMonitorEvent event_type, BeditFileBrowserDir *dir
);

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

static void bedit_file_browser_dir_constructed(GObject *object) {
    BeditFileBrowserDir *dir;

    dir = BEDIT_FILE_BROWSER_DIR(object);

    if (!G_IS_FILE(dir->file)) {
        g_critical("The directory was not properly constructed");
        return;
    }

    if (g_file_is_native(dir->file)) {
        dir->monitor = g_file_monitor_directory(
            dir->file, G_FILE_MONITOR_NONE, NULL, NULL
        );
        if (dir->monitor != NULL) {
            g_signal_connect(
                dir->monitor, "changed",
                G_CALLBACK(bedit_file_browser_dir_monitor_event_cb), dir
            );
        }
    }

    bedit_file_browser_dir_refresh(dir);

    G_OBJECT_CLASS(bedit_file_browser_dir_parent_class)->constructed(object);
}

static void bedit_file_browser_dir_dispose(GObject *object) {
    BeditFileBrowserDir *dir;

    dir = BEDIT_FILE_BROWSER_DIR(object);

    g_clear_object(&dir->file);
    g_clear_object(&dir->children);

    if (dir->context != NULL) {
        g_cancellable_cancel(dir->context->cancellable);
        async_context_unref(dir->context);
        dir->context = NULL;
    }

    g_clear_object(&dir->monitor);

    G_OBJECT_CLASS(bedit_file_browser_dir_parent_class)->dispose(object);

}

static void bedit_file_browser_dir_class_init(BeditFileBrowserDirClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->constructed = bedit_file_browser_dir_constructed;
    object_class->dispose = bedit_file_browser_dir_dispose;

    object_class->get_property = bedit_file_browser_dir_get_property;
    object_class->set_property = bedit_file_browser_dir_set_property;

    g_object_class_install_property(
        object_class, PROP_FILE,
        g_param_spec_object(
            "file", "File",
            "The GFile object that this directory wraps",
            G_TYPE_FILE,
            G_PARAM_CONSTRUCT_ONLY | G_PARAM_READABLE | G_PARAM_STATIC_STRINGS
        )
    );

    g_object_class_install_property(
        object_class, PROP_IS_LOADING,
        g_param_spec_object(
            "loading", "Loading",
            "Indicates whether loading has completed",
            G_TYPE_FILE, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS
        )
    );

    signals[SIGNAL_REFRESH] = g_signal_new_class_handler(
        "refresh", G_TYPE_FROM_CLASS(klass),
        G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
        G_CALLBACK(bedit_file_browser_dir_refresh),
        NULL, NULL,
        NULL,
        G_TYPE_NONE, 0
    );

    signals[SIGNAL_CHILD_ADDED] = g_signal_new(
        "child-added", G_TYPE_FROM_CLASS(klass),
        G_SIGNAL_RUN_FIRST,
        0,
        NULL, NULL,
        NULL,
        G_TYPE_NONE, 1, G_TYPE_FILE
    );

    signals[SIGNAL_CHILD_REMOVED] = g_signal_new(
        "child-removed", G_TYPE_FROM_CLASS(klass),
        G_SIGNAL_RUN_FIRST,
        0,
        NULL, NULL,
        NULL,
        G_TYPE_NONE, 1, G_TYPE_FILE
    );
}

static void bedit_file_browser_dir_class_finalize(
    BeditFileBrowserDirClass *klass
) {}

void bedit_file_browser_dir_init(BeditFileBrowserDir *dir) {
    dir->file = NULL;
    dir->children = g_hash_table_new_full(
        g_file_hash, (GEqualFunc) g_file_equal, g_object_unref, g_object_unref
    );
    dir->context = NULL;
    dir->monitor = NULL;
}

BeditFileBrowserDir *bedit_file_browser_dir_new(GFile *file) {
    return BEDIT_FILE_BROWSER_DIR(
        g_object_new(BEDIT_TYPE_FILE_BROWSER_DIR, "file", file, NULL)
    );
}

static void bedit_file_browser_dir_next_load_cb(
    GFileEnumerator *enumerator, GAsyncResult *result,
    AsyncContext *context
) {
    BeditFileBrowserDir *dir;
    GError *error = NULL;
    GList *files;

    g_return_if_fail(G_IS_FILE_ENUMERATOR(enumerator));

    files = g_file_enumerator_next_files_finish(
        enumerator, result, &error
    );

    if (g_cancellable_is_cancelled(context->cancellable)) {
        g_object_unref(enumerator);
        g_list_free(files);
        if (error != NULL) g_error_free(error);
        async_context_unref(context);

        return;
    }
    // Note that context does not hold a strong reference to the directory, so
    // it is not safe to dereference this pointer until after the cancellable
    // has been checked.
    dir = context->dir;
    g_return_if_fail(BEDIT_IS_FILE_BROWSER_DIR(dir));

    if (error != NULL) {
        // TODO
        // bedit_file_browser_dir_set_error(dir, error);
        // bedit_file_browser_dir_unload(dir);

        g_error_free(error);
        g_object_unref(enumerator);
        async_context_unref(context);

        return;
    }

    if (files == NULL) {
        // We've reached the end of the enumerator and can close everything up.
        // TODO mark as finished.
        g_object_unref(enumerator);
        async_context_unref(context);

        return;
    }

    for (GList *cursor = files; cursor != NULL; cursor = cursor->next) {
        GFile *file;
        GFileInfo *info;
        gchar const *name;

        info = cursor->data;
        name = g_file_info_get_name(info);
        file = g_file_get_child(dir->file, name);

        bedit_file_browser_dir_add_child(dir, file);
    }

    g_list_free(files);

    g_file_enumerator_next_files_async(
        enumerator, DIRECTORY_LOAD_ITEMS_PER_CALLBACK, G_PRIORITY_DEFAULT,
        context->cancellable,
        (GAsyncReadyCallback)bedit_file_browser_dir_next_load_cb,
        context
    );
}

static void bedit_file_browser_dir_begin_load_cb(
    GFile *file, GAsyncResult *result, AsyncContext *context
) {
    BeditFileBrowserDir *dir;
    GError *error = NULL;
    GFileEnumerator *enumerator;

    g_return_if_fail(G_IS_FILE(file));

    enumerator = g_file_enumerate_children_finish(file, result, &error);

    if (g_cancellable_is_cancelled(context->cancellable)) {
        if (enumerator != NULL) g_object_unref(enumerator);
        if (error) g_error_free(error);

        return;
    }
    // Note that context does not hold a strong reference to the directory, so
    // it is not safe to dereference this pointer until after the cancellable
    // has been checked.
    dir = context->dir;
    g_return_if_fail(BEDIT_IS_FILE_BROWSER_DIR(dir));

    if (error != NULL) {
        // TODO
        // bedit_file_browser_dir_set_error(dir, error);
        // bedit_file_browser_dir_unload(dir);

        g_error_free(error);
        async_context_unref(context);

        return;
    }

    g_return_if_fail(G_IS_FILE_ENUMERATOR(enumerator));

    g_file_enumerator_next_files_async(
        enumerator, DIRECTORY_LOAD_ITEMS_PER_CALLBACK, G_PRIORITY_DEFAULT,
        context->cancellable,
        (GAsyncReadyCallback)bedit_file_browser_dir_next_load_cb,
        context
    );
}

void bedit_file_browser_dir_refresh(BeditFileBrowserDir *dir) {
    g_return_if_fail(BEDIT_IS_FILE_BROWSER_DIR(dir));

    if (dir->context != NULL) {
        g_cancellable_cancel(dir->context->cancellable);
        async_context_unref(dir->context);
    }
    dir->context = async_context_new(dir);

    // Set loading to true.

    // Free the children.
    while (TRUE) {
        GHashTableIter child_iter;
        GFile *child;

        g_hash_table_iter_init(&child_iter, dir->children);
        if (!g_hash_table_iter_next(&child_iter, (gpointer) &child, NULL)) {
            break;
        }

        bedit_file_browser_dir_remove_child(dir, child);
    }

    g_file_enumerate_children_async(
        dir->file, STANDARD_ATTRIBUTE_TYPES, G_FILE_QUERY_INFO_NONE,
        G_PRIORITY_DEFAULT, dir->context->cancellable,
        (GAsyncReadyCallback)bedit_file_browser_dir_begin_load_cb,
        async_context_ref(dir->context)
    );
}


static void bedit_file_browser_dir_monitor_event_cb(
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

static void bedit_file_browser_dir_set_file(BeditFileBrowserDir *dir, GFile *file) {
    g_return_if_fail(BEDIT_IS_FILE_BROWSER_DIR(dir));
    g_return_if_fail(dir->file == NULL);

    dir->file = g_object_ref(file);
}

gboolean bedit_file_browser_dir_is_loading(BeditFileBrowserDir *dir) {
    g_return_val_if_fail(BEDIT_IS_FILE_BROWSER_DIR(dir), FALSE);
    if (dir->context == NULL) {
        return FALSE;
    }
    if (dir->context->cancellable == NULL) {
        return FALSE;
    }
    // TODO
    return TRUE;
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

        g_signal_emit(dir, signals[SIGNAL_CHILD_ADDED], 0, file);
    }
}

static void bedit_file_browser_dir_remove_child(
    BeditFileBrowserDir *dir, GFile *file
) {
    g_return_if_fail(BEDIT_IS_FILE_BROWSER_DIR(dir));
    g_return_if_fail(G_IS_FILE(file));

    g_hash_table_remove(dir->children, file);

    g_signal_emit(dir, signals[SIGNAL_CHILD_REMOVED], 0, file);
}

void _bedit_file_browser_dir_register_type(GTypeModule *type_module) {
    bedit_file_browser_dir_register_type(type_module);
}
