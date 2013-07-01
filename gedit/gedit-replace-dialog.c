/*
 * gedit-replace-dialog.c
 * This file is part of gedit
 *
 * Copyright (C) 2005 Paolo Maggi
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
#include "gedit-utils.h"
#include "gedit-marshal.h"
#include "gedit-dirs.h"

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
	GtkWidget *backwards_checkbutton;
	GtkWidget *wrap_around_checkbutton;

	gboolean   ui_error;
};

G_DEFINE_TYPE(GeditReplaceDialog, gedit_replace_dialog, GTK_TYPE_DIALOG)

void
gedit_replace_dialog_present_with_time (GeditReplaceDialog *dialog,
				       guint32            timestamp)
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
gedit_replace_dialog_response (GtkDialog *dialog,
                               gint       response_id)
{
	GeditReplaceDialog *dlg = GEDIT_REPLACE_DIALOG (dialog);
	const gchar *str;

	switch (response_id)
	{
		case GEDIT_REPLACE_DIALOG_REPLACE_RESPONSE:
		case GEDIT_REPLACE_DIALOG_REPLACE_ALL_RESPONSE:
			str = gtk_entry_get_text (GTK_ENTRY (dlg->priv->replace_text_entry));
			if (*str != '\0')
			{
				gchar *text;

				text = gedit_utils_unescape_search_text (str);
				gedit_history_entry_prepend_text
						(GEDIT_HISTORY_ENTRY (dlg->priv->replace_entry),
						 text);

				g_free (text);
			}
			/* fall through, so that we also save the find entry */
		case GEDIT_REPLACE_DIALOG_FIND_RESPONSE:
			str = gtk_entry_get_text (GTK_ENTRY (dlg->priv->search_text_entry));
			if (*str != '\0')
			{
				gchar *text;

				text = gedit_utils_unescape_search_text (str);
				gedit_history_entry_prepend_text
						(GEDIT_HISTORY_ENTRY (dlg->priv->search_entry),
						 text);

				g_free (text);
			}
	}
}

static void
gedit_replace_dialog_class_init (GeditReplaceDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
	GtkDialogClass *dialog_class = GTK_DIALOG_CLASS (klass);

	widget_class->delete_event = gedit_replace_dialog_delete_event;
	dialog_class->response = gedit_replace_dialog_response;

	/* Bind class to template */
	gtk_widget_class_set_template_from_resource (widget_class,
	                                             "/org/gnome/gedit/ui/gedit-replace-dialog.ui");
	gtk_widget_class_bind_child (widget_class, GeditReplaceDialogPrivate, grid);
	gtk_widget_class_bind_child (widget_class, GeditReplaceDialogPrivate, search_label);
	gtk_widget_class_bind_child (widget_class, GeditReplaceDialogPrivate, replace_label);
	gtk_widget_class_bind_child (widget_class, GeditReplaceDialogPrivate, match_case_checkbutton);
	gtk_widget_class_bind_child (widget_class, GeditReplaceDialogPrivate, entire_word_checkbutton);
	gtk_widget_class_bind_child (widget_class, GeditReplaceDialogPrivate, backwards_checkbutton);
	gtk_widget_class_bind_child (widget_class, GeditReplaceDialogPrivate, wrap_around_checkbutton);

	g_type_class_add_private (object_class, sizeof (GeditReplaceDialogPrivate));
}

static void
insert_text_handler (GtkEditable *editable,
		     const gchar *text,
		     gint         length,
		     gint        *position,
		     gpointer     data)
{
	static gboolean insert_text = FALSE;
	gchar *escaped_text;
	gint new_len;

	/* To avoid recursive behavior */
	if (insert_text)
		return;

	escaped_text = gedit_utils_escape_search_text (text);

	new_len = strlen (escaped_text);

	if (new_len == length)
	{
		g_free (escaped_text);
		return;
	}

	insert_text = TRUE;

	g_signal_stop_emission_by_name (editable, "insert_text");

	gtk_editable_insert_text (editable, escaped_text, new_len, position);

	insert_text = FALSE;

	g_free (escaped_text);
}

