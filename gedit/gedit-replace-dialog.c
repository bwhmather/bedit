/*
 * gedit-replace-dialog.c
 * This file is part of gedit
 *
 * Copyright (C) 2005 Paolo Maggi
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>

#include "gedit-replace-dialog.h"
#include "gedit-history-entry.h"
#include "gedit-document.h"

struct _GeditReplaceDialogPrivate
{
	GtkWidget *grid;
	GtkWidget *search_label;
	GtkWidget *search_entry;
	GtkWidget *search_text_entry;
	GtkWidget *replace_label;
	GtkWidget *replace_entry;
	GtkWidget *replace_text_entry;
	GtkWidget *match_case_checkbutton;
	GtkWidget *entire_word_checkbutton;
	GtkWidget *regex_checkbutton;
	GtkWidget *backwards_checkbutton;
	GtkWidget *wrap_around_checkbutton;

	GtkSourceSearchSettings *search_settings;
};

G_DEFINE_TYPE_WITH_PRIVATE (GeditReplaceDialog, gedit_replace_dialog, GTK_TYPE_DIALOG)

static GeditWindow *
get_gedit_window (GeditReplaceDialog *dialog)
{
	GtkWindow *transient_for = gtk_window_get_transient_for (GTK_WINDOW (dialog));

	if (transient_for != NULL)
	{
		return GEDIT_WINDOW (transient_for);
	}

	return NULL;
}

static GeditDocument *
get_active_document (GeditReplaceDialog *dialog)
{
	GeditWindow *window = get_gedit_window (dialog);

	if (window != NULL)
	{
		return gedit_window_get_active_document (window);
	}

	return NULL;
}

static GtkSourceSearchContext *
get_active_search_context (GeditReplaceDialog *dialog)
{
	GeditDocument *doc;
	GtkSourceSearchContext *search_context;

	doc = get_active_document (dialog);

	if (doc == NULL)
	{
		return NULL;
	}

	search_context = _gedit_document_get_search_context (doc);

	if (search_context != NULL &&
	    dialog->priv->search_settings == gtk_source_search_context_get_settings (search_context))
	{
		return search_context;
	}

	return NULL;
}

void
gedit_replace_dialog_present_with_time (GeditReplaceDialog *dialog,
					guint32             timestamp)
{
	g_return_if_fail (GEDIT_REPLACE_DIALOG (dialog));

	gtk_window_present_with_time (GTK_WINDOW (dialog), timestamp);

	gtk_widget_grab_focus (dialog->priv->search_text_entry);
}

static gboolean
gedit_replace_dialog_delete_event (GtkWidget   *widget,
                                   GdkEventAny *event)
{
	/* prevent destruction */
	return TRUE;
}

static void
set_error (GtkEntry    *entry,
	   const gchar *error_msg)
{
	if (error_msg == NULL || error_msg[0] == '\0')
	{
		gtk_entry_set_icon_from_gicon (entry, GTK_ENTRY_ICON_SECONDARY, NULL);
		gtk_entry_set_icon_tooltip_text (entry, GTK_ENTRY_ICON_SECONDARY, NULL);
	}
	else
	{
		GIcon *icon = g_themed_icon_new_with_default_fallbacks ("dialog-error-symbolic");

		gtk_entry_set_icon_from_gicon (entry, GTK_ENTRY_ICON_SECONDARY, icon);
		gtk_entry_set_icon_tooltip_text (entry, GTK_ENTRY_ICON_SECONDARY, error_msg);

		g_object_unref (icon);
	}
}

static void
set_search_error (GeditReplaceDialog *dialog,
		  const gchar        *error_msg)
{
	set_error (GTK_ENTRY (dialog->priv->search_text_entry), error_msg);
}

static void
set_replace_error (GeditReplaceDialog *dialog,
		   const gchar        *error_msg)
{
	set_error (GTK_ENTRY (dialog->priv->replace_text_entry), error_msg);
}

static void
update_regex_error (GeditReplaceDialog *dialog)
{
	GtkSourceSearchContext *search_context;
	GError *regex_error;
	GtkSourceRegexSearchState regex_state;

	set_search_error (dialog, NULL);
	set_replace_error (dialog, NULL);

	search_context = get_active_search_context (dialog);

	if (search_context == NULL)
	{
		return;
	}

	regex_error = gtk_source_search_context_get_regex_error (search_context);
	regex_state = gtk_source_search_context_get_regex_state (search_context);

	if (regex_error == NULL)
	{
		return;
	}

	switch (regex_state)
	{
		case GTK_SOURCE_REGEX_SEARCH_COMPILATION_ERROR:
		case GTK_SOURCE_REGEX_SEARCH_MATCHING_ERROR:
			set_search_error (dialog, regex_error->message);
			break;

		case GTK_SOURCE_REGEX_SEARCH_REPLACE_ERROR:
			set_replace_error (dialog, regex_error->message);
			break;

		default:
			g_return_if_reached ();
	}

	g_error_free (regex_error);
}

