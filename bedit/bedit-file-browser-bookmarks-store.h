/*
 * bedit-file-browser-bookmarks-store.h
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * Based on gedit-file-bookmarks-store.h from Gedit.
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

#ifndef BEDIT_FILE_BROWSER_BOOKMARKS_STORE_H
#define BEDIT_FILE_BROWSER_BOOKMARKS_STORE_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define BEDIT_TYPE_FILE_BROWSER_BOOKMARKS_STORE                             \
    (bedit_file_browser_bookmarks_store_get_type())

G_DECLARE_FINAL_TYPE(
    BeditFileBrowserBookmarksStore, bedit_file_browser_bookmarks_store,
    BEDIT, FILE_BROWSER_BOOKMARKS_STORE,
    GtkTreeStore
)

enum {
    BEDIT_FILE_BROWSER_BOOKMARKS_STORE_COLUMN_ICON = 0,
    BEDIT_FILE_BROWSER_BOOKMARKS_STORE_COLUMN_ICON_NAME,
    BEDIT_FILE_BROWSER_BOOKMARKS_STORE_COLUMN_NAME,
    BEDIT_FILE_BROWSER_BOOKMARKS_STORE_COLUMN_OBJECT,
    BEDIT_FILE_BROWSER_BOOKMARKS_STORE_COLUMN_FLAGS,
    BEDIT_FILE_BROWSER_BOOKMARKS_STORE_N_COLUMNS
};

enum {
    BEDIT_FILE_BROWSER_BOOKMARKS_STORE_NONE = 0,
    /* Separator item */
    BEDIT_FILE_BROWSER_BOOKMARKS_STORE_IS_SEPARATOR = 1 << 0,
    /* Special user dir */
    BEDIT_FILE_BROWSER_BOOKMARKS_STORE_IS_SPECIAL_DIR = 1 << 1,
    /* The special Home user directory */
    BEDIT_FILE_BROWSER_BOOKMARKS_STORE_IS_HOME = 1 << 2,
    /* The special Desktop user directory */
    BEDIT_FILE_BROWSER_BOOKMARKS_STORE_IS_DESKTOP = 1 << 3,
    /* The special Documents user directory */
    BEDIT_FILE_BROWSER_BOOKMARKS_STORE_IS_DOCUMENTS = 1 << 4,
    /* A mount object */
    BEDIT_FILE_BROWSER_BOOKMARKS_STORE_IS_FS = 1 << 5,
    /* A mount object */
    BEDIT_FILE_BROWSER_BOOKMARKS_STORE_IS_MOUNT = 1 << 6,
    /* A volume object */
    BEDIT_FILE_BROWSER_BOOKMARKS_STORE_IS_VOLUME = 1 << 7,
    /* A drive object */
    BEDIT_FILE_BROWSER_BOOKMARKS_STORE_IS_DRIVE = 1 << 8,
    /* The root file system (file:///) */
    BEDIT_FILE_BROWSER_BOOKMARKS_STORE_IS_ROOT = 1 << 9,
    /* A gtk bookmark */
    BEDIT_FILE_BROWSER_BOOKMARKS_STORE_IS_BOOKMARK = 1 << 10,
    /* A remote gtk bookmark */
    BEDIT_FILE_BROWSER_BOOKMARKS_STORE_IS_REMOTE_BOOKMARK = 1 << 11,
    /* A local gtk bookmark */
    BEDIT_FILE_BROWSER_BOOKMARKS_STORE_IS_LOCAL_BOOKMARK = 1 << 12
};

BeditFileBrowserBookmarksStore *bedit_file_browser_bookmarks_store_new(void);
GFile *bedit_file_browser_bookmarks_store_get_location(
    BeditFileBrowserBookmarksStore *model, GtkTreeIter *iter
);
void bedit_file_browser_bookmarks_store_refresh(
    BeditFileBrowserBookmarksStore *model
);

G_END_DECLS

#endif /* BEDIT_FILE_BROWSER_BOOKMARKS_STORE_H */

