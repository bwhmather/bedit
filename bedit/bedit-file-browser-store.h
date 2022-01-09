/*
 * bedit-file-browser-store.h
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * Based on gedit-file-browser-store.h from Gedit.
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

#ifndef BEDIT_FILE_BROWSER_STORE_H
#define BEDIT_FILE_BROWSER_STORE_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define BEDIT_TYPE_FILE_BROWSER_STORE (bedit_file_browser_store_get_type())

G_DECLARE_FINAL_TYPE(
    BeditFileBrowserStore,
    bedit_file_browser_store,
    BEDIT, FILE_BROWSER_STORE,
    GObject
)

typedef enum {
    BEDIT_FILE_BROWSER_STORE_COLUMN_ICON = 0,
    BEDIT_FILE_BROWSER_STORE_COLUMN_MARKUP,
    BEDIT_FILE_BROWSER_STORE_COLUMN_LOCATION,
    BEDIT_FILE_BROWSER_STORE_COLUMN_FLAGS,

    /* Columns not in common with BeditFileBookmarksStore */
    BEDIT_FILE_BROWSER_STORE_COLUMN_NAME,
    BEDIT_FILE_BROWSER_STORE_COLUMN_EMBLEM,
    BEDIT_FILE_BROWSER_STORE_COLUMN_NUM
} BeditFileBrowserStoreColumn;

typedef enum {
    BEDIT_FILE_BROWSER_STORE_FLAG_IS_DIRECTORY = 1 << 0,
    BEDIT_FILE_BROWSER_STORE_FLAG_IS_HIDDEN = 1 << 1,
    BEDIT_FILE_BROWSER_STORE_FLAG_IS_TEXT = 1 << 2,
    BEDIT_FILE_BROWSER_STORE_FLAG_LOADED = 1 << 3,
    BEDIT_FILE_BROWSER_STORE_FLAG_IS_FILTERED = 1 << 4,
    BEDIT_FILE_BROWSER_STORE_FLAG_IS_DUMMY = 1 << 5
} BeditFileBrowserStoreFlag;

#define FILE_IS_DIR(flags) (flags & BEDIT_FILE_BROWSER_STORE_FLAG_IS_DIRECTORY)
#define FILE_IS_HIDDEN(flags) (flags & BEDIT_FILE_BROWSER_STORE_FLAG_IS_HIDDEN)
#define FILE_IS_TEXT(flags) (flags & BEDIT_FILE_BROWSER_STORE_FLAG_IS_TEXT)
#define FILE_LOADED(flags) (flags & BEDIT_FILE_BROWSER_STORE_FLAG_LOADED)
#define FILE_IS_FILTERED(flags)                                             \
    (flags & BEDIT_FILE_BROWSER_STORE_FLAG_IS_FILTERED)
#define FILE_IS_DUMMY(flags) (flags & BEDIT_FILE_BROWSER_STORE_FLAG_IS_DUMMY)

BeditFileBrowserStore *bedit_file_browser_store_new(GFile *root);
void bedit_file_browser_store_set_root_and_virtual_root(
    BeditFileBrowserStore *model, GFile *root, GFile *virtual_root
);
void bedit_file_browser_store_set_root(
    BeditFileBrowserStore *model, GFile *root
);
void bedit_file_browser_store_set_virtual_root(
    BeditFileBrowserStore *model, GtkTreeIter *iter
);
void
bedit_file_browser_store_set_virtual_root_from_location(
    BeditFileBrowserStore *model, GFile *root
);
void bedit_file_browser_store_set_virtual_root_up(
    BeditFileBrowserStore *model
);
void bedit_file_browser_store_set_virtual_root_top(
    BeditFileBrowserStore *model
);
gboolean bedit_file_browser_store_get_iter_virtual_root(
    BeditFileBrowserStore *model, GtkTreeIter *iter
);
gboolean bedit_file_browser_store_get_iter_root(
    BeditFileBrowserStore *model, GtkTreeIter *iter
);
GFile *bedit_file_browser_store_get_root(BeditFileBrowserStore *model);
GFile *bedit_file_browser_store_get_virtual_root(BeditFileBrowserStore *model);
gboolean bedit_file_browser_store_iter_equal(
    BeditFileBrowserStore *model, GtkTreeIter *iter1, GtkTreeIter *iter2
);
void bedit_file_browser_store_set_value(
    BeditFileBrowserStore *tree_model, GtkTreeIter *iter, gint column,
    GValue *value
);
void _bedit_file_browser_store_iter_expanded(
    BeditFileBrowserStore *model, GtkTreeIter *iter
);
void _bedit_file_browser_store_iter_collapsed(
    BeditFileBrowserStore *model, GtkTreeIter *iter
);
gboolean bedit_file_browser_store_get_show_hidden(
    BeditFileBrowserStore *model
);
void bedit_file_browser_store_set_show_hidden(
    BeditFileBrowserStore *model, gboolean show_hidden
);
gboolean bedit_file_browser_store_get_show_binary(
    BeditFileBrowserStore *model
);
void bedit_file_browser_store_set_show_binary(
    BeditFileBrowserStore *model, gboolean show_binary
);
const gchar *const *bedit_file_browser_store_get_binary_patterns(
    BeditFileBrowserStore *model
);
void bedit_file_browser_store_set_binary_patterns(
    BeditFileBrowserStore *model, const gchar **binary_patterns
);
void bedit_file_browser_store_refilter(BeditFileBrowserStore *model);
void bedit_file_browser_store_refresh(BeditFileBrowserStore *model);
gboolean bedit_file_browser_store_rename(
    BeditFileBrowserStore *model, GtkTreeIter *iter, gchar const *new_name,
    GError **error
);
void bedit_file_browser_store_delete(
    BeditFileBrowserStore *model, GtkTreeIter *iter, gboolean trash
);
void bedit_file_browser_store_delete_all(
    BeditFileBrowserStore *model, GList *rows, gboolean trash
);
gboolean bedit_file_browser_store_new_file(
    BeditFileBrowserStore *model, GtkTreeIter *parent, GtkTreeIter *iter
);
gboolean bedit_file_browser_store_new_directory(
    BeditFileBrowserStore *model, GtkTreeIter *parent, GtkTreeIter *iter
);
void bedit_file_browser_store_cancel_mount_operation(
    BeditFileBrowserStore *store
);

G_END_DECLS

#endif /* BEDIT_FILE_BROWSER_STORE_H */