static void
update_responses_sensitivity (GeditReplaceDialog *dialog)
{
	const gchar *search_text;
	GtkSourceSearchContext *search_context;
	GtkSourceRegexSearchState regex_state;
	gboolean sensitive;

	search_text = gtk_entry_get_text (GTK_ENTRY (dialog->priv->search_text_entry));

	if (search_text[0] == '\0')
	{
		gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog),
						   GEDIT_REPLACE_DIALOG_FIND_RESPONSE,
						   FALSE);

		gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog),
						   GEDIT_REPLACE_DIALOG_REPLACE_ALL_RESPONSE,
						   FALSE);

		return;
	}

	search_context = get_active_search_context (dialog);
	regex_state = gtk_source_search_context_get_regex_state (search_context);
	sensitive = (regex_state == GTK_SOURCE_REGEX_SEARCH_NO_ERROR ||
		     regex_state == GTK_SOURCE_REGEX_SEARCH_REPLACE_ERROR);

	gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog),
					   GEDIT_REPLACE_DIALOG_FIND_RESPONSE,
					   sensitive);

	gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog),
					   GEDIT_REPLACE_DIALOG_REPLACE_ALL_RESPONSE,
					   sensitive);
}

static void
regex_error_notify_cb (GeditReplaceDialog *dialog)
{
	update_regex_error (dialog);
	update_responses_sensitivity (dialog);
}

static void
create_search_context (GeditReplaceDialog *dialog)
{
	GtkSourceSearchContext *search_context = get_active_search_context (dialog);
	GeditDocument *doc;

	if (search_context != NULL)
	{
		return;
	}

	doc = get_active_document (dialog);
	search_context = gtk_source_search_context_new (GTK_SOURCE_BUFFER (doc),
							dialog->priv->search_settings);

	g_signal_connect_object (search_context,
				 "notify::regex-error",
				 G_CALLBACK (regex_error_notify_cb),
				 dialog,
				 G_CONNECT_SWAPPED);

	_gedit_document_set_search_context (doc, search_context);
	g_object_unref (search_context);

	update_regex_error (dialog);
	update_responses_sensitivity (dialog);
}

static void
gedit_replace_dialog_response (GtkDialog *dialog,
                               gint       response_id)
{
	GeditReplaceDialog *dlg = GEDIT_REPLACE_DIALOG (dialog);
	const gchar *str;

	create_search_context (GEDIT_REPLACE_DIALOG (dialog));

	switch (response_id)
	{
		case GEDIT_REPLACE_DIALOG_REPLACE_RESPONSE:
		case GEDIT_REPLACE_DIALOG_REPLACE_ALL_RESPONSE:
			str = gtk_entry_get_text (GTK_ENTRY (dlg->priv->replace_text_entry));
			if (*str != '\0')
			{
				gedit_history_entry_prepend_text
						(GEDIT_HISTORY_ENTRY (dlg->priv->replace_entry),
						 str);
			}
			/* fall through, so that we also save the find entry */
		case GEDIT_REPLACE_DIALOG_FIND_RESPONSE:
			str = gtk_entry_get_text (GTK_ENTRY (dlg->priv->search_text_entry));
			if (*str != '\0')
			{
				gedit_history_entry_prepend_text
						(GEDIT_HISTORY_ENTRY (dlg->priv->search_entry),
						 str);
			}
	}
}

static void
gedit_replace_dialog_dispose (GObject *object)
{
	GeditReplaceDialog *dialog = GEDIT_REPLACE_DIALOG (object);

	g_clear_object (&dialog->priv->search_settings);

	G_OBJECT_CLASS (gedit_replace_dialog_parent_class)->dispose (object);
}

