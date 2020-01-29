/*
 * bedit-documents-commands.c
 * This file is part of bedit
 *
 * Copyright (C) 1998, 1999 Alex Roberts, Evan Lawrence
 * Copyright (C) 2000, 2001 Chema Celorio, Paolo Maggi
 * Copyright (C) 2002-2005 Paolo Maggi
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

#include "config.h"

#include "bedit-commands-private.h"
#include "bedit-commands.h"

#include <gtk/gtk.h>

#include "bedit-debug.h"
#include "bedit-notebook.h"
#include "bedit-window.h"

void _bedit_cmd_documents_previous_document(
    GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    BeditWindow *window = BEDIT_WINDOW(user_data);
    GtkNotebook *notebook;

    bedit_debug(DEBUG_COMMANDS);

    notebook = GTK_NOTEBOOK(_bedit_window_get_notebook(window));
    gtk_notebook_prev_page(notebook);
}

void _bedit_cmd_documents_next_document(
    GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    BeditWindow *window = BEDIT_WINDOW(user_data);
    GtkNotebook *notebook;

    bedit_debug(DEBUG_COMMANDS);

    notebook = GTK_NOTEBOOK(_bedit_window_get_notebook(window));
    gtk_notebook_next_page(notebook);
}

void _bedit_cmd_documents_move_to_new_window(
    GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    BeditWindow *window = BEDIT_WINDOW(user_data);
    BeditTab *tab;

    bedit_debug(DEBUG_COMMANDS);

    tab = bedit_window_get_active_tab(window);

    if (tab == NULL)
        return;

    _bedit_window_move_tab_to_new_window(window, tab);
}
