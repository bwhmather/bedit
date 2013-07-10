/*
 * gedit-commands-search.c
 * This file is part of gedit
 *
 * Copyright (C) 1998, 1999 Alex Roberts, Evan Lawrence
 * Copyright (C) 2000, 2001 Chema Celorio, Paolo Maggi
 * Copyright (C) 2002-2006 Paolo Maggi
 * Copyright (C) 2013 Sébastien Wilmet
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
 * Modified by the gedit Team, 1998-2013. See the AUTHORS file for a
 * list of people on the gedit Team.
 * See the ChangeLog files for a list of changes.
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>

#include "gedit-commands.h"
#include "gedit-debug.h"
#include "gedit-statusbar.h"
#include "gedit-view-frame.h"
#include "gedit-window.h"
#include "gedit-window-private.h"
#include "gedit-utils.h"
#include "gedit-replace-dialog.h"

#define GEDIT_REPLACE_DIALOG_KEY	"gedit-replace-dialog-key"
#define GEDIT_LAST_SEARCH_DATA_KEY	"gedit-last-search-data-key"

typedef struct _LastSearchData LastSearchData;
struct _LastSearchData
{
	gint x;
	gint y;
};

static void
last_search_data_free (LastSearchData *data)
{
	g_slice_free (LastSearchData, data);
}

static void
last_search_data_restore_position (GeditReplaceDialog *dlg)
{
	LastSearchData *data;

	data = g_object_get_data (G_OBJECT (dlg), GEDIT_LAST_SEARCH_DATA_KEY);

	if (data != NULL)
	{
		gtk_window_move (GTK_WINDOW (dlg),
				 data->x,
				 data->y);
	}
}

static void
last_search_data_store_position (GeditReplaceDialog *dlg)
{
	LastSearchData *data;

	data = g_object_get_data (G_OBJECT (dlg), GEDIT_LAST_SEARCH_DATA_KEY);

	if (data == NULL)
	{
		data = g_slice_new (LastSearchData);

		g_object_set_data_full (G_OBJECT (dlg),
					GEDIT_LAST_SEARCH_DATA_KEY,
					data,
					(GDestroyNotify) last_search_data_free);
	}

	gtk_window_get_position (GTK_WINDOW (dlg),
				 &data->x,
				 &data->y);
}

/* Use occurences only for Replace All */
static void
text_found (GeditWindow *window,
	    gint         occurrences)
{
	if (occurrences > 1)
	{
		gedit_statusbar_flash_message (GEDIT_STATUSBAR (window->priv->statusbar),
					       window->priv->generic_message_cid,
					       ngettext("Found and replaced %d occurrence",
					     	        "Found and replaced %d occurrences",
					     	        occurrences),
					       occurrences);
	}
	else if (occurrences == 1)
	{
		gedit_statusbar_flash_message (GEDIT_STATUSBAR (window->priv->statusbar),
					       window->priv->generic_message_cid,
					       _("Found and replaced one occurrence"));
	}
	else
	{
		gedit_statusbar_flash_message (GEDIT_STATUSBAR (window->priv->statusbar),
					       window->priv->generic_message_cid,
					       " ");
	}
}

#define MAX_MSG_LENGTH 40
static void
text_not_found (GeditWindow *window,
		const gchar *text)
{
	gchar *searched;

	searched = gedit_utils_str_end_truncate (text, MAX_MSG_LENGTH);

	gedit_statusbar_flash_message (GEDIT_STATUSBAR (window->priv->statusbar),
				       window->priv->generic_message_cid,
				       /* Translators: %s is replaced by the text
				          entered by the user in the search box */
				       _("\"%s\" not found"), searched);
	g_free (searched);
}

static void
finish_search_from_dialog (GeditWindow *window,
			   gboolean     found)
{
	GeditReplaceDialog *replace_dialog;

	replace_dialog = g_object_get_data (G_OBJECT (window), GEDIT_REPLACE_DIALOG_KEY);

	g_return_if_fail (replace_dialog != NULL);

	if (found)
	{
		text_found (window, 0);
	}
	else
	{
		const gchar *search_text = gedit_replace_dialog_get_search_text (replace_dialog);
		text_not_found (window, search_text);
	}

	gtk_dialog_set_response_sensitive (GTK_DIALOG (replace_dialog),
					   GEDIT_REPLACE_DIALOG_REPLACE_RESPONSE,
					   found);
}

