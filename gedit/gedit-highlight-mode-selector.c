/*
 * gedit-highlight-mode-selector.c
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

#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>
#include <string.h>
#include "gedit-highlight-mode-selector.h"

enum
{
	COLUMN_NAME,
	COLUMN_LANG,
	N_COLUMNS
};

struct _GeditHighlightModeSelectorPrivate
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

G_DEFINE_TYPE_WITH_PRIVATE (GeditHighlightModeSelector, gedit_highlight_mode_selector, GTK_TYPE_GRID)

static void
gedit_highlight_mode_selector_class_init (GeditHighlightModeSelectorClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	signals[LANGUAGE_SELECTED] =
		g_signal_new ("language-selected",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		              G_STRUCT_OFFSET (GeditHighlightModeSelectorClass, language_selected),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__OBJECT,
		              G_TYPE_NONE,
		              1,
		              GTK_SOURCE_TYPE_LANGUAGE);

	/* Bind class to template */
	gtk_widget_class_set_template_from_resource (widget_class,
	                                             "/org/gnome/gedit/ui/gedit-highlight-mode-selector.ui");
	gtk_widget_class_bind_template_child_private (widget_class, GeditHighlightModeSelector, treeview);
	gtk_widget_class_bind_template_child_private (widget_class, GeditHighlightModeSelector, entry);
	gtk_widget_class_bind_template_child_private (widget_class, GeditHighlightModeSelector, liststore);
	gtk_widget_class_bind_template_child_private (widget_class, GeditHighlightModeSelector, treemodelfilter);
	gtk_widget_class_bind_template_child_private (widget_class, GeditHighlightModeSelector, treeview_selection);
}

static gboolean
visible_func (GtkTreeModel               *model,
              GtkTreeIter                *iter,
              GeditHighlightModeSelector *selector)
{
	const gchar *entry_text;
	gchar *name;
	gchar *name_lower;
	gchar *text_lower;
	gboolean visible = FALSE;

	entry_text = gtk_entry_get_text (GTK_ENTRY (selector->priv->entry));

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
on_entry_activate (GtkEntry                   *entry,
                   GeditHighlightModeSelector *selector)
{
	gedit_highlight_mode_selector_activate_selected_language (selector);
}

static void
on_entry_changed (GtkEntry                   *entry,
                  GeditHighlightModeSelector *selector)
{
	GtkTreeIter iter;

	gtk_tree_model_filter_refilter (selector->priv->treemodelfilter);

	if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (selector->priv->treemodelfilter), &iter))
	{
		gtk_tree_selection_select_iter (selector->priv->treeview_selection, &iter);
	}
}

static gboolean
move_selection (GeditHighlightModeSelector *selector,
                gint                      howmany)
{
	GtkTreeIter iter;
	GtkTreePath *path;
	gint *indices;
	gint ret = FALSE;

	if (!gtk_tree_selection_get_selected (selector->priv->treeview_selection, NULL, &iter) &&
	    !gtk_tree_model_get_iter_first (GTK_TREE_MODEL (selector->priv->treemodelfilter), &iter))
	{
		return FALSE;
	}

	path = gtk_tree_model_get_path (GTK_TREE_MODEL (selector->priv->treemodelfilter), &iter);
	indices = gtk_tree_path_get_indices (path);

	if (indices)
	{
		gint num;
		gint idx;
		GtkTreePath *new_path;

		idx = indices[0];
		num = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (selector->priv->treemodelfilter), NULL);

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
		gtk_tree_selection_select_path (selector->priv->treeview_selection, new_path);
		gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (selector->priv->treeview),
		                              new_path, NULL, TRUE, 0.5, 0);
		gtk_tree_path_free (new_path);

		ret = TRUE;
	}

	gtk_tree_path_free (path);

	return ret;
}

