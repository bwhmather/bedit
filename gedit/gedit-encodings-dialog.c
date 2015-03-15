/*
 * gedit-encodings-dialog.c
 * This file is part of gedit
 *
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gedit-encodings-dialog.h"

#include <string.h>
#include <glib/gi18n.h>
#include <gtksourceview/gtksource.h>

#include "gedit-utils.h"
#include "gedit-settings.h"

struct _GeditEncodingsDialogPrivate
{
	GSettings *enc_settings;

	/* Available encodings */
	GtkListStore *liststore_available;
	GtkTreeModelSort *sort_available;
	GtkTreeView *treeview_available;
	GtkWidget *add_button;

	/* Chosen encodings */
	GtkListStore *liststore_chosen;
	GtkTreeView *treeview_chosen;
	GtkWidget *remove_button;
	GSList *candidates_list;
};

enum
{
	COLUMN_NAME,
	COLUMN_CHARSET,
	N_COLUMNS
};

G_DEFINE_TYPE_WITH_PRIVATE (GeditEncodingsDialog, gedit_encodings_dialog, GTK_TYPE_DIALOG)

static void
gedit_encodings_dialog_finalize (GObject *object)
{
	GeditEncodingsDialogPrivate *priv = GEDIT_ENCODINGS_DIALOG (object)->priv;

	g_slist_free (priv->candidates_list);

	G_OBJECT_CLASS (gedit_encodings_dialog_parent_class)->finalize (object);
}

static void
gedit_encodings_dialog_dispose (GObject *object)
{
	GeditEncodingsDialogPrivate *priv = GEDIT_ENCODINGS_DIALOG (object)->priv;

	g_clear_object (&priv->enc_settings);

	G_OBJECT_CLASS (gedit_encodings_dialog_parent_class)->dispose (object);
}

static void
gedit_encodings_dialog_response (GtkDialog *dialog,
                                 gint       response_id)
{
	GeditEncodingsDialogPrivate *priv = GEDIT_ENCODINGS_DIALOG (dialog)->priv;

	switch (response_id)
	{
		case GTK_RESPONSE_HELP:
			gedit_app_show_help (GEDIT_APP (g_application_get_default ()),
			                     GTK_WINDOW (dialog),
			                     "gedit",
			                     NULL);
			break;

		case GTK_RESPONSE_OK:
		{
			gchar **enc_strv;

			enc_strv = _gedit_utils_encoding_list_to_strv (priv->candidates_list);
			g_settings_set_strv (priv->enc_settings,
			                     GEDIT_SETTINGS_CANDIDATE_ENCODINGS,
			                     (const gchar * const *)enc_strv);

			g_strfreev (enc_strv);
			break;
		}
	}
}

static void
gedit_encodings_dialog_class_init (GeditEncodingsDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
	GtkDialogClass *dialog_class = GTK_DIALOG_CLASS (klass);

	object_class->finalize = gedit_encodings_dialog_finalize;
	object_class->dispose = gedit_encodings_dialog_dispose;

	dialog_class->response = gedit_encodings_dialog_response;

	/* Bind class to template */
	gtk_widget_class_set_template_from_resource (widget_class,
	                                             "/org/gnome/gedit/ui/gedit-encodings-dialog.ui");
	gtk_widget_class_bind_template_child_private (widget_class, GeditEncodingsDialog, liststore_available);
	gtk_widget_class_bind_template_child_private (widget_class, GeditEncodingsDialog, liststore_chosen);
	gtk_widget_class_bind_template_child_private (widget_class, GeditEncodingsDialog, sort_available);
	gtk_widget_class_bind_template_child_private (widget_class, GeditEncodingsDialog, treeview_available);
	gtk_widget_class_bind_template_child_private (widget_class, GeditEncodingsDialog, treeview_chosen);
	gtk_widget_class_bind_template_child_private (widget_class, GeditEncodingsDialog, add_button);
	gtk_widget_class_bind_template_child_private (widget_class, GeditEncodingsDialog, remove_button);
}

static void
update_add_button_sensitivity (GeditEncodingsDialog *dialog)
{
	GtkTreeSelection *selection;
	gint count;

	selection = gtk_tree_view_get_selection (dialog->priv->treeview_available);
	count = gtk_tree_selection_count_selected_rows (selection);
	gtk_widget_set_sensitive (dialog->priv->add_button, count > 0);
}

static void
update_remove_button_sensitivity (GeditEncodingsDialog *dialog)
{
	GtkTreeSelection *selection;
	gint count;

	selection = gtk_tree_view_get_selection (dialog->priv->treeview_chosen);
	count = gtk_tree_selection_count_selected_rows (selection);
	gtk_widget_set_sensitive (dialog->priv->remove_button, count > 0);
}

static void
get_selected_encodings_func (GtkTreeModel *model,
			     GtkTreePath  *path,
			     GtkTreeIter  *iter,
			     gpointer      data)
{
	GSList **list = data;
	gchar *charset = NULL;
	const GtkSourceEncoding *enc;

	gtk_tree_model_get (model, iter,
			    COLUMN_CHARSET, &charset,
			    -1);

	enc = gtk_source_encoding_get_from_charset (charset);
	g_free (charset);

	*list = g_slist_prepend (*list, (gpointer)enc);
}

/* Returns a list of GtkSourceEncoding's. */
static GSList *
get_selected_encodings (GtkTreeView *treeview)
{
	GtkTreeSelection *selection;
	GSList *encodings = NULL;

	selection = gtk_tree_view_get_selection (treeview);

	gtk_tree_selection_selected_foreach (selection,
					     get_selected_encodings_func,
					     &encodings);

	return encodings;
}