static void
gedit_replace_dialog_class_init (GeditReplaceDialogClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
	GtkDialogClass *dialog_class = GTK_DIALOG_CLASS (klass);

	gobject_class->dispose = gedit_replace_dialog_dispose;
	widget_class->delete_event = gedit_replace_dialog_delete_event;
	dialog_class->response = gedit_replace_dialog_response;

	/* Bind class to template */
	gtk_widget_class_set_template_from_resource (widget_class,
	                                             "/org/gnome/gedit/ui/gedit-replace-dialog.ui");
	gtk_widget_class_bind_template_child_private (widget_class, GeditReplaceDialog, grid);
	gtk_widget_class_bind_template_child_private (widget_class, GeditReplaceDialog, search_label);
	gtk_widget_class_bind_template_child_private (widget_class, GeditReplaceDialog, replace_label);
	gtk_widget_class_bind_template_child_private (widget_class, GeditReplaceDialog, match_case_checkbutton);
	gtk_widget_class_bind_template_child_private (widget_class, GeditReplaceDialog, entire_word_checkbutton);
	gtk_widget_class_bind_template_child_private (widget_class, GeditReplaceDialog, regex_checkbutton);
	gtk_widget_class_bind_template_child_private (widget_class, GeditReplaceDialog, backwards_checkbutton);
	gtk_widget_class_bind_template_child_private (widget_class, GeditReplaceDialog, wrap_around_checkbutton);
}

static void
search_text_entry_changed (GtkEditable        *editable,
			   GeditReplaceDialog *dialog)
{
	const gchar *search_text;
	gchar *unescaped_search_text;

	search_text = gtk_entry_get_text (GTK_ENTRY (editable));

	unescaped_search_text = gtk_source_utils_unescape_search_text (search_text);

	gtk_source_search_settings_set_search_text (dialog->priv->search_settings,
						    unescaped_search_text);

	update_responses_sensitivity (dialog);

	g_free (unescaped_search_text);
}

/* TODO: move in gedit-document.c and share it with gedit-view-frame */
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
		{
			len = 0;
		}

		return FALSE;
	}

	*selected_text = gtk_text_buffer_get_slice (doc, &start, &end, TRUE);

	if (len != NULL)
	{
		*len = g_utf8_strlen (*selected_text, -1);
	}

	return TRUE;
}

static void
active_tab_changed_cb (GeditReplaceDialog *dialog)
{
	create_search_context (dialog);
	update_regex_error (dialog);
	update_responses_sensitivity (dialog);
}

static void
show_cb (GeditReplaceDialog *dialog)
{
	GeditWindow *window;
	GeditDocument *doc;
	gboolean selection_exists;
	gchar *selection = NULL;
	gint selection_length;

	window = get_gedit_window (dialog);

	if (window == NULL)
	{
		return;
	}

	g_signal_connect_object (window,
				 "active-tab-changed",
				 G_CALLBACK (active_tab_changed_cb),
				 dialog,
				 G_CONNECT_SWAPPED);

	doc = get_active_document (dialog);

	if (doc == NULL)
	{
		return;
	}

	selection_exists = get_selected_text (GTK_TEXT_BUFFER (doc),
					      &selection,
					      &selection_length);

	if (selection_exists && selection != NULL && selection_length < 80)
	{
		gchar *escaped_selection = gtk_source_utils_escape_search_text (selection);

		gtk_entry_set_text (GTK_ENTRY (dialog->priv->search_text_entry),
				    escaped_selection);

		g_free (escaped_selection);
	}

	create_search_context (dialog);
	update_regex_error (dialog);
	update_responses_sensitivity (dialog);

	g_free (selection);
}

static void
hide_cb (GeditReplaceDialog *dialog)
{
	GeditWindow *window = get_gedit_window (dialog);

	g_signal_handlers_disconnect_by_func (window, active_tab_changed_cb, dialog);
}

