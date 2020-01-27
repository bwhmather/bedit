/*
 * bedit-file-bookmarks-store.h - Bedit plugin providing easy file access
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

#ifndef BEDIT_FILE_BOOKMARKS_STORE_H
#define BEDIT_FILE_BOOKMARKS_STORE_H

#include <gtk/gtk.h>

G_BEGIN_DECLS
#define BEDIT_TYPE_FILE_BOOKMARKS_STORE (bedit_file_bookmarks_store_get_type())
#define BEDIT_FILE_BOOKMARKS_STORE(obj)                                        \
    (G_TYPE_CHECK_INSTANCE_CAST(                                               \
        (obj), BEDIT_TYPE_FILE_BOOKMARKS_STORE, BeditFileBookmarksStore))
#define BEDIT_FILE_BOOKMARKS_STORE_CONST(obj)                                  \
    (G_TYPE_CHECK_INSTANCE_CAST(                                               \
        (obj), BEDIT_TYPE_FILE_BOOKMARKS_STORE,                                \
        BeditFileBookmarksStore const))
#define BEDIT_FILE_BOOKMARKS_STORE_CLASS(klass)                                \
    (G_TYPE_CHECK_CLASS_CAST(                                                  \
        (klass), BEDIT_TYPE_FILE_BOOKMARKS_STORE,                              \
        BeditFileBookmarksStoreClass))
#define BEDIT_IS_FILE_BOOKMARKS_STORE(obj)                                     \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), BEDIT_TYPE_FILE_BOOKMARKS_STORE))
#define BEDIT_IS_FILE_BOOKMARKS_STORE_CLASS(klass)                             \
    (G_TYPE_CHECK_CLASS_TYPE((klass), BEDIT_TYPE_FILE_BOOKMARKS_STORE))
#define BEDIT_FILE_BOOKMARKS_STORE_GET_CLASS(obj)                              \
    (G_TYPE_INSTANCE_GET_CLASS(                                                \
        (obj), BEDIT_TYPE_FILE_BOOKMARKS_STORE, BeditFileBookmarksStoreClass))

typedef struct _BeditFileBookmarksStore BeditFileBookmarksStore;
typedef struct _BeditFileBookmarksStoreClass BeditFileBookmarksStoreClass;
typedef struct _BeditFileBookmarksStorePrivate BeditFileBookmarksStorePrivate;

enum {
    BEDIT_FILE_BOOKMARKS_STORE_COLUMN_ICON = 0,
    BEDIT_FILE_BOOKMARKS_STORE_COLUMN_ICON_NAME,
    BEDIT_FILE_BOOKMARKS_STORE_COLUMN_NAME,
    BEDIT_FILE_BOOKMARKS_STORE_COLUMN_OBJECT,
    BEDIT_FILE_BOOKMARKS_STORE_COLUMN_FLAGS,
    BEDIT_FILE_BOOKMARKS_STORE_N_COLUMNS
};

enum {
    BEDIT_FILE_BOOKMARKS_STORE_NONE = 0,
    BEDIT_FILE_BOOKMARKS_STORE_IS_SEPARATOR = 1 << 0, /* Separator item */
    BEDIT_FILE_BOOKMARKS_STORE_IS_SPECIAL_DIR = 1 << 1, /* Special user dir */
    BEDIT_FILE_BOOKMARKS_STORE_IS_HOME = 1
        << 2, /* The special Home user directory */
    BEDIT_FILE_BOOKMARKS_STORE_IS_DESKTOP = 1
        << 3, /* The special Desktop user directory */
    BEDIT_FILE_BOOKMARKS_STORE_IS_DOCUMENTS = 1
        << 4, /* The special Documents user directory */
    BEDIT_FILE_BOOKMARKS_STORE_IS_FS = 1 << 5, /* A mount object */
    BEDIT_FILE_BOOKMARKS_STORE_IS_MOUNT = 1 << 6, /* A mount object */
    BEDIT_FILE_BOOKMARKS_STORE_IS_VOLUME = 1 << 7, /* A volume object */
    BEDIT_FILE_BOOKMARKS_STORE_IS_DRIVE = 1 << 8, /* A drive object */
    BEDIT_FILE_BOOKMARKS_STORE_IS_ROOT = 1
        << 9, /* The root file system (file:///) */
    BEDIT_FILE_BOOKMARKS_STORE_IS_BOOKMARK = 1 << 10, /* A gtk bookmark */
    BEDIT_FILE_BOOKMARKS_STORE_IS_REMOTE_BOOKMARK = 1
        << 11, /* A remote gtk bookmark */
    BEDIT_FILE_BOOKMARKS_STORE_IS_LOCAL_BOOKMARK = 1
        << 12 /* A local gtk bookmark */
};

struct _BeditFileBookmarksStore {
    GtkTreeStore parent;

    BeditFileBookmarksStorePrivate *priv;
};

struct _BeditFileBookmarksStoreClass {
    GtkTreeStoreClass parent_class;
};

GType bedit_file_bookmarks_store_get_type(void) G_GNUC_CONST;

BeditFileBookmarksStore *bedit_file_bookmarks_store_new(void);
GFile *bedit_file_bookmarks_store_get_location(
    BeditFileBookmarksStore *model, GtkTreeIter *iter);
void bedit_file_bookmarks_store_refresh(BeditFileBookmarksStore *model);

void _bedit_file_bookmarks_store_register_type(GTypeModule *type_module);

G_END_DECLS

#endif /* BEDIT_FILE_BOOKMARKS_STORE_H */

