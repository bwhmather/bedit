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

	GtkListStore *liststore_available;
	GtkListStore *liststore_displayed;
	GtkTreeModelSort *sort_available;
	GtkTreeModelSort *sort_displayed;
	GtkTreeView *treeview_available;
	GtkTreeView *treeview_displayed;
	GtkWidget *add_button;
	GtkWidget *remove_button;

	GSList *show_in_menu_list;
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

	g_slist_free (priv->show_in_menu_list);

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

			enc_strv = _gedit_utils_encoding_list_to_strv (priv->show_in_menu_list);
			g_settings_set_strv (priv->enc_settings,
			                     GEDIT_SETTINGS_ENCODING_SHOWN_IN_MENU,
			                     (const gchar * const *)enc_strv);

			g_strfreev (enc_strv);
		}
		default:
			gtk_widget_destroy (GTK_WIDGET (dialog));
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
	gtk_widget_class_bind_template_child_private (widget_class, GeditEncodingsDialog, liststore_displayed);
	gtk_widget_class_bind_template_child_private (widget_class, GeditEncodingsDialog, sort_available);
	gtk_widget_class_bind_template_child_private (widget_class, GeditEncodingsDialog, sort_displayed);
	gtk_widget_class_bind_template_child_private (widget_class, GeditEncodingsDialog, treeview_available);
	gtk_widget_class_bind_template_child_private (widget_class, GeditEncodingsDialog, treeview_displayed);
	gtk_widget_class_bind_template_child_private (widget_class, GeditEncodingsDialog, add_button);
	gtk_widget_class_bind_template_child_private (widget_class, GeditEncodingsDialog, remove_button);
}

static void
available_selection_changed_cb (GtkTreeSelection     *selection,
				GeditEncodingsDialog *dialogs)
{
	gint count;

	count = gtk_tree_selection_count_selected_rows (selection);

	gtk_widget_set_sensitive (dialogs->priv->add_button, count > 0);
}

static void
displayed_selection_changed_cb (GtkTreeSelection     *selection,
				GeditEncodingsDialog *dialogs)
{
	gint count;

	count = gtk_tree_selection_count_selected_rows (selection);

	gtk_widget_set_sensitive (dialogs->priv->remove_button, count > 0);
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

static void
update_shown_in_menu_tree_model (GtkListStore *store,
				 GSList       *list)
{
	GtkTreeIter iter;

	gtk_list_store_clear (store);

	for (; list != NULL; list = list->next)
	{
		const GtkSourceEncoding *enc = list->data;

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
				    COLUMN_CHARSET, gtk_source_encoding_get_charset (enc),
				    COLUMN_NAME, gtk_source_encoding_get_name (enc),
				    -1);
	}
}

static void
add_button_clicked_cb (GtkWidget            *button,
		       GeditEncodingsDialog *dialog)
{
	GtkTreeSelection *selection;
	GSList *encodings;
	GSList *l;

	selection = gtk_tree_view_get_selection (dialog->priv->treeview_available);

	encodings = NULL;
	gtk_tree_selection_selected_foreach (selection,
					     get_selected_encodings_func,
					     &encodings);

	for (l = encodings; l != NULL; l = l->next)
	{
		gpointer cur_encoding = l->data;

		if (g_slist_find (dialog->priv->show_in_menu_list, cur_encoding) == NULL)
		{
			dialog->priv->show_in_menu_list = g_slist_prepend (dialog->priv->show_in_menu_list,
									   cur_encoding);
		}
	}

	g_slist_free (encodings);

	update_shown_in_menu_tree_model (dialog->priv->liststore_displayed,
					 dialog->priv->show_in_menu_list);
}

static void
remove_button_clicked_cb (GtkWidget            *button,
			  GeditEncodingsDialog *dialog)
{
	GtkTreeSelection *selection;
	GSList *encodings;
	GSList *l;

	selection = gtk_tree_view_get_selection (dialog->priv->treeview_displayed);

	encodings = NULL;
	gtk_tree_selection_selected_foreach (selection,
					     get_selected_encodings_func,
					     &encodings);

	for (l = encodings; l != NULL; l = l->next)
	{
		gpointer cur_encoding = l->data;

		dialog->priv->show_in_menu_list = g_slist_remove (dialog->priv->show_in_menu_list,
								  cur_encoding);
	}

	g_slist_free (encodings);

	update_shown_in_menu_tree_model (dialog->priv->liststore_displayed,
					 dialog->priv->show_in_menu_list);
}