static void
update_liststore_chosen (GeditEncodingsDialog *dialog)
{
	GSList *l;

	gtk_list_store_clear (dialog->priv->liststore_chosen);

	for (l = dialog->priv->candidates_list; l != NULL; l = l->next)
	{
		const GtkSourceEncoding *enc = l->data;
		GtkTreeIter iter;

		gtk_list_store_append (dialog->priv->liststore_chosen,
				       &iter);

		gtk_list_store_set (dialog->priv->liststore_chosen,
				    &iter,
				    COLUMN_CHARSET, gtk_source_encoding_get_charset (enc),
				    COLUMN_NAME, gtk_source_encoding_get_name (enc),
				    -1);
	}
}

static void
add_button_clicked_cb (GtkWidget            *button,
		       GeditEncodingsDialog *dialog)
{
	GSList *encodings;
	GSList *l;

	encodings = get_selected_encodings (dialog->priv->treeview_available);

	for (l = encodings; l != NULL; l = l->next)
	{
		gpointer cur_encoding = l->data;

		if (g_slist_find (dialog->priv->candidates_list, cur_encoding) == NULL)
		{
			dialog->priv->candidates_list = g_slist_prepend (dialog->priv->candidates_list,
									 cur_encoding);
		}
	}

	g_slist_free (encodings);

	update_liststore_chosen (dialog);
}

static void
remove_button_clicked_cb (GtkWidget            *button,
			  GeditEncodingsDialog *dialog)
{
	GSList *encodings;
	GSList *l;

	encodings = get_selected_encodings (dialog->priv->treeview_chosen);

	for (l = encodings; l != NULL; l = l->next)
	{
		gpointer cur_encoding = l->data;

		dialog->priv->candidates_list = g_slist_remove (dialog->priv->candidates_list,
								cur_encoding);
	}

	g_slist_free (encodings);

	update_liststore_chosen (dialog);
}

static void
init_liststore_chosen (GeditEncodingsDialog *dialog)
{
	GtkTreeIter iter;
	gchar **enc_strv;
	GSList *list;
	GSList *l;

	enc_strv = g_settings_get_strv (dialog->priv->enc_settings,
					GEDIT_SETTINGS_CANDIDATE_ENCODINGS);

	list = _gedit_utils_encoding_strv_to_list ((const gchar * const *)enc_strv);

	for (l = list; l != NULL; l = l->next)
	{
		const GtkSourceEncoding *cur_encoding = l->data;

		dialog->priv->candidates_list = g_slist_prepend (dialog->priv->candidates_list,
								 (gpointer) cur_encoding);

		gtk_list_store_append (dialog->priv->liststore_chosen, &iter);
		gtk_list_store_set (dialog->priv->liststore_chosen, &iter,
				    COLUMN_CHARSET, gtk_source_encoding_get_charset (cur_encoding),
				    COLUMN_NAME, gtk_source_encoding_get_name (cur_encoding),
				    -1);
	}

	g_slist_free (list);
}

static void
init_liststore_available (GeditEncodingsDialog *dialog)
{
	GSList *all_encodings;
	GSList *l;

	all_encodings = gtk_source_encoding_get_all ();

	for (l = all_encodings; l != NULL; l = l->next)
	{
		const GtkSourceEncoding *encoding = l->data;
		GtkTreeIter iter;

		gtk_list_store_append (dialog->priv->liststore_available, &iter);

		gtk_list_store_set (dialog->priv->liststore_available,
				    &iter,
				    COLUMN_CHARSET, gtk_source_encoding_get_charset (encoding),
				    COLUMN_NAME, gtk_source_encoding_get_name (encoding),
				    -1);
	}

	g_slist_free (all_encodings);
}

static void
gedit_encodings_dialog_init (GeditEncodingsDialog *dialog)
{
	GtkTreeSelection *selection;

	dialog->priv = gedit_encodings_dialog_get_instance_private (dialog);

	dialog->priv->enc_settings = g_settings_new ("org.gnome.gedit.preferences.encodings");

	gtk_widget_init_template (GTK_WIDGET (dialog));

	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

	g_signal_connect (dialog->priv->add_button,
			  "clicked",
			  G_CALLBACK (add_button_clicked_cb),
			  dialog);

	g_signal_connect (dialog->priv->remove_button,
			  "clicked",
			  G_CALLBACK (remove_button_clicked_cb),
			  dialog);

	/* Tree view of available encodings */

	init_liststore_available (dialog);

	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (dialog->priv->sort_available),
					      COLUMN_NAME,
					      GTK_SORT_ASCENDING);

	selection = gtk_tree_view_get_selection (dialog->priv->treeview_available);

	g_signal_connect_swapped (selection,
				  "changed",
				  G_CALLBACK (update_add_button_sensitivity),
				  dialog);

	update_add_button_sensitivity (dialog);

	/* Tree view of chosen encodings */

	init_liststore_chosen (dialog);

	selection = gtk_tree_view_get_selection (dialog->priv->treeview_chosen);

	g_signal_connect_swapped (selection,
				  "changed",
				  G_CALLBACK (update_remove_button_sensitivity),
				  dialog);

	update_remove_button_sensitivity (dialog);
}

GtkWidget *
gedit_encodings_dialog_new (void)
{
	return g_object_new (GEDIT_TYPE_ENCODINGS_DIALOG, NULL);
}

/* ex:set ts=8 noet: */
