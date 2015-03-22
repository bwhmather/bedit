/*
 * gedit-encodings-dialog.c
 * This file is part of gedit
 *
 * Copyright (C) 2002-2005 Paolo Maggi
 * Copyright (C) 2015 Sébastien Wilmet
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
	GtkWidget *up_button;
	GtkWidget *down_button;
};

enum
{
	COLUMN_NAME,
	COLUMN_CHARSET,
	COLUMN_ENCODING,
	N_COLUMNS
};

G_DEFINE_TYPE_WITH_PRIVATE (GeditEncodingsDialog, gedit_encodings_dialog, GTK_TYPE_DIALOG)

static void
gedit_encodings_dialog_dispose (GObject *object)
{
	GeditEncodingsDialogPrivate *priv = GEDIT_ENCODINGS_DIALOG (object)->priv;

	g_clear_object (&priv->enc_settings);

	G_OBJECT_CLASS (gedit_encodings_dialog_parent_class)->dispose (object);
}

static GSList *
get_chosen_encodings_list (GeditEncodingsDialog *dialog)
{
	GtkTreeModel *model = GTK_TREE_MODEL (dialog->priv->liststore_chosen);
	GtkTreeIter iter;
	gboolean iter_set;
	GSList *ret = NULL;

	iter_set = gtk_tree_model_get_iter_first (model, &iter);

	while (iter_set)
	{
		const GtkSourceEncoding *encoding = NULL;

		gtk_tree_model_get (model, &iter,
				    COLUMN_ENCODING, &encoding,
				    -1);

		ret = g_slist_prepend (ret, (gpointer)encoding);

		iter_set = gtk_tree_model_iter_next (model, &iter);
	}

	return ret;
}

static void
gedit_encodings_dialog_response (GtkDialog *gtk_dialog,
                                 gint       response_id)
{
	GeditEncodingsDialog *dialog = GEDIT_ENCODINGS_DIALOG (gtk_dialog);

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
			GSList *enc_list;
			gchar **enc_strv;

			enc_list = get_chosen_encodings_list (dialog);
			enc_strv = _gedit_utils_encoding_list_to_strv (enc_list);

			g_settings_set_strv (dialog->priv->enc_settings,
			                     GEDIT_SETTINGS_CANDIDATE_ENCODINGS,
			                     (const gchar * const *)enc_strv);

			g_slist_free (enc_list);
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
	gtk_widget_class_bind_template_child_private (widget_class, GeditEncodingsDialog, up_button);
	gtk_widget_class_bind_template_child_private (widget_class, GeditEncodingsDialog, down_button);
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
update_chosen_buttons_sensitivity (GeditEncodingsDialog *dialog)
{
	GtkTreeSelection *selection;
	gint count;
	GList *selected_rows;
	GtkTreeModel *model;
	GtkTreePath *path;
	gint *indices;
	gint depth;
	gint items_count;
	gboolean first_item_selected;
	gboolean last_item_selected;

	selection = gtk_tree_view_get_selection (dialog->priv->treeview_chosen);
	count = gtk_tree_selection_count_selected_rows (selection);
	gtk_widget_set_sensitive (dialog->priv->remove_button, count > 0);

	if (count != 1)
	{
		gtk_widget_set_sensitive (dialog->priv->up_button, FALSE);
		gtk_widget_set_sensitive (dialog->priv->down_button, FALSE);
		return;
	}

	selected_rows = gtk_tree_selection_get_selected_rows (selection, &model);
	g_assert (g_list_length (selected_rows) == 1);

	path = selected_rows->data;
	indices = gtk_tree_path_get_indices_with_depth (path, &depth);
	g_assert (depth == 1);

	items_count = gtk_tree_model_iter_n_children (model, NULL);

	first_item_selected = indices[0] == 0;
	last_item_selected = indices[0] == (items_count - 1);

	gtk_widget_set_sensitive (dialog->priv->up_button, !first_item_selected);
	gtk_widget_set_sensitive (dialog->priv->down_button, !last_item_selected);

	g_list_free_full (selected_rows, (GDestroyNotify) gtk_tree_path_free);
}

static void
append_encoding (GtkListStore            *liststore,
		 const GtkSourceEncoding *encoding)
{
	GtkTreeIter iter;

	gtk_list_store_append (liststore, &iter);
	gtk_list_store_set (liststore, &iter,
			    COLUMN_NAME, gtk_source_encoding_get_name (encoding),
			    COLUMN_CHARSET, gtk_source_encoding_get_charset (encoding),
			    COLUMN_ENCODING, encoding,
			    -1);
}

/* Removes all @paths from @orig, and append them at the end of @dest in the
 * same order.
 */
