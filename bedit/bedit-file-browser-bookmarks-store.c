/*
 * bedit-file-browser-bookmarks-store.c
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * Based on gedit-file-bookmarks-store.c from Gedit.
 *
 * Copyright (C) 2006 - Jesse van den Kieboom <jesse@icecrew.nl>
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
#include <string.h>

#include <gio/gio.h>
#include <glib/gi18n.h>

#include "bedit-file-browser-bookmarks-store.h"
#include "bedit-file-browser-utils.h"
#include "bedit-utils.h"

struct _BeditFileBrowserBookmarksStore {
    GtkTreeStore parent_instance;

    GVolumeMonitor *volume_monitor;
    GFileMonitor *bookmarks_monitor;
};

G_DEFINE_TYPE(
    BeditFileBrowserBookmarksStore, bedit_file_browser_bookmarks_store,
    GTK_TYPE_TREE_STORE
)

static void remove_node(GtkTreeModel *model, GtkTreeIter *iter);

static void on_fs_changed(
    GVolumeMonitor *monitor, GObject *object,
    BeditFileBrowserBookmarksStore *model
);

static void on_bookmarks_file_changed(
    GFileMonitor *monitor, GFile *file, GFile *other_file,
    GFileMonitorEvent event_type,
    BeditFileBrowserBookmarksStore *model
);
static gboolean find_with_flags(
    GtkTreeModel *model, GtkTreeIter *iter, gpointer obj,
    guint flags, guint notflags
);

static void bedit_file_browser_bookmarks_store_dispose(GObject *object) {
    BeditFileBrowserBookmarksStore *store;

    store = BEDIT_FILE_BROWSER_BOOKMARKS_STORE(object);

    if (store->volume_monitor != NULL) {
        g_signal_handlers_disconnect_by_func(
            store->volume_monitor, on_fs_changed, store
        );

        g_object_unref(store->volume_monitor);
        store->volume_monitor = NULL;
    }

    g_clear_object(&store->bookmarks_monitor);

    G_OBJECT_CLASS(bedit_file_browser_bookmarks_store_parent_class)
        ->dispose(object);
}

static void bedit_file_browser_bookmarks_store_class_init(
    BeditFileBrowserBookmarksStoreClass *klass
) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->dispose = bedit_file_browser_bookmarks_store_dispose;
}

static void bedit_file_browser_bookmarks_store_init(
    BeditFileBrowserBookmarksStore *store
) {}

/* Private */
static void add_node(
    BeditFileBrowserBookmarksStore *model,
    GdkPixbuf *pixbuf, const gchar *icon_name,
    const gchar *name, GObject *store,
    guint flags, GtkTreeIter *iter
) {
    GtkTreeIter newiter;

    gtk_tree_store_append(GTK_TREE_STORE(model), &newiter, NULL);

    gtk_tree_store_set(
        GTK_TREE_STORE(model), &newiter,
        BEDIT_FILE_BROWSER_BOOKMARKS_STORE_COLUMN_ICON, pixbuf,
        BEDIT_FILE_BROWSER_BOOKMARKS_STORE_COLUMN_ICON_NAME, icon_name,
        BEDIT_FILE_BROWSER_BOOKMARKS_STORE_COLUMN_NAME, name,
        BEDIT_FILE_BROWSER_BOOKMARKS_STORE_COLUMN_OBJECT, store,
        BEDIT_FILE_BROWSER_BOOKMARKS_STORE_COLUMN_FLAGS, flags,
        -1
    );

    if (iter != NULL) {
        *iter = newiter;
    }
}