static void
init_shown_in_menu_tree_model (GeditEncodingsDialog *dialog)
{
	GtkTreeIter iter;
	gchar **enc_strv;
	GSList *list;
	GSList *l;

	/* add data to the list store */
	enc_strv = g_settings_get_strv (dialog->priv->enc_settings,
					GEDIT_SETTINGS_ENCODING_SHOWN_IN_MENU);

	list = _gedit_utils_encoding_strv_to_list ((const gchar * const *)enc_strv);

	for (l = list; l != NULL; l = l->next)
	{
		const GtkSourceEncoding *cur_encoding = l->data;

		dialog->priv->show_in_menu_list = g_slist_prepend (dialog->priv->show_in_menu_list,
								   (gpointer) cur_encoding);

		gtk_list_store_append (dialog->priv->liststore_displayed, &iter);
		gtk_list_store_set (dialog->priv->liststore_displayed, &iter,
				    COLUMN_CHARSET, gtk_source_encoding_get_charset (cur_encoding),
				    COLUMN_NAME, gtk_source_encoding_get_name (cur_encoding),
				    -1);
	}

	g_slist_free (list);
}

static void
insert_encoding_foreach_cb (const GtkSourceEncoding *encoding,
			    GeditEncodingsDialog    *dlg)
{
	GtkTreeIter iter;

	if (encoding == gtk_source_encoding_get_utf8 ())
	{
		/* The UTF-8 encoding is always added to the combobox. */
		return;
	}

	gtk_list_store_append (dlg->priv->liststore_available, &iter);

	gtk_list_store_set (dlg->priv->liststore_available,
			    &iter,
			    COLUMN_CHARSET, gtk_source_encoding_get_charset (encoding),
			    COLUMN_NAME, gtk_source_encoding_get_name (encoding),
			    -1);
}

static void
gedit_encodings_dialog_init (GeditEncodingsDialog *dlg)
{
	GtkTreeSelection *selection;

	dlg->priv = gedit_encodings_dialog_get_instance_private (dlg);

	dlg->priv->enc_settings = g_settings_new ("org.gnome.gedit.preferences.encodings");

	gtk_widget_init_template (GTK_WIDGET (dlg));

	gtk_dialog_set_default_response (GTK_DIALOG (dlg), GTK_RESPONSE_OK);

	g_signal_connect (dlg->priv->add_button,
			  "clicked",
			  G_CALLBACK (add_button_clicked_cb),
			  dlg);

	g_signal_connect (dlg->priv->remove_button,
			  "clicked",
			  G_CALLBACK (remove_button_clicked_cb),
			  dlg);

	/* Tree view of available encodings */

	/* Add the data */
	gtk_source_encoding_foreach ((GtkSourceEncodingForeachFunc) insert_encoding_foreach_cb,
				     dlg);

	/* Sort model */
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (dlg->priv->sort_available),
					      COLUMN_NAME,
					      GTK_SORT_ASCENDING);

	selection = gtk_tree_view_get_selection (dlg->priv->treeview_available);

	available_selection_changed_cb (selection, dlg);
	g_signal_connect (selection,
			  "changed",
			  G_CALLBACK (available_selection_changed_cb),
			  dlg);

	/* Tree view of selected encodings */

	/* Add the data */
	init_shown_in_menu_tree_model (dlg);

	/* Sort model */
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (dlg->priv->sort_displayed),
					      COLUMN_NAME,
					      GTK_SORT_ASCENDING);

	selection = gtk_tree_view_get_selection (dlg->priv->treeview_displayed);

	displayed_selection_changed_cb (selection, dlg);
	g_signal_connect (selection,
			  "changed",
			  G_CALLBACK (displayed_selection_changed_cb),
			  dlg);
}

GtkWidget *
gedit_encodings_dialog_new (void)
{
	return g_object_new (GEDIT_TYPE_ENCODINGS_DIALOG, NULL);
}

/* ex:set ts=8 noet: */