static void
transfer_encodings (GList        *paths,
		    GtkListStore *orig,
		    GtkListStore *dest)
{
	GtkTreeModel *model_orig = GTK_TREE_MODEL (orig);
	GList *refs = NULL;
	GList *l;

	for (l = paths; l != NULL; l = l->next)
	{
		GtkTreePath *path = l->data;
		refs = g_list_prepend (refs, gtk_tree_row_reference_new (model_orig, path));
	}

	refs = g_list_reverse (refs);

	for (l = refs; l != NULL; l = l->next)
	{
		GtkTreeRowReference *ref = l->data;
		GtkTreePath *path;
		GtkTreeIter iter;
		const GtkSourceEncoding *encoding = NULL;

		path = gtk_tree_row_reference_get_path (ref);

		if (!gtk_tree_model_get_iter (model_orig, &iter, path))
		{
			gtk_tree_path_free (path);
			g_warning ("Remove encoding: invalid path");
			continue;
		}

		/* Transfer encoding */
		gtk_tree_model_get (model_orig, &iter,
				    COLUMN_ENCODING, &encoding,
				    -1);

		append_encoding (dest, encoding);

		gtk_list_store_remove (orig, &iter);

		gtk_tree_path_free (path);
	}

	g_list_free_full (refs, (GDestroyNotify) gtk_tree_row_reference_free);
}

static void
add_button_clicked_cb (GtkWidget            *button,
		       GeditEncodingsDialog *dialog)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GList *filter_paths;
	GList *children_paths = NULL;
	GList *l;

	selection = gtk_tree_view_get_selection (dialog->priv->treeview_available);
	filter_paths = gtk_tree_selection_get_selected_rows (selection, &model);

	g_return_if_fail (model == GTK_TREE_MODEL (dialog->priv->sort_available));

	for (l = filter_paths; l != NULL; l = l->next)
	{
		GtkTreePath *filter_path = l->data;
		GtkTreePath *child_path;

		child_path = gtk_tree_model_sort_convert_path_to_child_path (dialog->priv->sort_available,
									     filter_path);

		children_paths = g_list_prepend (children_paths, child_path);
	}

	children_paths = g_list_reverse (children_paths);

	transfer_encodings (children_paths,
			    dialog->priv->liststore_available,
			    dialog->priv->liststore_chosen);

	/* For the treeview_available, it's more natural to unselect the added
	 * encodings.
	 * Note that when removing encodings from treeview_chosen, it is
	 * desirable to keep a selection, so we can remove several elements in a
	 * row (if the user doesn't know that several elements can be selected
	 * at once with Ctrl or Shift).
	 */
	gtk_tree_selection_unselect_all (selection);

	g_list_free_full (filter_paths, (GDestroyNotify) gtk_tree_path_free);
	g_list_free_full (children_paths, (GDestroyNotify) gtk_tree_path_free);
}

static void
remove_button_clicked_cb (GtkWidget            *button,
			  GeditEncodingsDialog *dialog)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GList *paths;

	selection = gtk_tree_view_get_selection (dialog->priv->treeview_chosen);
	paths = gtk_tree_selection_get_selected_rows (selection, &model);

	g_return_if_fail (model == GTK_TREE_MODEL (dialog->priv->liststore_chosen));

	transfer_encodings (paths,
			    dialog->priv->liststore_chosen,
			    dialog->priv->liststore_available);

	g_list_free_full (paths, (GDestroyNotify) gtk_tree_path_free);
}

static void
up_button_clicked_cb (GtkWidget            *button,
		      GeditEncodingsDialog *dialog)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GList *selected_rows;
	GtkTreePath *path;
	GtkTreeIter iter;
	GtkTreeIter prev_iter;

	selection = gtk_tree_view_get_selection (dialog->priv->treeview_chosen);
	selected_rows = gtk_tree_selection_get_selected_rows (selection, &model);

	g_return_if_fail (model == GTK_TREE_MODEL (dialog->priv->liststore_chosen));
	g_return_if_fail (g_list_length (selected_rows) == 1);

	path = selected_rows->data;
	if (!gtk_tree_model_get_iter (model, &iter, path))
	{
		g_return_if_reached ();
	}

	prev_iter = iter;
	if (!gtk_tree_model_iter_previous (model, &prev_iter))
	{
		g_return_if_reached ();
	}

	gtk_list_store_move_before (dialog->priv->liststore_chosen,
				    &iter,
				    &prev_iter);

	update_chosen_buttons_sensitivity (dialog);

	g_list_free_full (selected_rows, (GDestroyNotify) gtk_tree_path_free);
}

