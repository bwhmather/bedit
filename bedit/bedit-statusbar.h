/*
 * bedit-statusbar.h
 * This file is part of bedit
 *
 * Copyright (C) 2005 - Paolo Borelli
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

#ifndef GEDIT_STATUSBAR_H
#define GEDIT_STATUSBAR_H

#include <bedit/bedit-window.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GEDIT_TYPE_STATUSBAR (bedit_statusbar_get_type())

G_DECLARE_FINAL_TYPE(
    BeditStatusbar, bedit_statusbar, GEDIT, STATUSBAR, GtkStatusbar)

GtkWidget *bedit_statusbar_new(void);

void bedit_statusbar_set_window_state(
    BeditStatusbar *statusbar, BeditWindowState state, gint num_of_errors);

void bedit_statusbar_set_overwrite(
    BeditStatusbar *statusbar, gboolean overwrite);

void bedit_statusbar_clear_overwrite(BeditStatusbar *statusbar);

void bedit_statusbar_flash_message(
    BeditStatusbar *statusbar, guint context_id, const gchar *format, ...)
    G_GNUC_PRINTF(3, 4);

G_END_DECLS

#endif