static void
gedit_replace_dialog_init (GeditReplaceDialog *dlg)
{
	dlg->priv = gedit_replace_dialog_get_instance_private (dlg);

	gtk_widget_init_template (GTK_WIDGET (dlg));

	dlg->priv->search_entry = gedit_history_entry_new ("search-for-entry", TRUE);
	gtk_widget_set_size_request (dlg->priv->search_entry, 300, -1);
	gtk_widget_set_hexpand (GTK_WIDGET (dlg->priv->search_entry), TRUE);
	dlg->priv->search_text_entry = gedit_history_entry_get_entry (GEDIT_HISTORY_ENTRY (dlg->priv->search_entry));
	gtk_entry_set_activates_default (GTK_ENTRY (dlg->priv->search_text_entry), TRUE);
	gtk_grid_attach_next_to (GTK_GRID (dlg->priv->grid),
				 dlg->priv->search_entry,
				 dlg->priv->search_label,
				 GTK_POS_RIGHT, 1, 1);
	gtk_widget_show_all (dlg->priv->search_entry);

	dlg->priv->replace_entry = gedit_history_entry_new ("replace-with-entry", TRUE);
	gtk_widget_set_hexpand (GTK_WIDGET (dlg->priv->replace_entry), TRUE);
	dlg->priv->replace_text_entry = gedit_history_entry_get_entry (GEDIT_HISTORY_ENTRY (dlg->priv->replace_entry));
	gtk_entry_set_activates_default (GTK_ENTRY (dlg->priv->replace_text_entry), TRUE);
	gtk_grid_attach_next_to (GTK_GRID (dlg->priv->grid),
				 dlg->priv->replace_entry,
				 dlg->priv->replace_label,
				 GTK_POS_RIGHT, 1, 1);
	gtk_widget_show_all (dlg->priv->replace_entry);

	gtk_label_set_mnemonic_widget (GTK_LABEL (dlg->priv->search_label),
				       dlg->priv->search_entry);
	gtk_label_set_mnemonic_widget (GTK_LABEL (dlg->priv->replace_label),
				       dlg->priv->replace_entry);

	gtk_dialog_set_default_response (GTK_DIALOG (dlg),
					 GEDIT_REPLACE_DIALOG_FIND_RESPONSE);

	/* insensitive by default */
	gtk_dialog_set_response_sensitive (GTK_DIALOG (dlg),
					   GEDIT_REPLACE_DIALOG_FIND_RESPONSE,
					   FALSE);
	gtk_dialog_set_response_sensitive (GTK_DIALOG (dlg),
					   GEDIT_REPLACE_DIALOG_REPLACE_RESPONSE,
					   FALSE);
	gtk_dialog_set_response_sensitive (GTK_DIALOG (dlg),
					   GEDIT_REPLACE_DIALOG_REPLACE_ALL_RESPONSE,
					   FALSE);

	g_signal_connect (dlg->priv->search_text_entry,
			  "changed",
			  G_CALLBACK (search_text_entry_changed),
			  dlg);

	dlg->priv->search_settings = gtk_source_search_settings_new ();

	g_object_bind_property (dlg->priv->match_case_checkbutton, "active",
				dlg->priv->search_settings, "case-sensitive",
				G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

	g_object_bind_property (dlg->priv->entire_word_checkbutton, "active",
				dlg->priv->search_settings, "at-word-boundaries",
				G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

	g_object_bind_property (dlg->priv->regex_checkbutton, "active",
				dlg->priv->search_settings, "regex-enabled",
				G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

	g_object_bind_property (dlg->priv->wrap_around_checkbutton, "active",
				dlg->priv->search_settings, "wrap-around",
				G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

	g_signal_connect (dlg,
			  "show",
			  G_CALLBACK (show_cb),
			  NULL);

	g_signal_connect (dlg,
			  "hide",
			  G_CALLBACK (hide_cb),
			  NULL);
}

GtkWidget *
gedit_replace_dialog_new (GeditWindow *window)
{
	GeditReplaceDialog *dialog;

	g_return_val_if_fail (GEDIT_IS_WINDOW (window), NULL);

	dialog = g_object_new (GEDIT_TYPE_REPLACE_DIALOG,
			       "transient-for", window,
			       "destroy-with-parent", TRUE,
			       NULL);

	return GTK_WIDGET (dialog);
}

const gchar *
gedit_replace_dialog_get_replace_text (GeditReplaceDialog *dialog)
{
	g_return_val_if_fail (GEDIT_IS_REPLACE_DIALOG (dialog), NULL);

	return gtk_entry_get_text (GTK_ENTRY (dialog->priv->replace_text_entry));
}

gboolean
gedit_replace_dialog_get_backwards (GeditReplaceDialog *dialog)
{
	g_return_val_if_fail (GEDIT_IS_REPLACE_DIALOG (dialog), FALSE);

	return gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->priv->backwards_checkbutton));
}

/* This function returns the original search text. The search text from the
 * search settings has been unescaped, and the escape function is not
 * reciprocal. So to avoid bugs, we have to deal with the original search text.
 */
const gchar *
gedit_replace_dialog_get_search_text (GeditReplaceDialog *dialog)
{
	g_return_val_if_fail (GEDIT_IS_REPLACE_DIALOG (dialog), NULL);

	return gtk_entry_get_text (GTK_ENTRY (dialog->priv->search_text_entry));
}

/* ex:set ts=8 noet: */