static gboolean add_file(
    BeditFileBrowserBookmarksStore *model,
    GFile *file, const gchar *name,
    guint flags,
    GtkTreeIter *iter
) {
    gboolean native = g_file_is_native(file);
    gchar *icon_name = NULL;
    gchar *newname;

    if (native && !g_file_query_exists(file, NULL)) {
        return FALSE;
    }

    if (flags & BEDIT_FILE_BROWSER_BOOKMARKS_STORE_IS_HOME) {
        icon_name = g_strdup("user-home");
    } else if (flags & BEDIT_FILE_BROWSER_BOOKMARKS_STORE_IS_DESKTOP) {
        icon_name = g_strdup("user-desktop");
    } else if (flags & BEDIT_FILE_BROWSER_BOOKMARKS_STORE_IS_ROOT) {
        icon_name = g_strdup("drive-harddisk");
    } else {
        /* getting the icon is a sync get_info call, so we just do it for local
         * files */
        if (native) {
            icon_name = bedit_file_browser_utils_icon_name_from_file(file);
        } else {
            icon_name = g_strdup("folder");
        }
    }

    if (name == NULL) {
        newname = bedit_file_browser_utils_file_basename(file);
    } else {
        newname = g_strdup(name);
    }

    add_node(model, NULL, icon_name, newname, G_OBJECT(file), flags, iter);

    g_free(icon_name);
    g_free(newname);

    return TRUE;
}

static void check_mount_separator(
    BeditFileBrowserBookmarksStore *model, guint flags, gboolean added
) {
    GtkTreeIter iter;
    gboolean found = find_with_flags(
        GTK_TREE_MODEL(model), &iter, NULL,
        flags | BEDIT_FILE_BROWSER_BOOKMARKS_STORE_IS_SEPARATOR, 0
    );

    if (added && !found) {
        /* Add the separator */
        add_node(
            model, NULL, NULL, NULL, NULL,
            flags | BEDIT_FILE_BROWSER_BOOKMARKS_STORE_IS_SEPARATOR, NULL
        );
    } else if (!added && found) {
        remove_node(GTK_TREE_MODEL(model), &iter);
    }
}

static void init_special_directories(BeditFileBrowserBookmarksStore *model) {
    gchar const *path = g_get_home_dir();
    GFile *file;

    if (path != NULL) {
        file = g_file_new_for_path(path);
        add_file(
            model, file, _("Home"),
            BEDIT_FILE_BROWSER_BOOKMARKS_STORE_IS_HOME |
            BEDIT_FILE_BROWSER_BOOKMARKS_STORE_IS_SPECIAL_DIR,
            NULL
        );
        g_object_unref(file);
    }

#if defined(G_OS_WIN32)
    path = g_get_user_special_dir(G_USER_DIRECTORY_DESKTOP);
    if (path != NULL) {
        file = g_file_new_for_path(path);
        add_file(
            model, file, NULL,
            BEDIT_FILE_BROWSER_BOOKMARKS_STORE_IS_DESKTOP |
            BEDIT_FILE_BROWSER_BOOKMARKS_STORE_IS_SPECIAL_DIR,
            NULL
        );
        g_object_unref(file);
    }

    path = g_get_user_special_dir(G_USER_DIRECTORY_DOCUMENTS);
    if (path != NULL) {
        file = g_file_new_for_path(path);
        add_file(
            model, file, NULL,
            BEDIT_FILE_BROWSER_BOOKMARKS_STORE_IS_DOCUMENTS |
            BEDIT_FILE_BROWSER_BOOKMARKS_STORE_IS_SPECIAL_DIR,
            NULL
        );
        g_object_unref(file);
    }
#endif

    file = g_file_new_for_uri("file:///");
    add_file(
        model,
        file, _("File System"),
        BEDIT_FILE_BROWSER_BOOKMARKS_STORE_IS_ROOT,
        NULL
    );
    g_object_unref(file);

    check_mount_separator(
        model, BEDIT_FILE_BROWSER_BOOKMARKS_STORE_IS_ROOT, TRUE
    );
}