static void
down_button_clicked_cb (GtkWidget            *button,
			GeditEncodingsDialog *dialog)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GList *selected_rows;
	GtkTreePath *path;
	GtkTreeIter iter;
	GtkTreeIter next_iter;

	selection = gtk_tree_view_get_selection (dialog->priv->treeview_chosen);
	selected_rows = gtk_tree_selection_get_selected_rows (selection, &model);

	g_return_if_fail (model == GTK_TREE_MODEL (dialog->priv->liststore_chosen));
	g_return_if_fail (g_list_length (selected_rows) == 1);

	path = selected_rows->data;
	if (!gtk_tree_model_get_iter (model, &iter, path))
	{
		g_return_if_reached ();
	}

	next_iter = iter;
	if (!gtk_tree_model_iter_next (model, &next_iter))
	{
		g_return_if_reached ();
	}

	gtk_list_store_move_after (dialog->priv->liststore_chosen,
				   &iter,
				   &next_iter);

	update_chosen_buttons_sensitivity (dialog);

	g_list_free_full (selected_rows, (GDestroyNotify) gtk_tree_path_free);
}

static void
init_liststores (GeditEncodingsDialog *dialog)
{
	gchar **enc_strv;
	GSList *chosen_encodings;
	GSList *all_encodings;
	GSList *l;

	/* Chosen encodings */

	enc_strv = g_settings_get_strv (dialog->priv->enc_settings,
					GEDIT_SETTINGS_CANDIDATE_ENCODINGS);

	chosen_encodings = _gedit_utils_encoding_strv_to_list ((const gchar * const *)enc_strv);

	for (l = chosen_encodings; l != NULL; l = l->next)
	{
		const GtkSourceEncoding *cur_encoding = l->data;
		append_encoding (dialog->priv->liststore_chosen, cur_encoding);
	}

	/* Available encodings */

	all_encodings = gtk_source_encoding_get_all ();

	for (l = chosen_encodings; l != NULL; l = l->next)
	{
		const GtkSourceEncoding *chosen_encoding = l->data;
		all_encodings = g_slist_remove (all_encodings, chosen_encoding);
	}

	for (l = all_encodings; l != NULL; l = l->next)
	{
		const GtkSourceEncoding *cur_encoding = l->data;
		append_encoding (dialog->priv->liststore_available, cur_encoding);
	}

	g_strfreev (enc_strv);
	g_slist_free (chosen_encodings);
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

	init_liststores (dialog);

	/* Tree view of available encodings */

	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (dialog->priv->sort_available),
					      COLUMN_NAME,
					      GTK_SORT_ASCENDING);

	selection = gtk_tree_view_get_selection (dialog->priv->treeview_available);

	g_signal_connect_swapped (selection,
				  "changed",
				  G_CALLBACK (update_add_button_sensitivity),
				  dialog);

	update_add_button_sensitivity (dialog);

	g_signal_connect (dialog->priv->add_button,
			  "clicked",
			  G_CALLBACK (add_button_clicked_cb),
			  dialog);

	/* Tree view of chosen encodings */

	selection = gtk_tree_view_get_selection (dialog->priv->treeview_chosen);

	g_signal_connect_swapped (selection,
				  "changed",
				  G_CALLBACK (update_chosen_buttons_sensitivity),
				  dialog);

	update_chosen_buttons_sensitivity (dialog);

	g_signal_connect (dialog->priv->remove_button,
			  "clicked",
			  G_CALLBACK (remove_button_clicked_cb),
			  dialog);

	g_signal_connect (dialog->priv->up_button,
			  "clicked",
			  G_CALLBACK (up_button_clicked_cb),
			  dialog);

	g_signal_connect (dialog->priv->down_button,
			  "clicked",
			  G_CALLBACK (down_button_clicked_cb),
			  dialog);
}

GtkWidget *
gedit_encodings_dialog_new (void)
{
	return g_object_new (GEDIT_TYPE_ENCODINGS_DIALOG, NULL);
}

/* ex:set ts=8 noet: */
