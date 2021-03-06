/*
 * bedit-file-browser-widget.h
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * Based on gedit-file-browser-widget.h from Gedit.
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

#ifndef BEDIT_FILE_BROWSER_WIDGET_H
#define BEDIT_FILE_BROWSER_WIDGET_H

#include <gtk/gtk.h>

#include "bedit-file-browser-bookmarks-store.h"
#include "bedit-file-browser-folder-view.h"
#include "bedit-file-browser-store.h"
#include "bedit-menu-extension.h"

G_BEGIN_DECLS

#define BEDIT_TYPE_FILE_BROWSER_WIDGET                                      \
    (bedit_file_browser_widget_get_type())

G_DECLARE_FINAL_TYPE(
    BeditFileBrowserWidget, bedit_file_browser_widget,
    BEDIT, FILE_BROWSER_WIDGET,
    GtkGrid
)

typedef gboolean (*BeditFileBrowserWidgetFilterFunc)(
    BeditFileBrowserWidget *obj, BeditFileBrowserStore *model,
    GtkTreeIter *iter, gpointer user_data
);

GtkWidget *bedit_file_browser_widget_new(void);

void bedit_file_browser_widget_show_bookmarks(BeditFileBrowserWidget *obj);
void bedit_file_browser_widget_show_files(BeditFileBrowserWidget *obj);

void bedit_file_browser_widget_set_root(
    BeditFileBrowserWidget *obj, GFile *root
);
void bedit_file_browser_widget_set_virtual_root(
    BeditFileBrowserWidget *obj, GFile *virtual_root
);
void bedit_file_browser_widget_set_root_and_virtual_root(
    BeditFileBrowserWidget *obj, GFile *root, GFile *virtual_root
);
GFile *bedit_file_browser_widget_get_virtual_root(BeditFileBrowserWidget *obj);

gboolean bedit_file_browser_widget_get_selected_directory(
    BeditFileBrowserWidget *obj, GtkTreeIter *iter
);

void bedit_file_browser_widget_set_active_root_enabled(
    BeditFileBrowserWidget *widget, gboolean enabled
);

BeditFileBrowserStore *bedit_file_browser_widget_get_browser_store(
    BeditFileBrowserWidget *obj
);
BeditFileBrowserBookmarksStore *bedit_file_browser_widget_get_bookmarks_store(
    BeditFileBrowserWidget *obj
);
BeditFileBrowserFolderView *bedit_file_browser_widget_get_browser_view(
    BeditFileBrowserWidget *obj
);

void bedit_file_browser_widget_set_filter_enabled(
    BeditFileBrowserWidget *obj, gboolean enabled
);
gboolean bedit_file_browser_widget_get_filter_enabled(
    BeditFileBrowserWidget *obj
);
void bedit_file_browser_widget_set_show_hidden(
    BeditFileBrowserWidget *obj, gboolean show_hidden
);
gboolean bedit_file_browser_widget_get_show_hidden(
    BeditFileBrowserWidget *obj
);
void bedit_file_browser_widget_set_show_binary(
    BeditFileBrowserWidget *obj, gboolean show_binary
);
gboolean bedit_file_browser_widget_get_show_binary(
    BeditFileBrowserWidget *obj
);

BeditMenuExtension *bedit_file_browser_widget_extend_context_menu(
    BeditFileBrowserWidget *obj
);
void bedit_file_browser_widget_refresh(BeditFileBrowserWidget *obj);

void _bedit_file_browser_widget_register_type(GTypeModule *type_module);

G_END_DECLS

#endif /* BEDIT_FILE_BROWSER_WIDGET_H */

