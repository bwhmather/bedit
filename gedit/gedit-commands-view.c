/*
 * gedit-view-commands.c
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the gedit Team, 1998-2005. See the AUTHORS file for a
 * list of people on the gedit Team.
 * See the ChangeLog files for a list of changes.
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>

#include "gedit-commands.h"
#include "gedit-debug.h"
#include "gedit-window.h"
#include "gedit-window-private.h"
#include "gedit-highlight-mode-dialog.h"

void
_gedit_cmd_view_toggle_side_panel (GSimpleAction *action,
                                   GVariant      *state,
                                   gpointer       user_data)
{
	GeditWindow *window = GEDIT_WINDOW (user_data);
	GtkWidget *panel;
	gboolean visible;

	gedit_debug (DEBUG_COMMANDS);

	panel = gedit_window_get_side_panel (window);

	visible = g_variant_get_boolean (state);
	gtk_widget_set_visible (panel, visible);

	if (visible)
	{
		gtk_widget_grab_focus (panel);
	}

	g_simple_action_set_state (action, state);
}

void
_gedit_cmd_view_toggle_bottom_panel (GSimpleAction *action,
                                     GVariant      *state,
                                     gpointer       user_data)
{
	GeditWindow *window = GEDIT_WINDOW (user_data);
	GtkWidget *panel_box;
	gboolean visible;

	gedit_debug (DEBUG_COMMANDS);

	panel_box = window->priv->bottom_panel_box;

	visible = g_variant_get_boolean (state);
	gtk_widget_set_visible (panel_box, visible);

	if (visible)
	{
		gtk_widget_grab_focus (window->priv->bottom_panel);
	}

	g_simple_action_set_state (action, state);
}

void
_gedit_cmd_view_toggle_fullscreen_mode (GSimpleAction *action,
                                        GVariant      *state,
                                        gpointer       user_data)
{
	GeditWindow *window = GEDIT_WINDOW (user_data);

	gedit_debug (DEBUG_COMMANDS);

	if (g_variant_get_boolean (state))
	{
		_gedit_window_fullscreen (window);
	}
	else
	{
		_gedit_window_unfullscreen (window);
	}

	g_simple_action_set_state (action, state);
}

void
_gedit_cmd_view_leave_fullscreen_mode (GSimpleAction *action,
                                       GVariant      *parameter,
                                       gpointer       user_data)
{
	GeditWindow *window = GEDIT_WINDOW (user_data);
	GAction *fullscreen_action;

	_gedit_window_unfullscreen (window);

	fullscreen_action = g_action_map_lookup_action (G_ACTION_MAP (window),
	                                                "fullscreen");

	g_simple_action_set_state (G_SIMPLE_ACTION (fullscreen_action),
	                           g_variant_new_boolean (FALSE));
}

static void
on_language_selected (GeditHighlightModeDialog *dlg,
                      GtkSourceLanguage        *language,
                      GeditWindow              *window)
{
	GeditDocument *doc;

	doc = gedit_window_get_active_document (window);

	if (!doc)
		return;

	gedit_document_set_language (doc, language);
}

void
_gedit_cmd_view_highlight_mode (GSimpleAction *action,
                                GVariant      *parameter,
                                gpointer       user_data)
{
	GtkWindow *window = GTK_WINDOW (user_data);
	GtkWidget *dlg;
	GeditDocument *doc;

	dlg = gedit_highlight_mode_dialog_new (window);

	doc = gedit_window_get_active_document (GEDIT_WINDOW (window));
	if (doc)
	{
		gedit_highlight_mode_dialog_select_language (GEDIT_HIGHLIGHT_MODE_DIALOG (dlg),
		                                             gedit_document_get_language (doc));
	}

	g_signal_connect (dlg, "language-selected",
	                  G_CALLBACK (on_language_selected), window);

	gtk_widget_show (GTK_WIDGET (dlg));
}

/* ex:set ts=8 noet: */
