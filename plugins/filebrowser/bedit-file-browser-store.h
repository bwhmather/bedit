/*
 * bedit-file-browser-store.h - Bedit plugin providing easy file access
 * from the sidepanel
 *
 * Copyright (C) 2006 - Jesse van den Kieboom <jesse@icecrew.nl>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GEDIT_FILE_BROWSER_STORE_H
#define GEDIT_FILE_BROWSER_STORE_H

#include <gtk/gtk.h>

G_BEGIN_DECLS
#define GEDIT_TYPE_FILE_BROWSER_STORE (bedit_file_browser_store_get_type())
#define GEDIT_FILE_BROWSER_STORE(obj)                                          \
    (G_TYPE_CHECK_INSTANCE_CAST(                                               \
        (obj), GEDIT_TYPE_FILE_BROWSER_STORE, BeditFileBrowserStore))
#define GEDIT_FILE_BROWSER_STORE_CONST(obj)                                    \
    (G_TYPE_CHECK_INSTANCE_CAST(                                               \
        (obj), GEDIT_TYPE_FILE_BROWSER_STORE, BeditFileBrowserStore const))
#define GEDIT_FILE_BROWSER_STORE_CLASS(klass)                                  \
    (G_TYPE_CHECK_CLASS_CAST(                                                  \
        (klass), GEDIT_TYPE_FILE_BROWSER_STORE, BeditFileBrowserStoreClass))
#define GEDIT_IS_FILE_BROWSER_STORE(obj)                                       \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GEDIT_TYPE_FILE_BROWSER_STORE))
#define GEDIT_IS_FILE_BROWSER_STORE_CLASS(klass)                               \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GEDIT_TYPE_FILE_BROWSER_STORE))
#define GEDIT_FILE_BROWSER_STORE_GET_CLASS(obj)                                \
    (G_TYPE_INSTANCE_GET_CLASS(                                                \
        (obj), GEDIT_TYPE_FILE_BROWSER_STORE, BeditFileBrowserStoreClass))

typedef enum {
    GEDIT_FILE_BROWSER_STORE_COLUMN_ICON = 0,
    GEDIT_FILE_BROWSER_STORE_COLUMN_ICON_NAME,
    GEDIT_FILE_BROWSER_STORE_COLUMN_MARKUP,
    GEDIT_FILE_BROWSER_STORE_COLUMN_LOCATION,
    GEDIT_FILE_BROWSER_STORE_COLUMN_FLAGS,

    /* Columns not in common with BeditFileBookmarksStore */
    GEDIT_FILE_BROWSER_STORE_COLUMN_NAME,
    GEDIT_FILE_BROWSER_STORE_COLUMN_EMBLEM,
    GEDIT_FILE_BROWSER_STORE_COLUMN_NUM
} BeditFileBrowserStoreColumn;

typedef enum {
    GEDIT_FILE_BROWSER_STORE_FLAG_IS_DIRECTORY = 1 << 0,
    GEDIT_FILE_BROWSER_STORE_FLAG_IS_HIDDEN = 1 << 1,
    GEDIT_FILE_BROWSER_STORE_FLAG_IS_TEXT = 1 << 2,
    GEDIT_FILE_BROWSER_STORE_FLAG_LOADED = 1 << 3,
    GEDIT_FILE_BROWSER_STORE_FLAG_IS_FILTERED = 1 << 4,
    GEDIT_FILE_BROWSER_STORE_FLAG_IS_DUMMY = 1 << 5
} BeditFileBrowserStoreFlag;

typedef enum {
    GEDIT_FILE_BROWSER_STORE_RESULT_OK,
    GEDIT_FILE_BROWSER_STORE_RESULT_NO_CHANGE,
    GEDIT_FILE_BROWSER_STORE_RESULT_ERROR,
    GEDIT_FILE_BROWSER_STORE_RESULT_NO_TRASH,
    GEDIT_FILE_BROWSER_STORE_RESULT_MOUNTING,
    GEDIT_FILE_BROWSER_STORE_RESULT_NUM
} BeditFileBrowserStoreResult;

typedef enum {
    GEDIT_FILE_BROWSER_STORE_FILTER_MODE_NONE = 0,
    GEDIT_FILE_BROWSER_STORE_FILTER_MODE_HIDE_HIDDEN = 1 << 0,
    GEDIT_FILE_BROWSER_STORE_FILTER_MODE_HIDE_BINARY = 1 << 1
} BeditFileBrowserStoreFilterMode;

#define FILE_IS_DIR(flags) (flags & GEDIT_FILE_BROWSER_STORE_FLAG_IS_DIRECTORY)
#define FILE_IS_HIDDEN(flags) (flags & GEDIT_FILE_BROWSER_STORE_FLAG_IS_HIDDEN)
#define FILE_IS_TEXT(flags) (flags & GEDIT_FILE_BROWSER_STORE_FLAG_IS_TEXT)
#define FILE_LOADED(flags) (flags & GEDIT_FILE_BROWSER_STORE_FLAG_LOADED)
#define FILE_IS_FILTERED(flags)                                                \
    (flags & GEDIT_FILE_BROWSER_STORE_FLAG_IS_FILTERED)
#define FILE_IS_DUMMY(flags) (flags & GEDIT_FILE_BROWSER_STORE_FLAG_IS_DUMMY)

