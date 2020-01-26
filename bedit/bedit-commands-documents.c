/*
 * gedit-documents-commands.c
 * This file is part of gedit
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

#include "gedit-commands.h"
#include "gedit-commands-private.h"

#include <gtk/gtk.h>

#include "gedit-window.h"
#include "gedit-notebook.h"
#include "gedit-multi-notebook.h"
#include "gedit-debug.h"

void
_gedit_cmd_documents_previous_document (GSimpleAction *action,
                                        GVariant      *parameter,
                                        gpointer       user_data)
{
	BeditWindow *window = GEDIT_WINDOW (user_data);
	GtkNotebook *notebook;

	gedit_debug (DEBUG_COMMANDS);

	notebook = GTK_NOTEBOOK (_gedit_window_get_notebook (window));
	gtk_notebook_prev_page (notebook);
}

void
_gedit_cmd_documents_next_document (GSimpleAction *action,
                                    GVariant      *parameter,
                                    gpointer       user_data)
{
	BeditWindow *window = GEDIT_WINDOW (user_data);
	GtkNotebook *notebook;

	gedit_debug (DEBUG_COMMANDS);

	notebook = GTK_NOTEBOOK (_gedit_window_get_notebook (window));
	gtk_notebook_next_page (notebook);
}

void
_gedit_cmd_documents_move_to_new_window (GSimpleAction *action,
                                         GVariant      *parameter,
                                         gpointer       user_data)
{
	BeditWindow *window = GEDIT_WINDOW (user_data);
	BeditTab *tab;

	gedit_debug (DEBUG_COMMANDS);

	tab = gedit_window_get_active_tab (window);

	if (tab == NULL)
		return;

	_gedit_window_move_tab_to_new_window (window, tab);
}

/* Methods releated with the tab groups */
void
_gedit_cmd_documents_new_tab_group (GSimpleAction *action,
                                    GVariant      *parameter,
                                    gpointer       user_data)
{
	gedit_multi_notebook_add_new_notebook (GEDIT_MULTI_NOTEBOOK (_gedit_window_get_multi_notebook (GEDIT_WINDOW (user_data))));
}

void
_gedit_cmd_documents_previous_tab_group (GSimpleAction *action,
                                         GVariant      *parameter,
                                         gpointer       user_data)
{
	gedit_multi_notebook_previous_notebook (GEDIT_MULTI_NOTEBOOK (_gedit_window_get_multi_notebook (GEDIT_WINDOW (user_data))));
}

void
_gedit_cmd_documents_next_tab_group (GSimpleAction *action,
                                     GVariant      *parameter,
                                     gpointer       user_data)
{
	gedit_multi_notebook_next_notebook (GEDIT_MULTI_NOTEBOOK (_gedit_window_get_multi_notebook (GEDIT_WINDOW (user_data))));
}

/* ex:set ts=8 noet: */