static void get_fs_properties(
    gpointer fs, gchar **name, gchar **icon_name, guint *flags
) {
    GIcon *icon = NULL;

    *flags = BEDIT_FILE_BROWSER_BOOKMARKS_STORE_IS_FS;
    *name = NULL;
    *icon_name = NULL;

    if (G_IS_DRIVE(fs)) {
        icon = g_drive_get_icon(G_DRIVE(fs));
        *name = g_drive_get_name(G_DRIVE(fs));
        *icon_name = bedit_file_browser_utils_name_from_themed_icon(icon);

        *flags |= BEDIT_FILE_BROWSER_BOOKMARKS_STORE_IS_DRIVE;
    } else if (G_IS_VOLUME(fs)) {
        icon = g_volume_get_icon(G_VOLUME(fs));
        *name = g_volume_get_name(G_VOLUME(fs));
        *icon_name = bedit_file_browser_utils_name_from_themed_icon(icon);

        *flags |= BEDIT_FILE_BROWSER_BOOKMARKS_STORE_IS_VOLUME;
    } else if (G_IS_MOUNT(fs)) {
        icon = g_mount_get_icon(G_MOUNT(fs));
        *name = g_mount_get_name(G_MOUNT(fs));
        *icon_name = bedit_file_browser_utils_name_from_themed_icon(icon);

        *flags |= BEDIT_FILE_BROWSER_BOOKMARKS_STORE_IS_MOUNT;
    }

    if (icon) {
        g_object_unref(icon);
    }
}

static void add_fs(
    BeditFileBrowserBookmarksStore *model, gpointer fs, guint flags,
    GtkTreeIter *iter
) {
    gchar *icon_name = NULL;
    gchar *name = NULL;
    guint fsflags;

    get_fs_properties(fs, &name, &icon_name, &fsflags);
    add_node(model, NULL, icon_name, name, fs, flags | fsflags, iter);

    g_free(name);
    g_free(icon_name);
    check_mount_separator(
        model, BEDIT_FILE_BROWSER_BOOKMARKS_STORE_IS_FS, TRUE
    );
}

static void process_volume_cb(
    GVolume *volume, BeditFileBrowserBookmarksStore *model
) {
    GMount *mount = g_volume_get_mount(volume);
    guint flags = BEDIT_FILE_BROWSER_BOOKMARKS_STORE_NONE;

    /* CHECK: should we use the LOCAL/REMOTE thing still? */
    if (mount) {
        /* Show mounted volume */
        add_fs(model, mount, flags, NULL);
        g_object_unref(mount);
    } else if (g_volume_can_mount(volume)) {
        /* We also show the unmounted volume here so users can
           mount it if they want to access it */
        add_fs(model, volume, flags, NULL);
    }
}

static void process_drive_novolumes(
    BeditFileBrowserBookmarksStore *model, GDrive *drive
) {
    if (
        g_drive_is_media_removable(drive) &&
        !g_drive_is_media_check_automatic(drive) &&
        g_drive_can_poll_for_media(drive)
    ) {
        /* This can be the case for floppy drives or other
           drives where media detection fails. We show the
           drive and poll for media when the user activates
           it */
        add_fs(model, drive, BEDIT_FILE_BROWSER_BOOKMARKS_STORE_NONE, NULL);
    }
}

static void process_drive_cb(
    GDrive *drive, BeditFileBrowserBookmarksStore *model
) {
    GList *volumes = g_drive_get_volumes(drive);

    if (volumes) {
        /* Add all volumes for the drive */
        g_list_foreach(volumes, (GFunc)process_volume_cb, model);
        g_list_free(volumes);
    } else {
        process_drive_novolumes(model, drive);
    }
}

static void init_drives(BeditFileBrowserBookmarksStore *model) {
    GList *drives = g_volume_monitor_get_connected_drives(
        model->volume_monitor
    );

    g_list_foreach(drives, (GFunc)process_drive_cb, model);
    g_list_free_full(drives, g_object_unref);
}

static void process_volume_nodrive_cb(
    GVolume *volume, BeditFileBrowserBookmarksStore *model
) {
    GDrive *drive = g_volume_get_drive(volume);

    if (drive) {
        g_object_unref(drive);
        return;
    }

    process_volume_cb(volume, model);
}