typedef struct _BeditFileBrowserStore BeditFileBrowserStore;
typedef struct _BeditFileBrowserStoreClass BeditFileBrowserStoreClass;
typedef struct _BeditFileBrowserStorePrivate BeditFileBrowserStorePrivate;

typedef gboolean (*BeditFileBrowserStoreFilterFunc)(
    BeditFileBrowserStore *model, GtkTreeIter *iter, gpointer user_data);

struct _BeditFileBrowserStore {
    GObject parent;

    BeditFileBrowserStorePrivate *priv;
};

struct _BeditFileBrowserStoreClass {
    GObjectClass parent_class;

    /* Signals */
    void (*begin_loading)(BeditFileBrowserStore *model, GtkTreeIter *iter);
    void (*end_loading)(BeditFileBrowserStore *model, GtkTreeIter *iter);
    void (*error)(BeditFileBrowserStore *model, guint code, gchar *message);
    gboolean (*no_trash)(BeditFileBrowserStore *model, GList *files);
    void (*rename)(
        BeditFileBrowserStore *model, GFile *oldfile, GFile *newfile);
    void (*begin_refresh)(BeditFileBrowserStore *model);
    void (*end_refresh)(BeditFileBrowserStore *model);
    void (*unload)(BeditFileBrowserStore *model, GFile *location);
    void (*before_row_deleted)(BeditFileBrowserStore *model, GtkTreePath *path);
};

GType bedit_file_browser_store_get_type(void) G_GNUC_CONST;

BeditFileBrowserStore *bedit_file_browser_store_new(GFile *root);
BeditFileBrowserStoreResult bedit_file_browser_store_set_root_and_virtual_root(
    BeditFileBrowserStore *model, GFile *root, GFile *virtual_root);
BeditFileBrowserStoreResult bedit_file_browser_store_set_root(
    BeditFileBrowserStore *model, GFile *root);
BeditFileBrowserStoreResult bedit_file_browser_store_set_virtual_root(
    BeditFileBrowserStore *model, GtkTreeIter *iter);
BeditFileBrowserStoreResult
bedit_file_browser_store_set_virtual_root_from_location(
    BeditFileBrowserStore *model, GFile *root);
BeditFileBrowserStoreResult bedit_file_browser_store_set_virtual_root_up(
    BeditFileBrowserStore *model);
BeditFileBrowserStoreResult bedit_file_browser_store_set_virtual_root_top(
    BeditFileBrowserStore *model);
gboolean bedit_file_browser_store_get_iter_virtual_root(
    BeditFileBrowserStore *model, GtkTreeIter *iter);
gboolean bedit_file_browser_store_get_iter_root(
    BeditFileBrowserStore *model, GtkTreeIter *iter);
GFile *bedit_file_browser_store_get_root(BeditFileBrowserStore *model);
GFile *bedit_file_browser_store_get_virtual_root(BeditFileBrowserStore *model);
gboolean bedit_file_browser_store_iter_equal(
    BeditFileBrowserStore *model, GtkTreeIter *iter1, GtkTreeIter *iter2);
void bedit_file_browser_store_set_value(
    BeditFileBrowserStore *tree_model, GtkTreeIter *iter, gint column,
    GValue *value);
void _bedit_file_browser_store_iter_expanded(
    BeditFileBrowserStore *model, GtkTreeIter *iter);
void _bedit_file_browser_store_iter_collapsed(
    BeditFileBrowserStore *model, GtkTreeIter *iter);
BeditFileBrowserStoreFilterMode bedit_file_browser_store_get_filter_mode(
    BeditFileBrowserStore *model);
void bedit_file_browser_store_set_filter_mode(
    BeditFileBrowserStore *model, BeditFileBrowserStoreFilterMode mode);
void bedit_file_browser_store_set_filter_func(
    BeditFileBrowserStore *model, BeditFileBrowserStoreFilterFunc func,
    gpointer user_data);
const gchar *const *bedit_file_browser_store_get_binary_patterns(
    BeditFileBrowserStore *model);
void bedit_file_browser_store_set_binary_patterns(
    BeditFileBrowserStore *model, const gchar **binary_patterns);
void bedit_file_browser_store_refilter(BeditFileBrowserStore *model);
BeditFileBrowserStoreFilterMode
bedit_file_browser_store_filter_mode_get_default(void);
void bedit_file_browser_store_refresh(BeditFileBrowserStore *model);
gboolean bedit_file_browser_store_rename(
    BeditFileBrowserStore *model, GtkTreeIter *iter, gchar const *new_name,
    GError **error);
BeditFileBrowserStoreResult bedit_file_browser_store_delete(
    BeditFileBrowserStore *model, GtkTreeIter *iter, gboolean trash);
BeditFileBrowserStoreResult bedit_file_browser_store_delete_all(
    BeditFileBrowserStore *model, GList *rows, gboolean trash);
gboolean bedit_file_browser_store_new_file(
    BeditFileBrowserStore *model, GtkTreeIter *parent, GtkTreeIter *iter);
gboolean bedit_file_browser_store_new_directory(
    BeditFileBrowserStore *model, GtkTreeIter *parent, GtkTreeIter *iter);
void bedit_file_browser_store_cancel_mount_operation(
    BeditFileBrowserStore *store);

void _bedit_file_browser_store_register_type(GTypeModule *type_module);

G_END_DECLS

#endif /* GEDIT_FILE_BROWSER_STORE_H */
/* ex:set ts=8 noet: */
