/*
 * bedit-file-browser-search-view.h
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

#ifndef BEDIT_FILE_BROWSER_SEARCH_VIEW_H
#define BEDIT_FILE_BROWSER_SEARCH_VIEW_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define BEDIT_TYPE_FILE_BROWSER_SEARCH_VIEW                                 \
    (bedit_file_browser_search_view_get_type())

G_DECLARE_FINAL_TYPE(
    BeditFileBrowserSearchView, bedit_file_browser_search_view,
    BEDIT, FILE_BROWSER_SEARCH_VIEW,
    GtkBin
)

GType bedit_file_browser_search_view_get_type(void) G_GNUC_CONST;

BeditFileBrowserSearchView *bedit_file_browser_search_view_new(void);

void bedit_file_browser_search_view_set_virtual_root(
    BeditFileBrowserSearchView *view, GFile *root
);
GFile *bedit_file_browser_search_view_get_virtual_root(
    BeditFileBrowserSearchView *view
);

void bedit_file_browser_search_view_set_query(
    BeditFileBrowserSearchView *view, const gchar *query
);
gchar *bedit_file_browser_search_view_get_query(
    BeditFileBrowserSearchView *view
);

void bedit_file_browser_search_view_set_enabled(
    BeditFileBrowserSearchView *view, gboolean enabled
);
gboolean bedit_file_browser_search_view_get_enabled(
    BeditFileBrowserSearchView *view
);

void _bedit_file_browser_search_view_register_type(
    GTypeModule *type_module
);

G_END_DECLS

#endif /* BEDIT_FILE_BROWSER_SEARCH_VIEW_H */