static void init_volumes(BeditFileBrowserBookmarksStore *model) {
    GList *volumes = g_volume_monitor_get_volumes(model->volume_monitor);

    g_list_foreach(volumes, (GFunc)process_volume_nodrive_cb, model);
    g_list_free_full(volumes, g_object_unref);
}

static void process_mount_novolume_cb(
    GMount *mount, BeditFileBrowserBookmarksStore *model
) {
    GVolume *volume = g_mount_get_volume(mount);

    if (volume) {
        g_object_unref(volume);
    } else if (!g_mount_is_shadowed(mount)) {
        /* Add the mount */
        add_fs(model, mount, BEDIT_FILE_BROWSER_BOOKMARKS_STORE_NONE, NULL);
    }
}

static void init_mounts(BeditFileBrowserBookmarksStore *model) {
    GList *mounts = g_volume_monitor_get_mounts(model->volume_monitor);

    g_list_foreach(mounts, (GFunc)process_mount_novolume_cb, model);
    g_list_free_full(mounts, g_object_unref);
}

static void init_fs(BeditFileBrowserBookmarksStore *model) {
    if (model->volume_monitor == NULL) {
        const gchar **ptr;
        const gchar *signals[] = {
            "drive-connected", "drive-changed",
            "drive-disconnected", "volume-added",
            "volume-removed", "volume-changed",
            "mount-added", "mount-removed",
            "mount-changed", NULL
        };

        model->volume_monitor = g_volume_monitor_get();

        /* Connect signals */
        for (ptr = signals; *ptr != NULL; ++ptr) {
            g_signal_connect(
                model->volume_monitor, *ptr,
                G_CALLBACK(on_fs_changed), model
            );
        }
    }

    /* First go through all the connected drives */
    init_drives(model);

    /* Then add all volumes, not associated with a drive */
    init_volumes(model);

    /* Then finally add all mounts that have no volume */
    init_mounts(model);
}

static gboolean add_bookmark(
    BeditFileBrowserBookmarksStore *model, gchar const *name, gchar const *uri
) {
    GFile *file = g_file_new_for_uri(uri);
    guint flags = BEDIT_FILE_BROWSER_BOOKMARKS_STORE_IS_BOOKMARK;
    GtkTreeIter iter;
    gboolean ret;

    if (g_file_is_native(file)) {
        flags |= BEDIT_FILE_BROWSER_BOOKMARKS_STORE_IS_LOCAL_BOOKMARK;
    } else {
        flags |= BEDIT_FILE_BROWSER_BOOKMARKS_STORE_IS_REMOTE_BOOKMARK;
    }

    ret = add_file(model, file, name, flags, &iter);

    g_object_unref(file);
    return ret;
}

static gchar *get_bookmarks_file(void) {
    return g_build_filename(
        g_get_user_config_dir(), "gtk-3.0", "bookmarks", NULL
    );
}

static gchar *get_legacy_bookmarks_file(void) {
    return g_build_filename(g_get_home_dir(), ".gtk-bookmarks", NULL);
}

static gboolean parse_bookmarks_file(
    BeditFileBrowserBookmarksStore *model,
    const gchar *bookmarks, gboolean *added
) {
    GError *error = NULL;
    gchar *contents;
    gchar **lines;
    gchar **line;

    if (!g_file_get_contents(bookmarks, &contents, NULL, &error)) {
        /* The bookmarks file doesn't exist (which is perfectly fine) */
        g_error_free(error);

        return FALSE;
    }

    lines = g_strsplit(contents, "\n", 0);

    for (line = lines; *line; ++line) {
        if (**line) {
            GFile *location;

            gchar *pos;
            gchar *name;

            /* CHECK: is this really utf8? */
            pos = g_utf8_strchr(*line, -1, ' ');

            if (pos != NULL) {
                *pos = '\0';
                name = pos + 1;
            } else {
                name = NULL;
            }

            /* the bookmarks file should contain valid
             * URIs, but paranoia is good */
            location = g_file_new_for_uri(*line);
            if (bedit_utils_is_valid_location(location)) {
                *added |= add_bookmark(model, name, *line);
            }
            g_object_unref(location);
        }
    }

    g_strfreev(lines);
    g_free(contents);

    /* Add a watch */
    if (model->bookmarks_monitor == NULL) {
        GFile *file = g_file_new_for_path(bookmarks);

        model->bookmarks_monitor = g_file_monitor_file(
            file, G_FILE_MONITOR_NONE, NULL, NULL
        );
        g_object_unref(file);

        g_signal_connect(
            model->bookmarks_monitor, "changed",
            G_CALLBACK(on_bookmarks_file_changed), model
        );
    }

    return TRUE;
}