static void
search_text_entry_changed (GtkEditable        *editable,
			   GeditReplaceDialog *dialog)
{
	const gchar *search_string;

	search_string = gtk_entry_get_text (GTK_ENTRY (editable));
	g_return_if_fail (search_string != NULL);

	if (*search_string != '\0')
	{
		gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog),
			GEDIT_REPLACE_DIALOG_FIND_RESPONSE, TRUE);
		gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog),
			GEDIT_REPLACE_DIALOG_REPLACE_ALL_RESPONSE, TRUE);
	}
	else
	{
		gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog),
			GEDIT_REPLACE_DIALOG_FIND_RESPONSE, FALSE);
		gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog),
			GEDIT_REPLACE_DIALOG_REPLACE_RESPONSE, FALSE);
		gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog),
			GEDIT_REPLACE_DIALOG_REPLACE_ALL_RESPONSE, FALSE);
	}
}

static void
gedit_replace_dialog_init (GeditReplaceDialog *dlg)
{
	dlg->priv = G_TYPE_INSTANCE_GET_PRIVATE (dlg, GEDIT_TYPE_REPLACE_DIALOG, GeditReplaceDialogPrivate);

	gtk_widget_init_template (GTK_WIDGET (dlg));

	dlg->priv->search_entry = gedit_history_entry_new ("search-for-entry", TRUE);
	gtk_widget_set_size_request (dlg->priv->search_entry, 300, -1);
	gedit_history_entry_set_escape_func (GEDIT_HISTORY_ENTRY (dlg->priv->search_entry),
	                                     (GeditHistoryEntryEscapeFunc) gedit_utils_escape_search_text);
	gtk_widget_set_hexpand (GTK_WIDGET (dlg->priv->search_entry), TRUE);
	dlg->priv->search_text_entry = gedit_history_entry_get_entry (GEDIT_HISTORY_ENTRY (dlg->priv->search_entry));
	gtk_entry_set_activates_default (GTK_ENTRY (dlg->priv->search_text_entry), TRUE);
	gtk_grid_attach_next_to (GTK_GRID (dlg->priv->grid),
				 dlg->priv->search_entry,
				 dlg->priv->search_label,
				 GTK_POS_RIGHT, 1, 1);

	dlg->priv->replace_entry = gedit_history_entry_new ("replace-with-entry", TRUE);
	gedit_history_entry_set_escape_func (GEDIT_HISTORY_ENTRY (dlg->priv->replace_entry),
	                                     (GeditHistoryEntryEscapeFunc) gedit_utils_escape_search_text);
	gtk_widget_set_hexpand (GTK_WIDGET (dlg->priv->replace_entry), TRUE);
	dlg->priv->replace_text_entry = gedit_history_entry_get_entry (GEDIT_HISTORY_ENTRY (dlg->priv->replace_entry));
	gtk_entry_set_activates_default (GTK_ENTRY (dlg->priv->replace_text_entry), TRUE);
	gtk_grid_attach_next_to (GTK_GRID (dlg->priv->grid),
				 dlg->priv->replace_entry,
				 dlg->priv->replace_label,
				 GTK_POS_RIGHT, 1, 1);

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
			  "insert_text",
			  G_CALLBACK (insert_text_handler),
			  NULL);
	g_signal_connect (dlg->priv->replace_text_entry,
			  "insert_text",
			  G_CALLBACK (insert_text_handler),
			  NULL);
	g_signal_connect (dlg->priv->search_text_entry,
			  "changed",
			  G_CALLBACK (search_text_entry_changed),
			  dlg);

	gtk_widget_show_all (GTK_WIDGET (dlg));
}

GtkWidget *
gedit_replace_dialog_new (GtkWindow *parent)
{
	GeditReplaceDialog *dlg;

	dlg = g_object_new (GEDIT_TYPE_REPLACE_DIALOG,
			    NULL);

	if (parent != NULL)
	{
		gtk_window_set_transient_for (GTK_WINDOW (dlg),
					      parent);

		gtk_window_set_destroy_with_parent (GTK_WINDOW (dlg),
						    TRUE);
	}

	return GTK_WIDGET (dlg);
}

