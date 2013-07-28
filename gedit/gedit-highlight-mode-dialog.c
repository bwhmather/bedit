/*
 * gedit-highlight-mode-dialog.c
 * This file is part of gedit
 *
 * Copyright (C) 2013 - Ignacio Casal Quinteiro
 *
 * gedit is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * gedit is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with gedit. If not, see <http://www.gnu.org/licenses/>.
 */


#include "gedit-highlight-mode-dialog.h"

#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>
#include <string.h>

enum
{
	COLUMN_NAME,
	COLUMN_LANG,
	N_COLUMNS
};

struct _GeditHighlightModeDialogPrivate
{
	GtkWidget *treeview;
	GtkWidget *entry;
	GtkListStore *liststore;
	GtkTreeModelFilter *treemodelfilter;
	GtkTreeSelection *treeview_selection;
};

/* Signals */
enum
{
	LANGUAGE_SELECTED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (GeditHighlightModeDialog, gedit_highlight_mode_dialog, GTK_TYPE_DIALOG)

static void
gedit_highlight_mode_dialog_response (GtkDialog *dialog,
                                      gint       response_id)
{
	GeditHighlightModeDialogPrivate *priv = GEDIT_HIGHLIGHT_MODE_DIALOG (dialog)->priv;

	if (response_id == GTK_RESPONSE_OK)
	{
		GtkSourceLanguage *lang;
		GtkTreeIter iter;

		if (gtk_tree_selection_get_selected (priv->treeview_selection, NULL, &iter))
		{
			gtk_tree_model_get (GTK_TREE_MODEL (priv->treemodelfilter), &iter,
			                    COLUMN_LANG, &lang,
			                    -1);
		}

		g_signal_emit (G_OBJECT (dialog), signals[LANGUAGE_SELECTED], 0, lang);

		if (lang)
		{
			g_object_unref (lang);
		}
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
gedit_highlight_mode_dialog_class_init (GeditHighlightModeDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
	GtkDialogClass *dialog_class = GTK_DIALOG_CLASS (klass);

	dialog_class->response = gedit_highlight_mode_dialog_response;

	signals[LANGUAGE_SELECTED] =
		g_signal_new ("language-selected",
		              G_TYPE_FROM_CLASS (object_class),
		              G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		              G_STRUCT_OFFSET (GeditHighlightModeDialogClass, language_selected),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__OBJECT,
		              G_TYPE_NONE,
		              1,
		              GTK_SOURCE_TYPE_LANGUAGE);

	/* Bind class to template */
	gtk_widget_class_set_template_from_resource (widget_class,
	                                             "/org/gnome/gedit/ui/gedit-highlight-mode-dialog.ui");
	gtk_widget_class_bind_template_child_private (widget_class, GeditHighlightModeDialog, treeview);
	gtk_widget_class_bind_template_child_private (widget_class, GeditHighlightModeDialog, entry);
	gtk_widget_class_bind_template_child_private (widget_class, GeditHighlightModeDialog, liststore);
	gtk_widget_class_bind_template_child_private (widget_class, GeditHighlightModeDialog, treemodelfilter);
	gtk_widget_class_bind_template_child_private (widget_class, GeditHighlightModeDialog, treeview_selection);
}

static gboolean
visible_func (GtkTreeModel             *model,
              GtkTreeIter              *iter,
              GeditHighlightModeDialog *dlg)
{
	const gchar *entry_text;
	gchar *name;
	gchar *name_lower;
	gchar *text_lower;
	gboolean visible = FALSE;

	entry_text = gtk_entry_get_text (GTK_ENTRY (dlg->priv->entry));

	if (*entry_text == '\0')
	{
		return TRUE;
	}

	gtk_tree_model_get (model, iter, COLUMN_NAME, &name, -1);

	name_lower = g_utf8_strdown (name, -1);
	g_free (name);

	text_lower = g_utf8_strdown (entry_text, -1);

	if (strstr (name_lower, text_lower) != NULL)
	{
		visible = TRUE;
	}

	g_free (name_lower);
	g_free (text_lower);

	return visible;
}

static void
on_entry_changed (GtkEntry                 *entry,
                  GeditHighlightModeDialog *dlg)
{
	GtkTreeIter iter;

	gtk_tree_model_filter_refilter (dlg->priv->treemodelfilter);

	if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (dlg->priv->treemodelfilter), &iter))
	{
		gtk_tree_selection_select_iter (dlg->priv->treeview_selection, &iter);
	}
}

static gboolean
move_selection (GeditHighlightModeDialog *dlg,
                gint                      howmany)
{
	GtkTreeIter iter;
	GtkTreePath *path;
	gint *indices;
	gint ret = FALSE;

	if (!gtk_tree_selection_get_selected (dlg->priv->treeview_selection, NULL, &iter) &&
	    !gtk_tree_model_get_iter_first (GTK_TREE_MODEL (dlg->priv->treemodelfilter), &iter))
	{
		return FALSE;
	}

	path = gtk_tree_model_get_path (GTK_TREE_MODEL (dlg->priv->treemodelfilter), &iter);
	indices = gtk_tree_path_get_indices (path);

	if (indices)
	{
		gint num;
		gint idx;
		GtkTreePath *new_path;

		idx = indices[0];
		num = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (dlg->priv->treemodelfilter), NULL);

		if ((idx + howmany) < 0)
		{
			idx = 0;
		}
		else if ((idx + howmany) >= num)
		{
			idx = num - 1;
		}
		else
		{
			idx = idx + howmany;
		}

		new_path = gtk_tree_path_new_from_indices (idx, -1);
		gtk_tree_selection_select_path (dlg->priv->treeview_selection, new_path);
		gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (dlg->priv->treeview),
		                              new_path, NULL, TRUE, 0.5, 0);
		gtk_tree_path_free (new_path);