static void init_bookmarks(BeditFileBrowserBookmarksStore *model) {
    gchar *bookmarks = get_bookmarks_file();
    gboolean added = FALSE;

    if (!parse_bookmarks_file(model, bookmarks, &added)) {
        g_free(bookmarks);

        /* try the old location (gtk <= 3.4) */
        bookmarks = get_legacy_bookmarks_file();
        parse_bookmarks_file(model, bookmarks, &added);
    }

    if (added) {
        /* Bookmarks separator */
        add_node(
            model, NULL, NULL, NULL, NULL,
            BEDIT_FILE_BROWSER_BOOKMARKS_STORE_IS_BOOKMARK |
            BEDIT_FILE_BROWSER_BOOKMARKS_STORE_IS_SEPARATOR,
            NULL
        );
    }

    g_free(bookmarks);
}

static gint flags_order[] = {
    BEDIT_FILE_BROWSER_BOOKMARKS_STORE_IS_HOME,
    BEDIT_FILE_BROWSER_BOOKMARKS_STORE_IS_DESKTOP,
    BEDIT_FILE_BROWSER_BOOKMARKS_STORE_IS_SPECIAL_DIR,
    BEDIT_FILE_BROWSER_BOOKMARKS_STORE_IS_ROOT,
    BEDIT_FILE_BROWSER_BOOKMARKS_STORE_IS_FS,
    BEDIT_FILE_BROWSER_BOOKMARKS_STORE_IS_BOOKMARK,
    -1
};

static gint utf8_casecmp(gchar const *s1, const gchar *s2) {
    gchar *n1;
    gchar *n2;
    gint result;

    n1 = g_utf8_casefold(s1, -1);
    n2 = g_utf8_casefold(s2, -1);

    result = g_utf8_collate(n1, n2);

    g_free(n1);
    g_free(n2);

    return result;
}

static gint bookmarks_compare_names(
    GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b
) {
    gchar *n1;
    gchar *n2;
    gint result;
    guint f1;
    guint f2;

    gtk_tree_model_get(
        model, a,
        BEDIT_FILE_BROWSER_BOOKMARKS_STORE_COLUMN_NAME, &n1,
        BEDIT_FILE_BROWSER_BOOKMARKS_STORE_COLUMN_FLAGS, &f1,
        -1
    );
    gtk_tree_model_get(
        model, b,
        BEDIT_FILE_BROWSER_BOOKMARKS_STORE_COLUMN_NAME, &n2,
        BEDIT_FILE_BROWSER_BOOKMARKS_STORE_COLUMN_FLAGS, &f2,
        -1
    );

    /* do not sort actual bookmarks to keep same order as in nautilus */
    if (
        (f1 & BEDIT_FILE_BROWSER_BOOKMARKS_STORE_IS_BOOKMARK) &&
        (f2 & BEDIT_FILE_BROWSER_BOOKMARKS_STORE_IS_BOOKMARK)
    ) {
        result = 0;
    } else if (n1 == NULL && n2 == NULL) {
        result = 0;
    } else if (n1 == NULL) {
        result = -1;
    } else if (n2 == NULL) {
        result = 1;
    } else {
        result = utf8_casecmp(n1, n2);
    }

    g_free(n1);
    g_free(n2);

    return result;
}

