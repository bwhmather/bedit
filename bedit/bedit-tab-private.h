/*
 * bedit-tab.h
 * This file is part of bedit
 *
 * Copyright (C) 2005 - Paolo Maggi
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

#ifndef BEDIT_TAB_PRIVATE_H
#define BEDIT_TAB_PRIVATE_H

#include "bedit-tab.h"
#include "bedit-view-frame.h"

G_BEGIN_DECLS

BeditTab *_bedit_tab_new(void);

gchar *_bedit_tab_get_name(BeditTab *tab);

gchar *_bedit_tab_get_tooltip(BeditTab *tab);

GdkPixbuf *_bedit_tab_get_icon(BeditTab *tab);

void _bedit_tab_load(
    BeditTab *tab, GFile *location, const GtkSourceEncoding *encoding,
    gint line_pos, gint column_pos, gboolean create);

void _bedit_tab_load_stream(
    BeditTab *tab, GInputStream *location, const GtkSourceEncoding *encoding,
    gint line_pos, gint column_pos);

void _bedit_tab_revert(BeditTab *tab);

void _bedit_tab_save_async(
    BeditTab *tab, GCancellable *cancellable, GAsyncReadyCallback callback,
    gpointer user_data);

gboolean _bedit_tab_save_finish(BeditTab *tab, GAsyncResult *result);

void _bedit_tab_save_as_async(
    BeditTab *tab, GFile *location, const GtkSourceEncoding *encoding,
    GtkSourceNewlineType newline_type,
    GtkSourceCompressionType compression_type, GCancellable *cancellable,
    GAsyncReadyCallback callback, gpointer user_data);

void _bedit_tab_print(BeditTab *tab);

void _bedit_tab_mark_for_closing(BeditTab *tab);

gboolean _bedit_tab_get_can_close(BeditTab *tab);

BeditViewFrame *_bedit_tab_get_view_frame(BeditTab *tab);

void _bedit_tab_set_network_available(BeditTab *tab, gboolean enable);

G_END_DECLS

#endif /* BEDIT_TAB_PRIVATE_H */

