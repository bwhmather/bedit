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
	GtkWidget *regex_checkbutton;
	GtkWidget *backwards_checkbutton;
	GtkWidget *wrap_around_checkbutton;

	GtkSourceSearchSettings *search_settings;

	gboolean ui_error;
};

G_DEFINE_TYPE_WITH_PRIVATE (GeditReplaceDialog, gedit_replace_dialog, GTK_TYPE_DIALOG)

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

				text = gtk_source_utils_unescape_search_text (str);
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

				text = gtk_source_utils_unescape_search_text (str);
				gedit_history_entry_prepend_text
						(GEDIT_HISTORY_ENTRY (dlg->priv->search_entry),
						 text);

				g_free (text);
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
	const gchar *search_string;
	gchar *unescaped_search_string;

	search_string = gtk_entry_get_text (GTK_ENTRY (editable));
	g_return_if_fail (search_string != NULL);

	if (*search_string != '\0')
	{
		gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog),
						   GEDIT_REPLACE_DIALOG_FIND_RESPONSE,
						   TRUE);

		gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog),
						   GEDIT_REPLACE_DIALOG_REPLACE_ALL_RESPONSE,
						   TRUE);
	}
	else
	{
		gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog),
						   GEDIT_REPLACE_DIALOG_FIND_RESPONSE,
						   FALSE);

		gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog),
						   GEDIT_REPLACE_DIALOG_REPLACE_RESPONSE,
						   FALSE);

		gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog),
						   GEDIT_REPLACE_DIALOG_REPLACE_ALL_RESPONSE,
						   FALSE);
	}

	unescaped_search_string = gtk_source_utils_unescape_search_text (search_string);

	gtk_source_search_settings_set_search_text (dialog->priv->search_settings,
						    unescaped_search_string);

	g_free (unescaped_search_string);
}

static void
gedit_replace_dialog_init (GeditReplaceDialog *dlg)
{
	dlg->priv = gedit_replace_dialog_get_instance_private (dlg);

	gtk_widget_init_template (GTK_WIDGET (dlg));

	dlg->priv->search_entry = gedit_history_entry_new ("search-for-entry", TRUE);
	gtk_widget_set_size_request (dlg->priv->search_entry, 300, -1);
	gedit_history_entry_set_escape_func (GEDIT_HISTORY_ENTRY (dlg->priv->search_entry),
	                                     (GeditHistoryEntryEscapeFunc) gtk_source_utils_escape_search_text);
	gtk_widget_set_hexpand (GTK_WIDGET (dlg->priv->search_entry), TRUE);
	dlg->priv->search_text_entry = gedit_history_entry_get_entry (GEDIT_HISTORY_ENTRY (dlg->priv->search_entry));
	gtk_entry_set_activates_default (GTK_ENTRY (dlg->priv->search_text_entry), TRUE);
	gtk_grid_attach_next_to (GTK_GRID (dlg->priv->grid),
				 dlg->priv->search_entry,
				 dlg->priv->search_label,
				 GTK_POS_RIGHT, 1, 1);

	dlg->priv->replace_entry = gedit_history_entry_new ("replace-with-entry", TRUE);
	gedit_history_entry_set_escape_func (GEDIT_HISTORY_ENTRY (dlg->priv->replace_entry),
	                                     (GeditHistoryEntryEscapeFunc) gtk_source_utils_escape_search_text);
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

GtkSourceSearchSettings *
gedit_replace_dialog_get_search_settings (GeditReplaceDialog *dialog)
{
	g_return_val_if_fail (GEDIT_IS_REPLACE_DIALOG (dialog), NULL);

	return dialog->priv->search_settings;
}

void
gedit_replace_dialog_set_search_text (GeditReplaceDialog *dialog,
				      const gchar        *search_text)
{
	g_return_if_fail (GEDIT_IS_REPLACE_DIALOG (dialog));

	gtk_entry_set_text (GTK_ENTRY (dialog->priv->search_text_entry),
			    search_text);
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

void
gedit_replace_dialog_set_search_error (GeditReplaceDialog *dialog,
				       const gchar        *error_msg)
{
	set_error (GTK_ENTRY (dialog->priv->search_text_entry), error_msg);
}

void
gedit_replace_dialog_set_replace_error (GeditReplaceDialog *dialog,
					const gchar        *error_msg)
{
	set_error (GTK_ENTRY (dialog->priv->replace_text_entry), error_msg);
}

/* ex:set ts=8 noet: */