static gint bookmarks_compare_flags(
    GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b
) {
    guint sep = BEDIT_FILE_BROWSER_BOOKMARKS_STORE_IS_SEPARATOR;
    guint f1;
    guint f2;
    gint *flags;

    gtk_tree_model_get(
        model, a,
        BEDIT_FILE_BROWSER_BOOKMARKS_STORE_COLUMN_FLAGS, &f1,
        -1
    );
    gtk_tree_model_get(
        model, b,
        BEDIT_FILE_BROWSER_BOOKMARKS_STORE_COLUMN_FLAGS, &f2,
        -1
    );

    for (flags = flags_order; *flags != -1; ++flags) {
        if ((f1 & *flags) != (f2 & *flags)) {
            if (f1 & *flags) {
                return -1;
            } else {
                return 1;
            }
        } else if ((f1 & *flags) && (f1 & sep) != (f2 & sep)) {
            if (f1 & sep) {
                return -1;
            } else {
                return 1;
            }
        }
    }

    return 0;
}

static gint bookmarks_compare_func(
    GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer user_data
) {
    gint result = bookmarks_compare_flags(model, a, b);

    if (result == 0) {
        result = bookmarks_compare_names(model, a, b);
    }

    return result;
}

static gboolean find_with_flags(
    GtkTreeModel *model, GtkTreeIter *iter, gpointer obj,
    guint flags, guint notflags
) {
    GtkTreeIter child;
    guint childflags = 0;
    GObject *childobj;
    gboolean fequal;

    if (!gtk_tree_model_get_iter_first(model, &child)) {
        return FALSE;
    }

    do {
        gtk_tree_model_get(
            model, &child,
            BEDIT_FILE_BROWSER_BOOKMARKS_STORE_COLUMN_OBJECT, &childobj,
            BEDIT_FILE_BROWSER_BOOKMARKS_STORE_COLUMN_FLAGS, &childflags,
            -1
        );

        fequal = (obj == childobj);

        if (childobj) {
            g_object_unref(childobj);
        }

        if (
            (obj == NULL || fequal) &&
            (childflags & flags) == flags &&
            !(childflags & notflags)
        ) {
            *iter = child;
            return TRUE;
        }
    } while (gtk_tree_model_iter_next(model, &child));

    return FALSE;
}

static void remove_node(GtkTreeModel *model, GtkTreeIter *iter) {
    guint flags;

    gtk_tree_model_get(
        model, iter,
        BEDIT_FILE_BROWSER_BOOKMARKS_STORE_COLUMN_FLAGS, &flags,
        -1
    );

    if (
        !(flags & BEDIT_FILE_BROWSER_BOOKMARKS_STORE_IS_SEPARATOR) &&
        flags & BEDIT_FILE_BROWSER_BOOKMARKS_STORE_IS_FS
    ) {
        check_mount_separator(
            BEDIT_FILE_BROWSER_BOOKMARKS_STORE(model),
            flags & BEDIT_FILE_BROWSER_BOOKMARKS_STORE_IS_FS, FALSE
        );
    }

    gtk_tree_store_remove(GTK_TREE_STORE(model), iter);
}

static void remove_bookmarks(BeditFileBrowserBookmarksStore *model) {
    GtkTreeIter iter;

    while (find_with_flags(
        GTK_TREE_MODEL(model), &iter, NULL,
        BEDIT_FILE_BROWSER_BOOKMARKS_STORE_IS_BOOKMARK, 0
    )) {
        remove_node(GTK_TREE_MODEL(model), &iter);
    }
}

static void initialize_fill(BeditFileBrowserBookmarksStore *model) {
    init_special_directories(model);
    init_fs(model);
    init_bookmarks(model);
}

