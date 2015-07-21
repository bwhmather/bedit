/*
 * gedit-spell-language-dialog.c
 * This file is part of gedit
 *
 * Copyright (C) 2002 Paolo Maggi
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

#include "gedit-spell-language-dialog.h"
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gedit/gedit-utils.h>
#include <gedit/gedit-app.h>
#include "gedit-spell-checker-language.h"

enum
{
	COLUMN_LANGUAGE_NAME = 0,
	COLUMN_LANGUAGE_POINTER,
	ENCODING_NUM_COLS
};

struct _GeditSpellLanguageDialog
{
	GtkDialog dialog;

	GtkWidget *treeview;
	GtkTreeModel *model;
};

G_DEFINE_TYPE (GeditSpellLanguageDialog, gedit_spell_language_dialog, GTK_TYPE_DIALOG)

static void
gedit_spell_language_dialog_response (GtkDialog *dialog,
				      gint       response_id)
{
	if (response_id == GTK_RESPONSE_HELP)
	{
		gedit_app_show_help (GEDIT_APP (g_application_get_default ()),
				     GTK_WINDOW (dialog),
				     NULL,
				     "gedit-spellcheck");
	}
	else if (GTK_DIALOG_CLASS (gedit_spell_language_dialog_parent_class)->response != NULL)
	{
		GTK_DIALOG_CLASS (gedit_spell_language_dialog_parent_class)->response (dialog, response_id);
	}
}

static void
gedit_spell_language_dialog_class_init (GeditSpellLanguageDialogClass *klass)
{
	GtkDialogClass *dialog_class = GTK_DIALOG_CLASS (klass);

	dialog_class->response = gedit_spell_language_dialog_response;
}

static void
scroll_to_selected (GtkTreeView *tree_view)
{
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	GtkTreeIter iter;

	model = gtk_tree_view_get_model (tree_view);
	g_return_if_fail (model != NULL);

	/* Scroll to selected */
	selection = gtk_tree_view_get_selection (tree_view);
	g_return_if_fail (selection != NULL);

	if (gtk_tree_selection_get_selected (selection, NULL, &iter))
	{
		GtkTreePath* path;

		path = gtk_tree_model_get_path (model, &iter);
		g_return_if_fail (path != NULL);

		gtk_tree_view_scroll_to_cell (tree_view,
					      path, NULL, TRUE, 1.0, 0.0);
		gtk_tree_path_free (path);
	}
}

static void
language_row_activated (GtkTreeView *tree_view,
			GtkTreePath *path,
			GtkTreeViewColumn *column,
			GeditSpellLanguageDialog *dialog)
{
	gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
}

static void
create_dialog (GeditSpellLanguageDialog *dialog)
{
	GtkBuilder *builder;
	GtkWidget *content;
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;
	gchar *root_objects[] = {
		"content",
		NULL
	};

	gtk_dialog_add_buttons (GTK_DIALOG (dialog),
				_("_Cancel"), GTK_RESPONSE_CANCEL,
				_("_OK"), GTK_RESPONSE_OK,
				_("_Help"), GTK_RESPONSE_HELP,
				NULL);

	gtk_window_set_title (GTK_WINDOW (dialog), _("Set language"));
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);

	/* HIG defaults */
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
	gtk_box_set_spacing (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))),
			     2); /* 2 * 5 + 2 = 12 */
	gtk_container_set_border_width (GTK_CONTAINER (gtk_dialog_get_action_area (GTK_DIALOG (dialog))),
					5);
	gtk_box_set_spacing (GTK_BOX (gtk_dialog_get_action_area (GTK_DIALOG (dialog))),
			     6);

	builder = gtk_builder_new ();
	gtk_builder_add_objects_from_resource (builder, "/org/gnome/gedit/plugins/spell/ui/languages-dialog.ui",
	                                       root_objects, NULL);
	content = GTK_WIDGET (gtk_builder_get_object (builder, "content"));
	g_object_ref (content);
	dialog->treeview = GTK_WIDGET (gtk_builder_get_object (builder, "languages_treeview"));
	g_object_unref (builder);

	gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))),
			    content, TRUE, TRUE, 0);
	g_object_unref (content);
	gtk_container_set_border_width (GTK_CONTAINER (content), 5);

	dialog->model = GTK_TREE_MODEL (gtk_list_store_new (ENCODING_NUM_COLS,
							    G_TYPE_STRING,
							    G_TYPE_POINTER));

	gtk_tree_view_set_model (GTK_TREE_VIEW (dialog->treeview),
				 dialog->model);

	g_object_unref (dialog->model);

	/* Add the encoding column */
	cell = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Languages"),
							   cell,
							   "text",
							   COLUMN_LANGUAGE_NAME,
							   NULL);

	gtk_tree_view_append_column (GTK_TREE_VIEW (dialog->treeview),
				     column);

	gtk_tree_view_set_search_column (GTK_TREE_VIEW (dialog->treeview),
					 COLUMN_LANGUAGE_NAME);

	g_signal_connect (dialog->treeview,
			  "realize",
			  G_CALLBACK (scroll_to_selected),
			  dialog);
	g_signal_connect (dialog->treeview,
			  "row-activated",
			  G_CALLBACK (language_row_activated),
			  dialog);
}

static void
gedit_spell_language_dialog_init (GeditSpellLanguageDialog *dialog)
{

}

static void
populate_language_list (GeditSpellLanguageDialog        *dialog,
			const GeditSpellCheckerLanguage *cur_lang)
{
	GtkListStore *store;
	GtkTreeIter iter;

	const GSList* langs;

	/* create list store */
	store = GTK_LIST_STORE (dialog->model);

	langs = gedit_spell_checker_get_available_languages ();

	while (langs)
	{
		const gchar *name;

		name = gedit_spell_checker_language_to_string ((const GeditSpellCheckerLanguage*)langs->data);

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
				    COLUMN_LANGUAGE_NAME, name,
				    COLUMN_LANGUAGE_POINTER, langs->data,
				    -1);

		if (langs->data == cur_lang)
		{
			GtkTreeSelection *selection;

			selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->treeview));
			g_return_if_fail (selection != NULL);

			gtk_tree_selection_select_iter (selection, &iter);
		}

		langs = g_slist_next (langs);
	}
}

GtkWidget *
gedit_spell_language_dialog_new (GtkWindow                       *parent,
				 const GeditSpellCheckerLanguage *cur_lang)
{
	GeditSpellLanguageDialog *dialog;

	g_return_val_if_fail (GTK_IS_WINDOW (parent), NULL);

	dialog = g_object_new (GEDIT_TYPE_SPELL_LANGUAGE_DIALOG, NULL);

	create_dialog (dialog);

	populate_language_list (dialog, cur_lang);

	gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);
	gtk_widget_grab_focus (dialog->treeview);

	return GTK_WIDGET (dialog);
}

const GeditSpellCheckerLanguage *
gedit_spell_language_get_selected_language (GeditSpellLanguageDialog *dialog)
{
	GValue value = {0, };
	const GeditSpellCheckerLanguage* lang;

	GtkTreeIter iter;
	GtkTreeSelection *selection;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->treeview));
	g_return_val_if_fail (selection != NULL, NULL);

	if (!gtk_tree_selection_get_selected (selection, NULL, &iter))
		return NULL;

	gtk_tree_model_get_value (dialog->model,
				  &iter,
				  COLUMN_LANGUAGE_POINTER,
				  &value);

	lang = (const GeditSpellCheckerLanguage* ) g_value_get_pointer (&value);

	return lang;
}

/* ex:set ts=8 noet: */