		ret = TRUE;
	}

	gtk_tree_path_free (path);

	return ret;
}

static gboolean
on_entry_key_press_event (GtkWidget                *entry,
                          GdkEventKey              *event,
                          GeditHighlightModeDialog *dlg)
{
	if (event->keyval == GDK_KEY_Down)
	{
		return move_selection (dlg, 1);
	}
	else if (event->keyval == GDK_KEY_Up)
	{
		return move_selection (dlg, -1);
	}
	else if (event->keyval == GDK_KEY_Page_Down)
	{
		return move_selection (dlg, 5);
	}
	else if (event->keyval == GDK_KEY_Page_Up)
	{
		return move_selection (dlg, -5);
	}

	return FALSE;
}

static void
on_selection_changed (GtkTreeSelection         *selection,
                      GeditHighlightModeDialog *dlg)
{
	gtk_dialog_set_response_sensitive (GTK_DIALOG (dlg),
	                                   GTK_RESPONSE_OK,
	                                   gtk_tree_selection_get_selected (selection, NULL, NULL));
}

static void
on_row_activated (GtkTreeView              *tree_view,
                  GtkTreePath              *path,
                  GtkTreeViewColumn        *column,
                  GeditHighlightModeDialog *dlg)
{
	gtk_window_activate_default (GTK_WINDOW (dlg));
}

static void
gedit_highlight_mode_dialog_init (GeditHighlightModeDialog *dlg)
{
	GeditHighlightModeDialogPrivate *priv;
	GtkSourceLanguageManager *lm;
	const gchar * const *ids;
	gint i;
	GtkTreeIter iter;

	dlg->priv = gedit_highlight_mode_dialog_get_instance_private (dlg);
	priv = dlg->priv;

	gtk_widget_init_template (GTK_WIDGET (dlg));
	gtk_dialog_set_default_response(GTK_DIALOG (dlg), GTK_RESPONSE_OK);

	gtk_tree_model_filter_set_visible_func (priv->treemodelfilter,
	                                        (GtkTreeModelFilterVisibleFunc)visible_func,
	                                        dlg,
	                                        NULL);

	g_signal_connect (priv->entry, "changed",
	                  G_CALLBACK (on_entry_changed), dlg);
	g_signal_connect (priv->entry, "key-press-event",
	                  G_CALLBACK (on_entry_key_press_event), dlg);

	g_signal_connect (priv->treeview_selection, "changed",
	                  G_CALLBACK (on_selection_changed), dlg);

	g_signal_connect (priv->treeview, "row-activated",
	                  G_CALLBACK (on_row_activated), dlg);

	/* Populate tree model */
	gtk_list_store_append (priv->liststore, &iter);
	gtk_list_store_set (priv->liststore, &iter,
	                    COLUMN_NAME, _("Plain Text"),
	                    COLUMN_LANG, NULL,
	                    -1);

	lm = gtk_source_language_manager_get_default ();
	ids = gtk_source_language_manager_get_language_ids (lm);

	for (i = 0; ids[i] != NULL; i++)
	{
		GtkSourceLanguage *lang;

		lang = gtk_source_language_manager_get_language (lm, ids[i]);

		if (!gtk_source_language_get_hidden (lang))
		{
			gtk_list_store_append (priv->liststore, &iter);
			gtk_list_store_set (priv->liststore, &iter,
			                    COLUMN_NAME, gtk_source_language_get_name (lang),
			                    COLUMN_LANG, lang,
			                    -1);
		}
	}

	/* select first item */
	if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (dlg->priv->treemodelfilter), &iter))
	{
		gtk_tree_selection_select_iter (dlg->priv->treeview_selection, &iter);
	}
}

GtkWidget *
gedit_highlight_mode_dialog_new (GtkWindow *parent)
{
	return GTK_WIDGET (g_object_new (GEDIT_TYPE_HIGHLIGHT_MODE_DIALOG,
	                                 "transient-for", parent,
	                                 NULL));
}

void
gedit_highlight_mode_dialog_select_language (GeditHighlightModeDialog *dlg,
                                             GtkSourceLanguage        *language)
{
	GtkTreeIter iter;

	g_return_if_fail (GEDIT_IS_HIGHLIGHT_MODE_DIALOG (dlg));

	if (language == NULL)
	{
		return;
	}

	if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (dlg->priv->treemodelfilter), &iter))
	{
		do
		{
			GtkSourceLanguage *lang;

			gtk_tree_model_get (GTK_TREE_MODEL (dlg->priv->treemodelfilter),
			                    &iter,
			                    COLUMN_LANG, &lang,
			                    -1);

			if (lang != NULL)
			{
				gboolean equal = (lang == language);

				g_object_unref (lang);

				if (equal)
				{
					GtkTreePath *path;

					path = gtk_tree_model_get_path (GTK_TREE_MODEL (dlg->priv->treemodelfilter), &iter);

					gtk_tree_selection_select_iter (dlg->priv->treeview_selection, &iter);
					gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (dlg->priv->treeview),
					                              path, NULL, TRUE, 0.5, 0);
					gtk_tree_path_free (path);
					break;
				}
			}
		}
		while (gtk_tree_model_iter_next (GTK_TREE_MODEL (dlg->priv->treemodelfilter), &iter));
	}
}

/* ex:set ts=8 noet: */
