/*
 * bedit-utils.h
 * This file is part of bedit
 *
 * Copyright (C) 1998, 1999 Alex Roberts, Evan Lawrence
 * Copyright (C) 2000, 2001 Chema Celorio, Paolo Maggi
 * Copyright (C) 2002 - 2005 Paolo Maggi
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

#ifndef GEDIT_UTILS_H
#define GEDIT_UTILS_H

#include <gtksourceview/gtksource.h>

G_BEGIN_DECLS

/* useful macro */
#define GBOOLEAN_TO_POINTER(i) (GINT_TO_POINTER((i) ? 2 : 1))
#define GPOINTER_TO_BOOLEAN(i)                                                 \
    ((gboolean)((GPOINTER_TO_INT(i) == 2) ? TRUE : FALSE))

enum { GEDIT_ALL_WORKSPACES = 0xffffffff };

gboolean bedit_utils_menu_position_under_tree_view(
    GtkTreeView *tree_view, GdkRectangle *rect);

void bedit_utils_set_atk_name_description(
    GtkWidget *widget, const gchar *name, const gchar *description);
void bedit_warning(GtkWindow *parent, const gchar *format, ...)
    G_GNUC_PRINTF(2, 3);

gchar *bedit_utils_location_get_dirname_for_display(GFile *location);
guint bedit_utils_get_current_workspace(GdkScreen *screen);
guint bedit_utils_get_window_workspace(GtkWindow *gtkwindow);
void bedit_utils_get_current_viewport(GdkScreen *screen, gint *x, gint *y);
gboolean bedit_utils_is_valid_location(GFile *location);

gchar *bedit_utils_basename_for_display(GFile *location);
gboolean bedit_utils_decode_uri(
    const gchar *uri, gchar **scheme, gchar **user, gchar **host, gchar **port,
    gchar **path);

/* Turns data from a drop into a list of well formatted uris */
gchar **bedit_utils_drop_get_uris(GtkSelectionData *selection_data);

GtkSourceCompressionType bedit_utils_get_compression_type_from_content_type(
    const gchar *content_type);
gchar *bedit_utils_set_direct_save_filename(GdkDragContext *context);
const gchar *bedit_utils_newline_type_to_string(
    GtkSourceNewlineType newline_type);

G_DEPRECATED_FOR(g_utf8_make_valid)
gchar *bedit_utils_make_valid_utf8(const char *name);

gchar *bedit_utils_str_middle_truncate(
    const gchar *string, guint truncate_length);
gchar *bedit_utils_str_end_truncate(const gchar *string, guint truncate_length);
gchar *bedit_utils_replace_home_dir_with_tilde(const gchar *uri);

G_DEPRECATED_FOR(gtk_menu_popup_at_widget)
void bedit_utils_menu_position_under_widget(
    GtkMenu *menu, gint *x, gint *y, gboolean *push_in, gpointer user_data);

G_DEPRECATED
void bedit_utils_set_atk_relation(
    GtkWidget *obj1, GtkWidget *obj2, AtkRelationType rel_type);

G_END_DECLS

#endif /* GEDIT_UTILS_H */