static gboolean
forward_search_finished (GtkSourceBuffer *buffer,
			 GAsyncResult    *result,
			 GeditView       *view)
{
	gboolean found;
	GtkTextIter match_start;
	GtkTextIter match_end;

	found = gtk_source_buffer_forward_search_finish (buffer,
							 result,
							 &match_start,
							 &match_end,
							 NULL);

	if (found)
	{
		gtk_text_buffer_select_range (GTK_TEXT_BUFFER (buffer),
					      &match_start,
					      &match_end);

		gedit_view_scroll_to_cursor (view);
	}
	else
	{
		GtkTextIter end_selection;

		gtk_text_buffer_get_selection_bounds (GTK_TEXT_BUFFER (buffer),
						      NULL,
						      &end_selection);

		gtk_text_buffer_select_range (GTK_TEXT_BUFFER (buffer),
					      &end_selection,
					      &end_selection);
	}

	return found;
}

static void
forward_search_from_dialog_finished (GtkSourceBuffer *buffer,
				     GAsyncResult    *result,
				     GeditWindow     *window)
{
	GeditView *view = gedit_window_get_active_view (window);
	gboolean found;

	if (view == NULL)
	{
		return;
	}

	found = forward_search_finished (buffer, result, view);

	finish_search_from_dialog (window, found);
}

static void
run_forward_search (GeditWindow *window,
		    gboolean     from_dialog)
{
	GeditView *view;
	GtkTextBuffer *buffer;
	GtkTextIter start_at;

	view = gedit_window_get_active_view (window);

	if (view == NULL)
	{
		return;
	}

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

	gtk_text_buffer_get_selection_bounds (buffer, NULL, &start_at);

	if (from_dialog)
	{
		gtk_source_buffer_forward_search_async (GTK_SOURCE_BUFFER (buffer),
							&start_at,
							NULL,
							(GAsyncReadyCallback)forward_search_from_dialog_finished,
							window);
	}
	else
	{
		gtk_source_buffer_forward_search_async (GTK_SOURCE_BUFFER (buffer),
							&start_at,
							NULL,
							(GAsyncReadyCallback)forward_search_finished,
							view);
	}
}

static gboolean
backward_search_finished (GtkSourceBuffer *buffer,
			  GAsyncResult    *result,
			  GeditView       *view)
{
	gboolean found;
	GtkTextIter match_start;
	GtkTextIter match_end;

	found = gtk_source_buffer_backward_search_finish (buffer,
							  result,
							  &match_start,
							  &match_end,
							  NULL);

	if (found)
	{
		gtk_text_buffer_select_range (GTK_TEXT_BUFFER (buffer),
					      &match_start,
					      &match_end);

		gedit_view_scroll_to_cursor (view);
	}
	else
	{
		GtkTextIter start_selection;

		gtk_text_buffer_get_selection_bounds (GTK_TEXT_BUFFER (buffer),
						      &start_selection,
						      NULL);

		gtk_text_buffer_select_range (GTK_TEXT_BUFFER (buffer),
					      &start_selection,
					      &start_selection);
	}

	return found;
}

static void
backward_search_from_dialog_finished (GtkSourceBuffer *buffer,
				      GAsyncResult    *result,
				      GeditWindow     *window)
{
	GeditView *view = gedit_window_get_active_view (window);
	gboolean found;

	if (view == NULL)
	{
		return;
	}

	found = backward_search_finished (buffer, result, view);

	finish_search_from_dialog (window, found);
}

static void
run_backward_search (GeditWindow *window,
		     gboolean     from_dialog)
{
	GeditView *view;
	GtkTextBuffer *buffer;
	GtkTextIter start_at;

	view = gedit_window_get_active_view (window);

	if (view == NULL)
	{
		return;
	}

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

	gtk_text_buffer_get_selection_bounds (buffer, &start_at, NULL);

	if (from_dialog)
	{
		gtk_source_buffer_backward_search_async (GTK_SOURCE_BUFFER (buffer),
							 &start_at,
							 NULL,
							 (GAsyncReadyCallback)backward_search_from_dialog_finished,
							 window);
	}
	else
	{
		gtk_source_buffer_backward_search_async (GTK_SOURCE_BUFFER (buffer),
							 &start_at,
							 NULL,
							 (GAsyncReadyCallback)backward_search_finished,
							 view);
	}
}

static void
set_search_state (GeditReplaceDialog *dialog,
		  GtkSourceBuffer    *buffer)
{
	const gchar *search_text;
	gchar *unescaped_search_text;
	gboolean match_case;
	gboolean entire_word;
	gboolean wrap_around;

	search_text = gedit_replace_dialog_get_search_text (dialog);
	unescaped_search_text = gtk_source_utils_unescape_search_text (search_text);
	gtk_source_buffer_set_search_text (buffer, unescaped_search_text);
	g_free (unescaped_search_text);

	match_case = gedit_replace_dialog_get_match_case (dialog);
	gtk_source_buffer_set_case_sensitive_search (buffer, match_case);

	entire_word = gedit_replace_dialog_get_entire_word (dialog);
	gtk_source_buffer_set_search_at_word_boundaries (buffer, entire_word);

	wrap_around = gedit_replace_dialog_get_wrap_around (dialog);
	gtk_source_buffer_set_search_wrap_around (buffer, wrap_around);
}