/* Public */
BeditFileBrowserBookmarksStore *bedit_file_browser_bookmarks_store_new(void) {
    BeditFileBrowserBookmarksStore *model;
    GType column_types[] = {
        GDK_TYPE_PIXBUF,
        G_TYPE_STRING,
        G_TYPE_STRING,
        G_TYPE_OBJECT,
        G_TYPE_UINT
    };

    model = g_object_new(BEDIT_TYPE_FILE_BROWSER_BOOKMARKS_STORE, NULL);
    gtk_tree_store_set_column_types(
        GTK_TREE_STORE(model),
        BEDIT_FILE_BROWSER_BOOKMARKS_STORE_N_COLUMNS,
        column_types
    );

    gtk_tree_sortable_set_default_sort_func(
        GTK_TREE_SORTABLE(model), bookmarks_compare_func, NULL, NULL
    );
    gtk_tree_sortable_set_sort_column_id(
        GTK_TREE_SORTABLE(model),
        GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
        GTK_SORT_ASCENDING
    );

    initialize_fill(model);

    return model;
}

GFile *bedit_file_browser_bookmarks_store_get_location(
    BeditFileBrowserBookmarksStore *model, GtkTreeIter *iter
) {
    GObject *obj;
    GFile *file = NULL;
    guint flags;
    GFile *ret = NULL;
    gboolean isfs;

    g_return_val_if_fail(BEDIT_IS_FILE_BROWSER_BOOKMARKS_STORE(model), NULL);
    g_return_val_if_fail(iter != NULL, NULL);

    gtk_tree_model_get(
        GTK_TREE_MODEL(model), iter,
        BEDIT_FILE_BROWSER_BOOKMARKS_STORE_COLUMN_FLAGS, &flags,
        BEDIT_FILE_BROWSER_BOOKMARKS_STORE_COLUMN_OBJECT, &obj,
        -1
    );

    if (obj == NULL) {
        return NULL;
    }

    isfs = (flags & BEDIT_FILE_BROWSER_BOOKMARKS_STORE_IS_FS);

    if (isfs && (flags & BEDIT_FILE_BROWSER_BOOKMARKS_STORE_IS_MOUNT)) {
        file = g_mount_get_root(G_MOUNT(obj));
    } else if (!isfs) {
        file = (GFile *)g_object_ref(obj);
    }

    g_object_unref(obj);

    if (file) {
        ret = g_file_dup(file);
        g_object_unref(file);
    }

    return ret;
}

void bedit_file_browser_bookmarks_store_refresh(
    BeditFileBrowserBookmarksStore *model
) {
    gtk_tree_store_clear(GTK_TREE_STORE(model));
    initialize_fill(model);
}

static void on_fs_changed(
    GVolumeMonitor *monitor, GObject *object,
    BeditFileBrowserBookmarksStore *model
) {
    GtkTreeModel *tree_model = GTK_TREE_MODEL(model);
    guint flags = BEDIT_FILE_BROWSER_BOOKMARKS_STORE_IS_FS;
    guint noflags = BEDIT_FILE_BROWSER_BOOKMARKS_STORE_IS_SEPARATOR;
    GtkTreeIter iter;

    /* clear all fs items */
    while (find_with_flags(tree_model, &iter, NULL, flags, noflags)) {
        remove_node(tree_model, &iter);
    }

    /* then reinitialize */
    init_fs(model);
}

static void on_bookmarks_file_changed(
    GFileMonitor *monitor, GFile *file, GFile *other_file,
    GFileMonitorEvent event_type, BeditFileBrowserBookmarksStore *model
) {
    switch (event_type) {
    case G_FILE_MONITOR_EVENT_CHANGED:
    case G_FILE_MONITOR_EVENT_CREATED:
        /* Re-initialize bookmarks */
        remove_bookmarks(model);
        init_bookmarks(model);
        break;
    /* FIXME: shouldn't we also monitor the directory? */
    case G_FILE_MONITOR_EVENT_DELETED:
        /* Remove bookmarks */
        remove_bookmarks(model);
        g_object_unref(monitor);
        model->bookmarks_monitor = NULL;
        break;
    default:
        break;
    }
}

