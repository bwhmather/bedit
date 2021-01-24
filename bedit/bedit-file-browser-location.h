/*
 * bedit-file-browser-location.h
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

#ifndef BEDIT_FILE_BROWSER_LOCATION_H
#define BEDIT_FILE_BROWSER_LOCATION_H

#include <gtk/gtk.h>
#include "bedit-file-browser-bookmarks-store.h"
#include "bedit-file-browser-store.h"

G_BEGIN_DECLS

#define BEDIT_TYPE_FILE_BROWSER_LOCATION (bedit_file_browser_location_get_type())

G_DECLARE_FINAL_TYPE(
    BeditFileBrowserLocation, bedit_file_browser_location,
    BEDIT, FILE_BROWSER_LOCATION,
    GtkMenuButton
)

GType bedit_file_browser_location_get_type(void) G_GNUC_CONST;

GtkWidget *bedit_file_browser_location_new(void);

void bedit_file_browser_location_set_bookmarks_store(
    BeditFileBrowserLocation *widget, BeditFileBrowserBookmarksStore *store
);
BeditFileBrowserBookmarksStore *bedit_file_browser_location_get_bookmarks_store(
    BeditFileBrowserLocation *widget
);
void bedit_file_browser_location_set_file_store(
    BeditFileBrowserLocation *widget, BeditFileBrowserStore *store
);
BeditFileBrowserStore *bedit_file_browser_location_get_file_store(
    BeditFileBrowserLocation *widget
);

G_END_DECLS

#endif /* BEDIT_FILE_BROWSER_LOCATION_H */