static void
do_find (GeditReplaceDialog *dialog,
	 GeditWindow        *window)
{
	GeditView *active_view;
	GtkSourceBuffer *buffer;

	/* TODO: make the dialog insensitive when all the tabs are closed
	 * and assert here that the view is not NULL */
	active_view = gedit_window_get_active_view (window);

	if (active_view == NULL)
	{
		return;
	}

	buffer = GTK_SOURCE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (active_view)));

	set_search_state (dialog, buffer);

	if (gedit_replace_dialog_get_backwards (dialog))
	{
		run_backward_search (window, TRUE);
	}
	else
	{
		run_forward_search (window, TRUE);
	}
}

/* FIXME: move in gedit-document.c and share it with gedit-view */
static gboolean
get_selected_text (GtkTextBuffer  *doc,
		   gchar         **selected_text,
		   gint           *len)
{
	GtkTextIter start, end;

	g_return_val_if_fail (selected_text != NULL, FALSE);
	g_return_val_if_fail (*selected_text == NULL, FALSE);

	if (!gtk_text_buffer_get_selection_bounds (doc, &start, &end))
	{
		if (len != NULL)
			len = 0;

		return FALSE;
	}

	*selected_text = gtk_text_buffer_get_slice (doc, &start, &end, TRUE);

	if (len != NULL)
		*len = g_utf8_strlen (*selected_text, -1);

	return TRUE;
}

static void
do_replace (GeditReplaceDialog *dialog,
	    GeditWindow        *window)
{
	GtkSourceBuffer *buffer;
	const gchar *replace_entry_text;
	gchar *unescaped_replace_text;
	GtkTextIter start;
	GtkTextIter end;

	buffer = GTK_SOURCE_BUFFER (gedit_window_get_active_document (window));

	if (buffer == NULL)
	{
		return;
	}

	set_search_state (dialog, buffer);

	/* replace text may be "", we just delete */
	replace_entry_text = gedit_replace_dialog_get_replace_text (dialog);
	g_return_if_fail (replace_entry_text != NULL);

	unescaped_replace_text = gtk_source_utils_unescape_search_text (replace_entry_text);

	gtk_text_buffer_get_selection_bounds (GTK_TEXT_BUFFER (buffer), &start, &end);

	gtk_source_buffer_search_replace (buffer, &start, &end, unescaped_replace_text, -1);

	g_free (unescaped_replace_text);

	do_find (dialog, window);
}

static void
do_replace_all (GeditReplaceDialog *dialog,
		GeditWindow        *window)
{
	GeditView *active_view;
	GtkSourceBuffer *buffer;
	const gchar *replace_entry_text;
	gchar *unescaped_replace_text;
	gint count;

	active_view = gedit_window_get_active_view (window);

	if (active_view == NULL)
	{
		return;
	}

	buffer = GTK_SOURCE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (active_view)));

	set_search_state (dialog, buffer);

	/* replace text may be "", we just delete all occurrences */
	replace_entry_text = gedit_replace_dialog_get_replace_text (dialog);
	g_return_if_fail (replace_entry_text != NULL);

	unescaped_replace_text = gtk_source_utils_unescape_search_text (replace_entry_text);

	count = gtk_source_buffer_search_replace_all (buffer, unescaped_replace_text, -1);

	g_free (unescaped_replace_text);

	if (count > 0)
	{
		text_found (window, count);
	}
	else
	{
		const gchar *search_entry_text = gedit_replace_dialog_get_search_text (dialog);
		text_not_found (window, search_entry_text);
	}

	gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog),
					   GEDIT_REPLACE_DIALOG_REPLACE_RESPONSE,
					   FALSE);
}

static void
replace_dialog_response_cb (GeditReplaceDialog *dialog,
			    gint                response_id,
			    GeditWindow        *window)
{
	gedit_debug (DEBUG_COMMANDS);

	switch (response_id)
	{
		case GEDIT_REPLACE_DIALOG_FIND_RESPONSE:
			do_find (dialog, window);
			break;

		case GEDIT_REPLACE_DIALOG_REPLACE_RESPONSE:
			do_replace (dialog, window);
			break;

		case GEDIT_REPLACE_DIALOG_REPLACE_ALL_RESPONSE:
			do_replace_all (dialog, window);
			break;

		default:
			last_search_data_store_position (dialog);
			gtk_widget_hide (GTK_WIDGET (dialog));
	}
}

static void
replace_dialog_destroyed (GeditWindow        *window,
			  GeditReplaceDialog *dialog)
{
	gedit_debug (DEBUG_COMMANDS);

	g_object_set_data (G_OBJECT (window),
			   GEDIT_REPLACE_DIALOG_KEY,
			   NULL);
	g_object_set_data (G_OBJECT (dialog),
			   GEDIT_LAST_SEARCH_DATA_KEY,
			   NULL);
}

