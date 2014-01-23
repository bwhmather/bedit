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

/*
 * Modified by the gedit Team, 2002-2005. See the AUTHORS file for a
 * list of people on the gedit Team.
 * See the ChangeLog files for a list of changes.
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "gedit-encodings-dialog.h"
#include "gedit-encodings.h"
#include "gedit-utils.h"
#include "gedit-debug.h"
#include "gedit-dirs.h"
#include "gedit-settings.h"

struct _GeditEncodingsDialogPrivate
{
	GSettings	*enc_settings;

	GtkListStore	*available_liststore;
	GtkTreeModel	*sort_available;
	GtkListStore	*displayed_liststore;
	GtkTreeModel	*sort_displayed;
	GtkWidget	*available_treeview;
	GtkWidget	*displayed_treeview;
	GtkWidget	*add_button;
	GtkWidget	*remove_button;

	GSList		*show_in_menu_list;
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
			gchar **encs;

			encs = _gedit_encoding_list_to_strv (priv->show_in_menu_list);
			g_settings_set_strv (priv->enc_settings,
			                     GEDIT_SETTINGS_ENCODING_SHOWN_IN_MENU,
			                     (const gchar * const *)encs);

			g_strfreev (encs);
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
	gtk_widget_class_bind_template_child_private (widget_class, GeditEncodingsDialog, add_button);
	gtk_widget_class_bind_template_child_private (widget_class, GeditEncodingsDialog, remove_button);
	gtk_widget_class_bind_template_child_private (widget_class, GeditEncodingsDialog, available_treeview);
	gtk_widget_class_bind_template_child_private (widget_class, GeditEncodingsDialog, available_liststore);
	gtk_widget_class_bind_template_child_private (widget_class, GeditEncodingsDialog, sort_available);
	gtk_widget_class_bind_template_child_private (widget_class, GeditEncodingsDialog, displayed_treeview);
	gtk_widget_class_bind_template_child_private (widget_class, GeditEncodingsDialog, displayed_liststore);
	gtk_widget_class_bind_template_child_private (widget_class, GeditEncodingsDialog, sort_displayed);
}

enum {
	COLUMN_NAME,
	COLUMN_CHARSET,
	N_COLUMNS
};

static void
count_selected_items_func (GtkTreeModel *model,
			   GtkTreePath  *path,
			   GtkTreeIter  *iter,
			   gpointer      data)
{
	int *count = data;

	*count += 1;
}

static void
available_selection_changed_callback (GtkTreeSelection     *selection,
				      GeditEncodingsDialog *dialogs)
{
	int count;

	count = 0;
	gtk_tree_selection_selected_foreach (selection,
					     count_selected_items_func,
					     &count);

	gtk_widget_set_sensitive (dialogs->priv->add_button, count > 0);
}

static void
displayed_selection_changed_callback (GtkTreeSelection     *selection,
				      GeditEncodingsDialog *dialogs)
{
	int count;

	count = 0;
	gtk_tree_selection_selected_foreach (selection,
					     count_selected_items_func,
					     &count);

	gtk_widget_set_sensitive (dialogs->priv->remove_button, count > 0);
}

static void
get_selected_encodings_func (GtkTreeModel *model,
			     GtkTreePath  *path,
			     GtkTreeIter  *iter,
			     gpointer      data)
{
	GSList **list = data;
	gchar *charset;
	const GeditEncoding *enc;

	charset = NULL;
	gtk_tree_model_get (model, iter, COLUMN_CHARSET, &charset, -1);

	enc = gedit_encoding_get_from_charset (charset);
	g_free (charset);

	*list = g_slist_prepend (*list, (gpointer)enc);
}

static void
update_shown_in_menu_tree_model (GtkListStore *store,
				 GSList       *list)
{
	GtkTreeIter iter;

	gtk_list_store_clear (store);

	while (list != NULL)
	{
		const GeditEncoding *enc;

		enc = (const GeditEncoding*) list->data;

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
				    COLUMN_CHARSET,
				    gedit_encoding_get_charset (enc),
				    COLUMN_NAME,
				    gedit_encoding_get_name (enc), -1);

		list = g_slist_next (list);
	}
}

static void
add_button_clicked_callback (GtkWidget            *button,
			     GeditEncodingsDialog *dialog)
{
	GtkTreeSelection *selection;
	GSList *encodings;
	GSList *tmp;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->priv->available_treeview));

	encodings = NULL;
	gtk_tree_selection_selected_foreach (selection,
					     get_selected_encodings_func,
					     &encodings);

	tmp = encodings;
	while (tmp != NULL)
	{
		if (g_slist_find (dialog->priv->show_in_menu_list, tmp->data) == NULL)
		{
			dialog->priv->show_in_menu_list = g_slist_prepend (dialog->priv->show_in_menu_list,
									   tmp->data);
		}

		tmp = g_slist_next (tmp);
	}

	g_slist_free (encodings);

	update_shown_in_menu_tree_model (GTK_LIST_STORE (dialog->priv->displayed_liststore),
					 dialog->priv->show_in_menu_list);
}

static void
remove_button_clicked_callback (GtkWidget            *button,
				GeditEncodingsDialog *dialog)
{
	GtkTreeSelection *selection;
	GSList *encodings;
	GSList *tmp;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->priv->displayed_treeview));

	encodings = NULL;
	gtk_tree_selection_selected_foreach (selection,
					     get_selected_encodings_func,
					     &encodings);

	tmp = encodings;
	while (tmp != NULL)
	{
		dialog->priv->show_in_menu_list = g_slist_remove (dialog->priv->show_in_menu_list,
								  tmp->data);

		tmp = g_slist_next (tmp);
	}

	g_slist_free (encodings);

	update_shown_in_menu_tree_model (GTK_LIST_STORE (dialog->priv->displayed_liststore),
					 dialog->priv->show_in_menu_list);
}

static void
init_shown_in_menu_tree_model (GeditEncodingsDialog *dialog)
{
	GtkTreeIter iter;
	gchar **enc_strv;
	GSList *list, *tmp;

	/* add data to the list store */
	enc_strv = g_settings_get_strv (dialog->priv->enc_settings,
					GEDIT_SETTINGS_ENCODING_SHOWN_IN_MENU);

	list = _gedit_encoding_strv_to_list ((const gchar * const *)enc_strv);

	for (tmp = list; tmp != NULL; tmp = g_slist_next (tmp))
	{
		const GeditEncoding *enc;

		enc = (const GeditEncoding *) tmp->data;

		dialog->priv->show_in_menu_list = g_slist_prepend (dialog->priv->show_in_menu_list,
								   tmp->data);

		gtk_list_store_append (dialog->priv->displayed_liststore,
				       &iter);
		gtk_list_store_set (dialog->priv->displayed_liststore,
				    &iter,
				    COLUMN_CHARSET,
				    gedit_encoding_get_charset (enc),
				    COLUMN_NAME,
				    gedit_encoding_get_name (enc), -1);
	}

	g_slist_free (list);
}