void
gedit_replace_dialog_set_search_text (GeditReplaceDialog *dialog,
				      const gchar        *text)
{
	g_return_if_fail (GEDIT_IS_REPLACE_DIALOG (dialog));
	g_return_if_fail (text != NULL);

	gtk_entry_set_text (GTK_ENTRY (dialog->priv->search_text_entry),
			    text);

	gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog),
					   GEDIT_REPLACE_DIALOG_FIND_RESPONSE,
					   (text != '\0'));

	gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog),
					   GEDIT_REPLACE_DIALOG_REPLACE_ALL_RESPONSE,
					   (text != '\0'));
}

/* The text must be unescaped before searching */
const gchar *
gedit_replace_dialog_get_search_text (GeditReplaceDialog *dialog)
{
	g_return_val_if_fail (GEDIT_IS_REPLACE_DIALOG (dialog), NULL);

	return gtk_entry_get_text (GTK_ENTRY (dialog->priv->search_text_entry));
}

void
gedit_replace_dialog_set_replace_text (GeditReplaceDialog *dialog,
				       const gchar        *text)
{
	g_return_if_fail (GEDIT_IS_REPLACE_DIALOG (dialog));
	g_return_if_fail (text != NULL);

	gtk_entry_set_text (GTK_ENTRY (dialog->priv->replace_text_entry),
			    text);
}

const gchar *
gedit_replace_dialog_get_replace_text (GeditReplaceDialog *dialog)
{
	g_return_val_if_fail (GEDIT_IS_REPLACE_DIALOG (dialog), NULL);

	return gtk_entry_get_text (GTK_ENTRY (dialog->priv->replace_text_entry));
}

void
gedit_replace_dialog_set_match_case (GeditReplaceDialog *dialog,
				     gboolean            match_case)
{
	g_return_if_fail (GEDIT_IS_REPLACE_DIALOG (dialog));

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->priv->match_case_checkbutton),
				      match_case);
}

gboolean
gedit_replace_dialog_get_match_case (GeditReplaceDialog *dialog)
{
	g_return_val_if_fail (GEDIT_IS_REPLACE_DIALOG (dialog), FALSE);

	return gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->priv->match_case_checkbutton));
}

void
gedit_replace_dialog_set_entire_word (GeditReplaceDialog *dialog,
				      gboolean            entire_word)
{
	g_return_if_fail (GEDIT_IS_REPLACE_DIALOG (dialog));

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->priv->entire_word_checkbutton),
				      entire_word);
}

gboolean
gedit_replace_dialog_get_entire_word (GeditReplaceDialog *dialog)
{
	g_return_val_if_fail (GEDIT_IS_REPLACE_DIALOG (dialog), FALSE);

	return gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->priv->entire_word_checkbutton));
}

void
gedit_replace_dialog_set_backwards (GeditReplaceDialog *dialog,
				    gboolean            backwards)
{
	g_return_if_fail (GEDIT_IS_REPLACE_DIALOG (dialog));

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->priv->backwards_checkbutton),
				      backwards);
}

gboolean
gedit_replace_dialog_get_backwards (GeditReplaceDialog *dialog)
{
	g_return_val_if_fail (GEDIT_IS_REPLACE_DIALOG (dialog), FALSE);

	return gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->priv->backwards_checkbutton));
}

void
gedit_replace_dialog_set_wrap_around (GeditReplaceDialog *dialog,
				      gboolean            wrap_around)
{
	g_return_if_fail (GEDIT_IS_REPLACE_DIALOG (dialog));

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->priv->wrap_around_checkbutton),
				      wrap_around);
}

gboolean
gedit_replace_dialog_get_wrap_around (GeditReplaceDialog *dialog)
{
	g_return_val_if_fail (GEDIT_IS_REPLACE_DIALOG (dialog), FALSE);

	return gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->priv->wrap_around_checkbutton));
}

/* ex:set ts=8 noet: */