static GtkWidget *
create_dialog (GeditWindow *window)
{
	GtkWidget *dialog;

	dialog = gedit_replace_dialog_new (GTK_WINDOW (window));

	g_signal_connect (dialog,
			  "response",
			  G_CALLBACK (replace_dialog_response_cb),
			  window);

	g_object_set_data (G_OBJECT (window),
			   GEDIT_REPLACE_DIALOG_KEY,
			   dialog);

	g_object_weak_ref (G_OBJECT (dialog),
			   (GWeakNotify) replace_dialog_destroyed,
			   window);

	return dialog;
}

void
_gedit_cmd_search_find (GtkAction   *action,
			GeditWindow *window)
{
	GeditTab *active_tab;
	GeditViewFrame *frame;

	gedit_debug (DEBUG_COMMANDS);

	active_tab = gedit_window_get_active_tab (window);

	if (active_tab == NULL)
	{
		return;
	}

	frame = GEDIT_VIEW_FRAME (_gedit_tab_get_view_frame (active_tab));
	gedit_view_frame_popup_search (frame);
}

void
_gedit_cmd_search_replace (GtkAction   *action,
			   GeditWindow *window)
{
	gpointer data;
	GtkWidget *replace_dialog;
	GeditDocument *doc;
	gboolean selection_exists;
	gchar *find_text = NULL;
	gint sel_len;

	gedit_debug (DEBUG_COMMANDS);

	data = g_object_get_data (G_OBJECT (window), GEDIT_REPLACE_DIALOG_KEY);

	if (data == NULL)
	{
		replace_dialog = create_dialog (window);
	}
	else
	{
		g_return_if_fail (GEDIT_IS_REPLACE_DIALOG (data));

		replace_dialog = GTK_WIDGET (data);
	}

	doc = gedit_window_get_active_document (window);
	g_return_if_fail (doc != NULL);

	selection_exists = get_selected_text (GTK_TEXT_BUFFER (doc),
					      &find_text,
					      &sel_len);

	if (selection_exists && find_text != NULL && sel_len < 80)
	{
		gedit_replace_dialog_set_search_text (GEDIT_REPLACE_DIALOG (replace_dialog),
						      find_text);
	}

	g_free (find_text);

	gtk_widget_show (replace_dialog);
	last_search_data_restore_position (GEDIT_REPLACE_DIALOG (replace_dialog));
	gedit_replace_dialog_present_with_time (GEDIT_REPLACE_DIALOG (replace_dialog),
					        GDK_CURRENT_TIME);
}

static void
do_find_again (GeditWindow *window,
	       gboolean     backward)
{
	GeditView *active_view;
	gpointer data;

	active_view = gedit_window_get_active_view (window);
	g_return_if_fail (active_view != NULL);

	data = g_object_get_data (G_OBJECT (window), GEDIT_REPLACE_DIALOG_KEY);

	if (data != NULL)
	{
		GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (active_view));

		set_search_state (GEDIT_REPLACE_DIALOG (data),
				  GTK_SOURCE_BUFFER (buffer));
	}

	if (backward)
	{
		run_backward_search (window, FALSE);
	}
	else
	{
		run_forward_search (window, FALSE);
	}
}

void
_gedit_cmd_search_find_next (GtkAction   *action,
			     GeditWindow *window)
{
	gedit_debug (DEBUG_COMMANDS);

	do_find_again (window, FALSE);
}

void
_gedit_cmd_search_find_prev (GtkAction   *action,
			     GeditWindow *window)
{
	gedit_debug (DEBUG_COMMANDS);

	do_find_again (window, TRUE);
}

void
_gedit_cmd_search_clear_highlight (GtkAction   *action,
				   GeditWindow *window)
{
	GeditTab *active_tab;
	GeditViewFrame *frame;

	gedit_debug (DEBUG_COMMANDS);

	active_tab = gedit_window_get_active_tab (window);

	if (active_tab == NULL)
	{
		return;
	}

	frame = GEDIT_VIEW_FRAME (_gedit_tab_get_view_frame (active_tab));
	gedit_view_frame_clear_search (frame);
}

void
_gedit_cmd_search_goto_line (GtkAction   *action,
			     GeditWindow *window)
{
	GeditTab *active_tab;
	GeditViewFrame *frame;

	gedit_debug (DEBUG_COMMANDS);

	active_tab = gedit_window_get_active_tab (window);

	if (active_tab == NULL)
	{
		return;
	}

	frame = GEDIT_VIEW_FRAME (_gedit_tab_get_view_frame (active_tab));
	gedit_view_frame_popup_goto_line (frame);
}

/* ex:set ts=8 noet: */