static void
gedit_encodings_dialog_init (GeditEncodingsDialog *dlg)
{
	GtkTreeIter parent_iter;
	GtkTreeSelection *selection;
	const GeditEncoding *enc;
	int i;

	dlg->priv = gedit_encodings_dialog_get_instance_private (dlg);

	dlg->priv->enc_settings = g_settings_new ("org.gnome.gedit.preferences.encodings");

	gtk_widget_init_template (GTK_WIDGET (dlg));

	gtk_dialog_set_default_response (GTK_DIALOG (dlg),
					 GTK_RESPONSE_OK);

	g_signal_connect (dlg->priv->add_button,
			  "clicked",
			  G_CALLBACK (add_button_clicked_callback),
			  dlg);
	g_signal_connect (dlg->priv->remove_button,
			  "clicked",
			  G_CALLBACK (remove_button_clicked_callback),
			  dlg);

	/* Tree view of available encodings */

	/* Add the data */
	i = 0;
	while ((enc = gedit_encoding_get_from_index (i)) != NULL)
	{
		gtk_list_store_append (dlg->priv->available_liststore,
				       &parent_iter);
		gtk_list_store_set (dlg->priv->available_liststore,
				    &parent_iter,
				    COLUMN_CHARSET,
				    gedit_encoding_get_charset (enc),
				    COLUMN_NAME,
				    gedit_encoding_get_name (enc), -1);

		++i;
	}

	/* Sort model */
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (dlg->priv->sort_available),
					      COLUMN_NAME,
					      GTK_SORT_ASCENDING);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dlg->priv->available_treeview));

	available_selection_changed_callback (selection, dlg);
	g_signal_connect (selection,
			  "changed",
			  G_CALLBACK (available_selection_changed_callback),
			  dlg);

	/* Tree view of selected encodings */

	/* Add the data */
	init_shown_in_menu_tree_model (dlg);

	/* Sort model */
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (dlg->priv->sort_displayed),
					      COLUMN_NAME,
					      GTK_SORT_ASCENDING);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dlg->priv->displayed_treeview));

	displayed_selection_changed_callback (selection, dlg);
	g_signal_connect (selection,
			  "changed",
			  G_CALLBACK (displayed_selection_changed_callback),
			  dlg);
}

GtkWidget *
gedit_encodings_dialog_new (void)
{
	return GTK_WIDGET (g_object_new (GEDIT_TYPE_ENCODINGS_DIALOG, NULL));
}

/* ex:set ts=8 noet: */