static gboolean
on_entry_key_press_event (GtkWidget                  *entry,
                          GdkEventKey                *event,
                          GeditHighlightModeSelector *selector)
{
	if (event->keyval == GDK_KEY_Down)
	{
		return move_selection (selector, 1);
	}
	else if (event->keyval == GDK_KEY_Up)
	{
		return move_selection (selector, -1);
	}
	else if (event->keyval == GDK_KEY_Page_Down)
	{
		return move_selection (selector, 5);
	}
	else if (event->keyval == GDK_KEY_Page_Up)
	{
		return move_selection (selector, -5);
	}

	return FALSE;
}

static void
on_row_activated (GtkTreeView                *tree_view,
                  GtkTreePath                *path,
                  GtkTreeViewColumn          *column,
                  GeditHighlightModeSelector *selector)
{
	gedit_highlight_mode_selector_activate_selected_language (selector);
}

static void
gedit_highlight_mode_selector_init (GeditHighlightModeSelector *selector)
{
	GeditHighlightModeSelectorPrivate *priv;
	GtkSourceLanguageManager *lm;
	const gchar * const *ids;
	gint i;
	GtkTreeIter iter;

	selector->priv = gedit_highlight_mode_selector_get_instance_private (selector);
	priv = selector->priv;

	gtk_widget_init_template (GTK_WIDGET (selector));

	gtk_tree_model_filter_set_visible_func (priv->treemodelfilter,
	                                        (GtkTreeModelFilterVisibleFunc)visible_func,
	                                        selector,
	                                        NULL);

	g_signal_connect (priv->entry, "activate",
	                  G_CALLBACK (on_entry_activate), selector);
	g_signal_connect (priv->entry, "changed",
	                  G_CALLBACK (on_entry_changed), selector);
	g_signal_connect (priv->entry, "key-press-event",
	                  G_CALLBACK (on_entry_key_press_event), selector);

	g_signal_connect (priv->treeview, "row-activated",
	                  G_CALLBACK (on_row_activated), selector);

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
	if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (selector->priv->treemodelfilter), &iter))
	{
		gtk_tree_selection_select_iter (selector->priv->treeview_selection, &iter);
	}
}

GeditHighlightModeSelector *
gedit_highlight_mode_selector_new ()
{
	return g_object_new (GEDIT_TYPE_HIGHLIGHT_MODE_SELECTOR, NULL);
}

void
gedit_highlight_mode_selector_select_language (GeditHighlightModeSelector *selector,
                                             GtkSourceLanguage        *language)
{
	GtkTreeIter iter;

	g_return_if_fail (GEDIT_IS_HIGHLIGHT_MODE_SELECTOR (selector));

	if (language == NULL)
	{
		return;
	}

	if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (selector->priv->treemodelfilter), &iter))
	{
		do
		{
			GtkSourceLanguage *lang;

			gtk_tree_model_get (GTK_TREE_MODEL (selector->priv->treemodelfilter),
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

					path = gtk_tree_model_get_path (GTK_TREE_MODEL (selector->priv->treemodelfilter), &iter);

					gtk_tree_selection_select_iter (selector->priv->treeview_selection, &iter);
					gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (selector->priv->treeview),
					                              path, NULL, TRUE, 0.5, 0);
					gtk_tree_path_free (path);
					break;
				}
			}
		}
		while (gtk_tree_model_iter_next (GTK_TREE_MODEL (selector->priv->treemodelfilter), &iter));
	}
}

void
gedit_highlight_mode_selector_activate_selected_language (GeditHighlightModeSelector *selector)
{
	GeditHighlightModeSelectorPrivate *priv = selector->priv;
	GtkSourceLanguage *lang;
	GtkTreeIter iter;

	g_return_if_fail (GEDIT_IS_HIGHLIGHT_MODE_SELECTOR (selector));

	if (!gtk_tree_selection_get_selected (priv->treeview_selection, NULL, &iter))
	{
		return;
	}

	gtk_tree_model_get (GTK_TREE_MODEL (priv->treemodelfilter), &iter,
	                    COLUMN_LANG, &lang,
	                    -1);

	g_signal_emit (G_OBJECT (selector), signals[LANGUAGE_SELECTED], 0, lang);

	if (lang != NULL)
	{
		g_object_unref (lang);
	}
}

/* ex:set ts=8 noet: */
