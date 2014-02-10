/*
 * gedit-documents-panel.c
 * This file is part of gedit
 *
 * Copyright (C) 2014 - Sebastien Lafargue <slaf66@gmail.com>
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

#include <glib/gi18n.h>

#include "gedit-documents-panel.h"
#include "gedit-debug.h"
#include "gedit-multi-notebook.h"
#include "gedit-notebook.h"
#include "gedit-notebook-popup-menu.h"
#include "gedit-small-button.h"
#include "gedit-utils.h"

typedef struct _GeditDocumentsGenericRow GeditDocumentsGenericRow;
typedef struct _GeditDocumentsGenericRow GeditDocumentsGroupRow;
typedef struct _GeditDocumentsGenericRow GeditDocumentsDocumentRow;

struct _GeditDocumentsGenericRow
{
	GtkListBoxRow        parent;

	GeditDocumentsPanel *panel;
	GtkWidget           *ref;

	GtkWidget           *box;
	GtkWidget           *label;
	GtkWidget           *close_button;

	/* Not used in GeditDocumentsGroupRow */
	GtkWidget           *image;
	GtkWidget           *status_label;
};

#define GEDIT_TYPE_DOCUMENTS_GROUP_ROW            (gedit_documents_group_row_get_type ())
#define GEDIT_DOCUMENTS_GROUP_ROW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GEDIT_TYPE_DOCUMENTS_GROUP_ROW, GeditDocumentsGroupRow))
#define GEDIT_DOCUMENTS_GROUP_ROW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GEDIT_TYPE_DOCUMENTS_GROUP_ROW, GeditDocumentsGroupRowClass))
#define GEDIT_IS_DOCUMENTS_GROUP_ROW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GEDIT_TYPE_DOCUMENTS_GROUP_ROW))
#define GEDIT_IS_DOCUMENTS_GROUP_ROW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GEDIT_TYPE_DOCUMENTS_GROUP_ROW))
#define GEDIT_DOCUMENTS_GROUP_ROW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GEDIT_TYPE_DOCUMENTS_GROUP_ROW, GeditDocumentsGroupRowClass))

typedef struct _GeditDocumentsGroupRowClass GeditDocumentsGroupRowClass;

struct _GeditDocumentsGroupRowClass
{
	GtkListBoxRowClass parent_class;
};

GType gedit_documents_group_row_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE (GeditDocumentsGroupRow, gedit_documents_group_row, GTK_TYPE_LIST_BOX_ROW)

#define GEDIT_TYPE_DOCUMENTS_DOCUMENT_ROW            (gedit_documents_document_row_get_type ())
#define GEDIT_DOCUMENTS_DOCUMENT_ROW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GEDIT_TYPE_DOCUMENTS_DOCUMENT_ROW, GeditDocumentsDocumentRow))
#define GEDIT_DOCUMENTS_DOCUMENT_ROW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GEDIT_TYPE_DOCUMENTS_DOCUMENT_ROW, GeditDocumentsDocumentRowClass))
#define GEDIT_IS_DOCUMENTS_DOCUMENT_ROW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GEDIT_TYPE_DOCUMENTS_DOCUMENT_ROW))
#define GEDIT_IS_DOCUMENTS_DOCUMENT_ROW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GEDIT_TYPE_DOCUMENTS_DOCUMENT_ROW))
#define GEDIT_DOCUMENTS_DOCUMENT_ROW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GEDIT_TYPE_DOCUMENTS_DOCUMENT_ROW, GeditDocumentsDocumentRowClass))

typedef struct _GeditDocumentsDocumentRowClass GeditDocumentsDocumentRowClass;

struct _GeditDocumentsDocumentRowClass
{
	GtkListBoxRowClass parent_class;
};

GType gedit_documents_document_row_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE (GeditDocumentsDocumentRow, gedit_documents_document_row, GTK_TYPE_LIST_BOX_ROW)

static GtkWidget *gedit_documents_document_row_new (GeditDocumentsPanel *panel,
                                                    GeditTab            *tab);
static GtkWidget *gedit_documents_group_row_new    (GeditDocumentsPanel *panel,
                                                    GeditNotebook       *notebook);

struct _GeditDocumentsPanelPrivate
{
	GeditWindow        *window;
	GeditMultiNotebook *mnb;
	GtkWidget          *listbox;

	guint               selection_changed_handler_id;
	guint               tab_switched_handler_id;
	gboolean            is_in_tab_switched;

	/* Flag to workaround first GroupRow selection at start ( we don't want to show it ) */
	gboolean            first_selection;

	GtkAdjustment      *adjustment;

	guint               nb_row_notebook;
	guint               nb_row_tab;
};

G_DEFINE_TYPE_WITH_PRIVATE (GeditDocumentsPanel, gedit_documents_panel, GTK_TYPE_BOX)

enum
{
	PROP_0,
	PROP_WINDOW
};

#define MAX_DOC_NAME_LENGTH 60

static guint
get_nb_visible_rows (GeditDocumentsPanel *panel)
{
	guint nb = 0;

	if (panel->priv->nb_row_notebook > 1)
	{
		nb += panel->priv->nb_row_notebook;
	}

	nb += panel->priv->nb_row_tab;

	return nb;
}

static guint
get_row_visible_index (GeditDocumentsPanel *panel,
                       GtkWidget           *searched_row)
{
	GList *children;
	GList *l;
	guint nb_notebook_row = 0;
	guint nb_tab_row = 0;

	children = gtk_container_get_children (GTK_CONTAINER (panel->priv->listbox));

	for (l = children; l != NULL; l = g_list_next (l))
	{
		GtkWidget *row = l->data;

		if (GEDIT_IS_DOCUMENTS_GROUP_ROW (row))
		{
			nb_notebook_row += 1;
		}
		else
		{
			nb_tab_row += 1;
		}

		if (row == searched_row)
		{
			break;
		}
	}

	g_list_free (children);

	if (panel->priv->nb_row_notebook == 1)
	{
		nb_notebook_row = 0;
	}

	return nb_tab_row + nb_notebook_row - 1;
}

/* We do not grab focus on the row, so scroll it into view manually */
static void
make_row_visible (GeditDocumentsPanel *panel,
                  GtkWidget           *row)
{
	gdouble adjustment_value;
	gdouble adjustment_lower;
	gdouble adjustment_upper;
	gdouble adjustment_page_size;
	gdouble nb_visible_rows;
	gdouble row_visible_index;
	gdouble row_size;
	gdouble row_position;
	gdouble offset;
	gdouble new_adjustment_value;

	adjustment_value = gtk_adjustment_get_value (panel->priv->adjustment);
	adjustment_lower = gtk_adjustment_get_lower (panel->priv->adjustment);
	adjustment_upper = gtk_adjustment_get_upper (panel->priv->adjustment);
	adjustment_page_size = gtk_adjustment_get_page_size (panel->priv->adjustment);

	nb_visible_rows = get_nb_visible_rows (panel);
	row_visible_index = get_row_visible_index (panel, GTK_WIDGET (row));

	row_size = (adjustment_upper - adjustment_lower) / nb_visible_rows;
	row_position =  row_size * row_visible_index;

	if (row_position < adjustment_value)
	{
		new_adjustment_value = row_position;
	}
	else if ((row_position + row_size) > (adjustment_value + adjustment_page_size))
	{
		offset = (row_position + row_size) - (adjustment_value + adjustment_page_size);
		new_adjustment_value = adjustment_value + offset;
	}
	else
	{
		new_adjustment_value = adjustment_value;
	}

	gtk_adjustment_set_value (panel->priv->adjustment, new_adjustment_value);
}

/* This function is a GCompareFunc to use with g_list_find_custom */
static gint
listbox_search_function (gconstpointer row,
                         gconstpointer item)
{
	GeditDocumentsGenericRow *generic_row = (GeditDocumentsGenericRow *)row;
	gpointer *searched_item = (gpointer *)generic_row->ref;

	return searched_item == item ? 0 : -1;
}

static GtkListBoxRow *
get_row_from_widget (GeditDocumentsPanel *panel,
                     GtkWidget           *widget)
{
	GList *children;
	GList *item;
	GtkListBoxRow *row;

	children = gtk_container_get_children (GTK_CONTAINER (panel->priv->listbox));

	item = g_list_find_custom (children, widget, listbox_search_function);
	row = item ? item->data : NULL;

	g_list_free (children);

	return row;
}

static void
row_select (GeditDocumentsPanel *panel,
            GtkListBox          *listbox,
            GtkListBoxRow       *row)
{
	GtkListBoxRow *selected_row = gtk_list_box_get_selected_row (listbox);

	if (row != selected_row)
	{
		g_signal_handler_block (listbox, panel->priv->selection_changed_handler_id);
		gtk_list_box_select_row (listbox, row);
		g_signal_handler_unblock (listbox, panel->priv->selection_changed_handler_id);
	}

	make_row_visible (panel, GTK_WIDGET (row));
}

static void
row_state_changed (GtkWidget           *row,
                   GtkStateFlags        previous_flags,
                   GeditDocumentsPanel *panel)
{
	GeditDocumentsGenericRow *generic_row = (GeditDocumentsGenericRow *)row;
	GtkStateFlags flags;

	flags = gtk_widget_get_state_flags (row);

	gtk_widget_set_visible (generic_row->close_button,
	                        flags & GTK_STATE_FLAG_PRELIGHT);

	if (GEDIT_IS_DOCUMENTS_GROUP_ROW (row))
	{
		GTK_WIDGET_CLASS (gedit_documents_group_row_parent_class)->state_flags_changed (row, previous_flags);
	}
	else
	{
		GTK_WIDGET_CLASS (gedit_documents_document_row_parent_class)->state_flags_changed (row, previous_flags);
	}
}

static void
insert_row (GeditDocumentsPanel *panel,
            GtkListBox          *listbox,
            GtkWidget           *row,
            gint                 position)
{
	g_signal_handler_block (listbox, panel->priv->selection_changed_handler_id);

	gtk_list_box_insert (listbox, row, position);

	g_signal_connect (row,
	                  "state-flags-changed",
	                  G_CALLBACK (row_state_changed),
	                  panel);

	g_signal_handler_unblock (listbox, panel->priv->selection_changed_handler_id);
}

static void
select_active_tab (GeditDocumentsPanel *panel)
{
	GeditNotebook *notebook;
	gboolean have_tabs;
	GeditTab *tab;

	notebook = gedit_multi_notebook_get_active_notebook (panel->priv->mnb);
	have_tabs = gtk_notebook_get_n_pages (GTK_NOTEBOOK (notebook)) > 0;
	tab = gedit_multi_notebook_get_active_tab (panel->priv->mnb);

	if (notebook != NULL && tab != NULL && have_tabs)
	{
		GtkListBoxRow *row = get_row_from_widget (panel, GTK_WIDGET (tab));

		if (row)
		{
			row_select (panel, GTK_LIST_BOX (panel->priv->listbox), row);
		}
	}
}

static GtkListBoxRow *
get_first_notebook_found (GeditDocumentsPanel *panel)
{
	GList *children;
	GList *l;
	GtkListBoxRow *row = NULL;

	children = gtk_container_get_children (GTK_CONTAINER (panel->priv->listbox));

	for (l = children; l != NULL; l = g_list_next (l))
	{
		if (GEDIT_IS_DOCUMENTS_GROUP_ROW (l->data))
		{
			row = l->data;
			break;
		}
	}

	g_list_free (children);

	return row;
}

static void
multi_notebook_tab_switched (GeditMultiNotebook  *mnb,
                             GeditNotebook       *old_notebook,
                             GeditTab            *old_tab,
                             GeditNotebook       *new_notebook,
                             GeditTab            *new_tab,
                             GeditDocumentsPanel *panel)
{
	gedit_debug (DEBUG_PANEL);

	if (!_gedit_window_is_removing_tabs (panel->priv->window) &&
	    panel->priv->is_in_tab_switched == FALSE)
	{
		GtkListBoxRow *row;

		panel->priv->is_in_tab_switched = TRUE;

		row = get_row_from_widget (panel, GTK_WIDGET (new_tab));

		if (row)
		{
			row_select (panel, GTK_LIST_BOX (panel->priv->listbox), row);
		}

		panel->priv->is_in_tab_switched = FALSE;
	}
}

static void
group_row_set_notebook_name (GtkWidget *row)
{
	GeditNotebook *notebook;
	GeditMultiNotebook *mnb;
	guint num;
	gchar *name;
	GeditDocumentsGroupRow *group_row = GEDIT_DOCUMENTS_GROUP_ROW (row);

	notebook = GEDIT_NOTEBOOK (group_row->ref);

	mnb = group_row->panel->priv->mnb;
	num = gedit_multi_notebook_get_notebook_num (mnb, notebook);

	name = g_strdup_printf (_("Tab Group %i"), num + 1);

	gtk_label_set_text (GTK_LABEL (group_row->label), name);

	g_free (name);
}

static void
group_row_update_names (GeditDocumentsPanel *panel,
                        GtkWidget           *listbox)
{
	GList *children;
	GList *l;
	GtkWidget *row;

	children = gtk_container_get_children (GTK_CONTAINER (listbox));

	for (l = children; l != NULL; l = g_list_next (l))
	{
		row = l->data;

		if (GEDIT_IS_DOCUMENTS_GROUP_ROW (row))
		{
			group_row_set_notebook_name (row);
		}
	}

	g_list_free (children);
}

static void
group_row_refresh_visibility (GeditDocumentsPanel *panel)
{
	gboolean notebook_is_unique;
	GtkWidget *first_group_row;

	notebook_is_unique = gedit_multi_notebook_get_n_notebooks (panel->priv->mnb) <= 1;
	first_group_row = GTK_WIDGET (get_first_notebook_found (panel));

	gtk_widget_set_no_show_all (first_group_row, notebook_is_unique);
	gtk_widget_set_visible (first_group_row, !notebook_is_unique);
}

static void
refresh_notebook (GeditDocumentsPanel *panel,
                  GeditNotebook       *notebook)
{
	GList *tabs;
	GList *l;

	tabs = gtk_container_get_children (GTK_CONTAINER (notebook));

	for (l = tabs; l != NULL; l = g_list_next (l))
	{
		GtkWidget *row;

		row = gedit_documents_document_row_new (panel, GEDIT_TAB (l->data));
		insert_row (panel, GTK_LIST_BOX (panel->priv->listbox), row, -1);
		panel->priv->nb_row_tab += 1;
	}

	g_list_free (tabs);
}

static void
refresh_notebook_foreach (GeditNotebook       *notebook,
                          GeditDocumentsPanel *panel)
{
	GtkWidget *row;

	row = gedit_documents_group_row_new (panel, notebook);
	insert_row (panel, GTK_LIST_BOX (panel->priv->listbox), row, -1);
	panel->priv->nb_row_notebook += 1;

	group_row_refresh_visibility (panel);
	refresh_notebook (panel, notebook);
}

static void
refresh_list (GeditDocumentsPanel *panel)
{
	GList *children;
	GList *l;

	/* Clear the listbox */
	children = gtk_container_get_children (GTK_CONTAINER (panel->priv->listbox));

	for (l = children; l != NULL; l = g_list_next (l))
	{
		gtk_widget_destroy (GTK_WIDGET (l->data));
	}

	g_list_free (children);

	gedit_multi_notebook_foreach_notebook (panel->priv->mnb,
	                                       (GtkCallback)refresh_notebook_foreach,
	                                       panel);
	select_active_tab (panel);
}

static void
multi_notebook_tab_removed (GeditMultiNotebook  *mnb,
                            GeditNotebook       *notebook,
                            GeditTab            *tab,
                            GeditDocumentsPanel *panel)
{
	GtkListBoxRow *row;

	gedit_debug (DEBUG_PANEL);

	row = get_row_from_widget (panel, GTK_WIDGET (tab));
	gtk_widget_destroy (GTK_WIDGET (row));
	panel->priv->nb_row_tab -= 1;
}

static gint
get_dest_position_for_tab (GeditDocumentsPanel *panel,
                           GeditNotebook       *notebook,
                           GeditTab            *tab)
{
	gint page_num;
	GList *children;
	GList *item;
	gint res = -1;

	/* Get tab's position in notebook and notebook's position in GtkListBox
	 * then return future tab's position in GtkListBox */

	page_num = gtk_notebook_page_num (GTK_NOTEBOOK (notebook), GTK_WIDGET (tab));

	children = gtk_container_get_children (GTK_CONTAINER (panel->priv->listbox));
	item = g_list_find_custom (children, notebook, listbox_search_function);

	if (item)
	{
		res = 1 + page_num + g_list_position (children, item);
	}

	g_list_free (children);

	return res;
}

static void
multi_notebook_tab_added (GeditMultiNotebook  *mnb,
                          GeditNotebook       *notebook,
                          GeditTab            *tab,
                          GeditDocumentsPanel *panel)
{
	gint position;
	GtkWidget *row;

	gedit_debug (DEBUG_PANEL);

	position = get_dest_position_for_tab (panel, notebook, tab);

	if (position == -1)
	{
		/* Notebook doesn't exit in GtkListBox, so create it */
		row = gedit_documents_group_row_new (panel, notebook);
		insert_row (panel, GTK_LIST_BOX (panel->priv->listbox), row, -1);

		panel->priv->nb_row_notebook += 1;

		/* Now, we have a correct position */
		position = get_dest_position_for_tab (panel, notebook, tab);

		group_row_refresh_visibility (panel);
	}

	/* Add a new tab's row to the listbox */
	row = gedit_documents_document_row_new (panel, tab);
	insert_row (panel, GTK_LIST_BOX (panel->priv->listbox), row, position);

	panel->priv->nb_row_tab += 1;

	row_select (panel, GTK_LIST_BOX (panel->priv->listbox), GTK_LIST_BOX_ROW (row));
}

static void
multi_notebook_notebook_removed (GeditMultiNotebook  *mnb,
                                 GeditNotebook       *notebook,
                                 GeditDocumentsPanel *panel)
{
	GtkListBoxRow *row;

	gedit_debug (DEBUG_PANEL);

	row = get_row_from_widget (panel, GTK_WIDGET (notebook));
	gtk_container_remove (GTK_CONTAINER (panel->priv->listbox), GTK_WIDGET (row));

	panel->priv->nb_row_notebook -= 1;

	group_row_refresh_visibility (panel);
	group_row_update_names (panel, panel->priv->listbox);
}

static void
row_move (GeditDocumentsPanel *panel,
          GeditNotebook       *notebook,
          GtkWidget           *tab,
          GtkWidget           *row)
{
	gint position;

	g_object_ref (row);

	gtk_container_remove (GTK_CONTAINER (panel->priv->listbox), GTK_WIDGET (row));
	position = get_dest_position_for_tab (panel, notebook, GEDIT_TAB (tab));

	g_signal_handler_block (panel->priv->listbox,
	                        panel->priv->selection_changed_handler_id);

	gtk_list_box_insert (GTK_LIST_BOX (panel->priv->listbox), row, position);

	g_object_unref (row);

	g_signal_handler_unblock (GTK_LIST_BOX (panel->priv->listbox),
	                          panel->priv->selection_changed_handler_id);
}

static void
multi_notebook_tabs_reordered (GeditMultiNotebook  *mnb,
                               GeditNotebook       *notebook,
                               GtkWidget           *page,
                               gint                 page_num,
                               GeditDocumentsPanel *panel)
{
	GtkListBoxRow *row;

	gedit_debug (DEBUG_PANEL);

	row = get_row_from_widget (panel, GTK_WIDGET (page));

	row_move (panel, notebook, page, GTK_WIDGET (row));

	row_select (panel, GTK_LIST_BOX (panel->priv->listbox), GTK_LIST_BOX_ROW (row));
}

static void
set_window (GeditDocumentsPanel *panel,
            GeditWindow         *window)
{
	panel->priv->window = g_object_ref (window);
	panel->priv->mnb = GEDIT_MULTI_NOTEBOOK (_gedit_window_get_multi_notebook (window));

	g_signal_connect (panel->priv->mnb,
	                  "notebook-removed",
	                  G_CALLBACK (multi_notebook_notebook_removed),
	                  panel);
	g_signal_connect (panel->priv->mnb,
	                  "tab-added",
	                  G_CALLBACK (multi_notebook_tab_added),
	                  panel);
	g_signal_connect (panel->priv->mnb,
	                  "tab-removed",
	                  G_CALLBACK (multi_notebook_tab_removed),
	                  panel);
	g_signal_connect (panel->priv->mnb,
	                  "page-reordered",
	                  G_CALLBACK (multi_notebook_tabs_reordered),
	                  panel);

	panel->priv->tab_switched_handler_id = g_signal_connect (panel->priv->mnb,
	                                                         "switch-tab",
	                                                         G_CALLBACK (multi_notebook_tab_switched),
	                                                         panel);

	panel->priv->first_selection = TRUE;

	refresh_list (panel);
	group_row_refresh_visibility (panel);
}

static void
listbox_selection_changed (GtkListBox          *listbox,
                           GtkListBoxRow       *row,
                           GeditDocumentsPanel *panel)
{
	if (row == NULL)
	{
		/* No selection on document panel */
		return;
	}

	/* When the window is shown, the first notebook row is selected
	 * and therefore also shown - we don't want this */

	if (panel->priv->first_selection)
	{
		panel->priv->first_selection = FALSE;
		group_row_refresh_visibility (panel);
	}

	g_signal_handler_block (panel->priv->mnb,
	                        panel->priv->tab_switched_handler_id);

	if (GEDIT_IS_DOCUMENTS_DOCUMENT_ROW (row))
	{
		gedit_multi_notebook_set_active_tab (panel->priv->mnb,
		                                     GEDIT_TAB (GEDIT_DOCUMENTS_DOCUMENT_ROW (row)->ref));
	}
	else if (GEDIT_IS_DOCUMENTS_GROUP_ROW (row))
	{
		gtk_widget_grab_focus (GTK_WIDGET (GEDIT_DOCUMENTS_GROUP_ROW (row)->ref));
	}
	else
	{
		g_assert_not_reached ();
	}

	g_signal_handler_unblock (panel->priv->mnb,
	                          panel->priv->tab_switched_handler_id);
}

static void
gedit_documents_panel_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
	GeditDocumentsPanel *panel = GEDIT_DOCUMENTS_PANEL (object);

	switch (prop_id)
	{
		case PROP_WINDOW:
			set_window (panel, g_value_get_object (value));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gedit_documents_panel_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
	GeditDocumentsPanel *panel = GEDIT_DOCUMENTS_PANEL (object);

	switch (prop_id)
	{
		case PROP_WINDOW:
			g_value_set_object (value, panel->priv->window);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gedit_documents_panel_finalize (GObject *object)
{
	GeditDocumentsPanel *panel = GEDIT_DOCUMENTS_PANEL (object);

	g_signal_handlers_disconnect_by_func (panel->priv->mnb,
	                                      G_CALLBACK (multi_notebook_notebook_removed),
	                                      panel);
	g_signal_handlers_disconnect_by_func (panel->priv->mnb,
	                                      G_CALLBACK (multi_notebook_tab_added),
	                                      panel);
	g_signal_handlers_disconnect_by_func (panel->priv->mnb,
	                                      G_CALLBACK (multi_notebook_tab_removed),
	                                      panel);
	g_signal_handlers_disconnect_by_func (panel->priv->mnb,
	                                      G_CALLBACK (multi_notebook_tabs_reordered),
	                                      panel);
	g_signal_handlers_disconnect_by_func (panel->priv->mnb,
	                                      G_CALLBACK (multi_notebook_tab_switched),
	                                      panel);

	G_OBJECT_CLASS (gedit_documents_panel_parent_class)->finalize (object);
}

static void
gedit_documents_panel_dispose (GObject *object)
{
	GeditDocumentsPanel *panel = GEDIT_DOCUMENTS_PANEL (object);

	g_clear_object (&panel->priv->window);

	G_OBJECT_CLASS (gedit_documents_panel_parent_class)->dispose (object);
}

static void
gedit_documents_panel_class_init (GeditDocumentsPanelClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = gedit_documents_panel_finalize;
	object_class->dispose = gedit_documents_panel_dispose;
	object_class->get_property = gedit_documents_panel_get_property;
	object_class->set_property = gedit_documents_panel_set_property;

	g_object_class_install_property (object_class,
	                                 PROP_WINDOW,
	                                 g_param_spec_object ("window",
	                                                      "Window",
	                                                      "The GeditWindow this GeditDocumentsPanel is associated with",
	                                                      GEDIT_TYPE_WINDOW,
	                                                      G_PARAM_READWRITE |
	                                                      G_PARAM_CONSTRUCT_ONLY |
	                                                      G_PARAM_STATIC_STRINGS));
}

static void
gedit_documents_panel_init (GeditDocumentsPanel *panel)
{
	GtkWidget *sw;
	GtkStyleContext *context;

	gedit_debug (DEBUG_PANEL);

	panel->priv = gedit_documents_panel_get_instance_private (panel);

	gtk_orientable_set_orientation (GTK_ORIENTABLE (panel),
	                                GTK_ORIENTATION_VERTICAL);

	/* Create the scrolled window */
	sw = gtk_scrolled_window_new (NULL, NULL);

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
	                                GTK_POLICY_AUTOMATIC,
	                                GTK_POLICY_AUTOMATIC);
	gtk_widget_show (sw);
	gtk_box_pack_start (GTK_BOX (panel), sw, TRUE, TRUE, 0);

	/* Create the listbox */
	panel->priv->listbox = gtk_list_box_new ();

	gtk_container_add (GTK_CONTAINER (sw), panel->priv->listbox);

	panel->priv->adjustment = gtk_list_box_get_adjustment (GTK_LIST_BOX (panel->priv->listbox));

	/* Disable focus so it doesn't steal focus each time from the view */
	gtk_widget_set_can_focus (panel->priv->listbox, FALSE);

	/* Css style */
	context = gtk_widget_get_style_context (panel->priv->listbox);
	gtk_style_context_add_class (context, "gedit-document-panel");

	panel->priv->selection_changed_handler_id = g_signal_connect (panel->priv->listbox,
	                                                              "row-selected",
	                                                              G_CALLBACK (listbox_selection_changed),
	                                                              panel);
	panel->priv->is_in_tab_switched = FALSE;
	panel->priv->nb_row_notebook = 0;
	panel->priv->nb_row_tab = 0;
}

GtkWidget *
gedit_documents_panel_new (GeditWindow *window)
{
	g_return_val_if_fail (GEDIT_IS_WINDOW (window), NULL);

	return g_object_new (GEDIT_TYPE_DOCUMENTS_PANEL,
	                     "window", window,
	                     NULL);
}

static gchar *
doc_get_name (GeditDocument *doc)
{
	gchar *name;
	gchar *docname;

	name = gedit_document_get_short_name_for_display (doc);

	/* Truncate the name so it doesn't get insanely wide. */
	docname = gedit_utils_str_middle_truncate (name, MAX_DOC_NAME_LENGTH);

	g_free (name);

	return docname;
}

static void
row_on_close_button_clicked (GtkWidget *close_button,
                             GtkWidget *row)
{
	GtkWidget *ref;
	GeditNotebook *notebook;

	if (GEDIT_IS_DOCUMENTS_GROUP_ROW (row))
	{
		/* Removes all tabs, then the tab group */
		ref = GEDIT_DOCUMENTS_GROUP_ROW (row)->ref;

		gedit_notebook_remove_all_tabs (GEDIT_NOTEBOOK (ref));
	}
	else if (GEDIT_IS_DOCUMENTS_DOCUMENT_ROW (row))
	{
		/* Removes the tab */
		ref = GEDIT_DOCUMENTS_DOCUMENT_ROW (row)->ref;

		notebook = GEDIT_NOTEBOOK (gtk_widget_get_parent (GTK_WIDGET (ref)));
		gtk_container_remove (GTK_CONTAINER (notebook), GTK_WIDGET (ref));
	}
	else
	{
		g_assert_not_reached ();
	}
}

static gboolean
row_on_button_pressed (GtkWidget      *row_event_box,
                       GdkEventButton *event,
                       GtkWidget      *row)
{
	if (event->type == GDK_BUTTON_PRESS &&
	    gdk_event_triggers_context_menu ((GdkEvent *)event) &&
	    GEDIT_IS_DOCUMENTS_DOCUMENT_ROW (row))
	{
		GeditDocumentsGenericRow *generic_row = (GeditDocumentsGenericRow *)row;
		GeditWindow *window = generic_row->panel->priv->window;
		GeditTab *tab = GEDIT_TAB (generic_row->ref);
		GtkWidget *menu = gedit_notebook_popup_menu_new (window, tab);

		gtk_menu_popup_for_device (GTK_MENU (menu),
		                           gdk_event_get_device ((GdkEvent *)event),
		                           NULL, NULL,
		                           NULL, NULL, NULL,
		                           event->button,
		                           event->time);

		return TRUE;
	}

	return FALSE;
}

static void
document_row_sync_tab_name_and_icon (GeditTab   *tab,
                                     GParamSpec *pspec,
                                     GtkWidget  *row)
{
	GeditDocumentsDocumentRow *document_row = GEDIT_DOCUMENTS_DOCUMENT_ROW (row);
	GeditDocument *doc;
	gchar *name;
	GdkPixbuf *pixbuf;

	doc = gedit_tab_get_document (tab);
	name = doc_get_name (doc);

	if (!gtk_text_buffer_get_modified (GTK_TEXT_BUFFER (doc)))
	{
		gtk_label_set_text (GTK_LABEL (document_row->label), name);
	}
	else
	{
		gchar *markup;

		markup = g_markup_printf_escaped ("<b>%s</b>", name);
		gtk_label_set_markup (GTK_LABEL (document_row->label), markup);

		g_free (markup);
	}

	g_free (name);

	/* The status has as separate label to prevent ellipsizing */
	if (!gedit_document_get_readonly (doc))
	{
		gtk_widget_hide (GTK_WIDGET (document_row->status_label));
	}
	else
	{
		gchar *status;

		status = g_strdup_printf ("[%s]", _("Read-Only"));

		gtk_label_set_text (GTK_LABEL (document_row->status_label), status);
		gtk_widget_show (GTK_WIDGET (document_row->status_label));

		g_free (status);
	}

	/* Update header of the row */
	pixbuf = _gedit_tab_get_icon (tab);

	if (pixbuf)
	{
		gtk_image_set_from_pixbuf (GTK_IMAGE (document_row->image), pixbuf);
	}
	else
	{
		gtk_image_clear (GTK_IMAGE (document_row->image));
	}
}

static void
document_row_create_header (GtkWidget *row)
{
	GeditDocumentsDocumentRow *document_row = GEDIT_DOCUMENTS_DOCUMENT_ROW (row);
	GtkWidget *image_box;
	gint width, height;

	gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &width, &height);

	image_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_set_size_request (image_box, width, height);

	document_row->image = gtk_image_new ();

	gtk_container_add (GTK_CONTAINER (image_box), document_row->image);

	gtk_box_pack_start (GTK_BOX (document_row->box),
	                    image_box, FALSE, FALSE, 0);

	/* Set the header on front of all other widget in the row */
	gtk_box_reorder_child (GTK_BOX (document_row->box),
	                       image_box, 0);

	gtk_widget_show_all (image_box);
}

static GtkWidget *
row_create (GtkWidget *row)
{
	GeditDocumentsGenericRow *generic_row = (GeditDocumentsGenericRow *)row;
	GtkWidget *event_box;

	gedit_debug (DEBUG_PANEL);

	event_box = gtk_event_box_new ();
	generic_row->box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);

	gtk_container_add (GTK_CONTAINER (event_box), generic_row->box);

	generic_row->label = gtk_label_new (NULL);
	gtk_label_set_ellipsize (GTK_LABEL (generic_row->label), PANGO_ELLIPSIZE_END);
	gtk_widget_set_halign (generic_row->label, GTK_ALIGN_START);
	gtk_misc_set_alignment (GTK_MISC (generic_row->label), 0.0, 0.5);

	generic_row->status_label = gtk_label_new (NULL);
	gtk_widget_set_halign (generic_row->status_label, GTK_ALIGN_END);
	gtk_misc_set_alignment (GTK_MISC (generic_row->status_label), 1.0, 0.5);

	generic_row->close_button = gedit_close_button_new ();

	gtk_box_pack_start (GTK_BOX (generic_row->box),
	                    generic_row->label, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (generic_row->box),
	                    generic_row->status_label, FALSE, FALSE, 0);

	gtk_box_pack_end (GTK_BOX (generic_row->box),
	                  generic_row->close_button, FALSE, FALSE, 0);

	g_signal_connect (event_box,
	                  "button-press-event",
	                  G_CALLBACK (row_on_button_pressed),
	                  row);
	g_signal_connect (generic_row->close_button,
	                  "clicked",
	                  G_CALLBACK (row_on_close_button_clicked),
	                  row);

	gtk_widget_set_no_show_all (generic_row->status_label, TRUE);
	gtk_widget_set_no_show_all (generic_row->close_button, TRUE);
	gtk_widget_show_all (event_box);

	return event_box;
}

static gboolean
document_row_query_tooltip (GtkWidget   *row,
                            gint         x,
                            gint         y,
                            gboolean     keyboard_tip,
                            GtkTooltip  *tooltip)
{
	GeditDocumentsGenericRow *generic_row = (GeditDocumentsGenericRow *)row;
	gchar *markup;

	if (!GEDIT_IS_DOCUMENTS_DOCUMENT_ROW (row))
	{
		return FALSE;
	}

	markup = _gedit_tab_get_tooltip (GEDIT_TAB (generic_row->ref));
	gtk_tooltip_set_markup (tooltip, markup);

	g_free (markup);

	return TRUE;
}

/* Gedit Document Row */
static void
gedit_documents_document_row_class_init (GeditDocumentsDocumentRowClass *klass)
{
}

static void
gedit_documents_document_row_init (GeditDocumentsDocumentRow *row)
{
	GtkWidget *row_widget;
	GtkStyleContext *context;

	gedit_debug (DEBUG_PANEL);

	row_widget = row_create (GTK_WIDGET (row));
	gtk_container_add (GTK_CONTAINER (row), row_widget);

	document_row_create_header (GTK_WIDGET (row));

	gtk_widget_set_has_tooltip (GTK_WIDGET (row), TRUE);

	/* Css style */
	context = gtk_widget_get_style_context (GTK_WIDGET (row));
	gtk_style_context_add_class (context, "gedit-document-panel-document-row");

	gtk_widget_show_all (GTK_WIDGET (row));
	gtk_widget_hide (row->close_button);

	gtk_widget_set_can_focus (GTK_WIDGET (row), FALSE);
}

/* Gedit Group Row */
static void
gedit_documents_group_row_class_init (GeditDocumentsGroupRowClass *klass)
{
}

static void
gedit_documents_group_row_init (GeditDocumentsGroupRow *row)
{
	GtkWidget *row_widget;
	GtkStyleContext *context;

	gedit_debug (DEBUG_PANEL);

	row_widget = row_create (GTK_WIDGET (row));
	gtk_container_add (GTK_CONTAINER (row), row_widget);

	/* Css style */
	context = gtk_widget_get_style_context (GTK_WIDGET (row));
	gtk_style_context_add_class (context, "gedit-document-panel-group-row");

	gtk_widget_show_all (GTK_WIDGET (row));
	gtk_widget_hide (row->close_button);

	gtk_widget_set_can_focus (GTK_WIDGET (row), FALSE);
}

static GtkWidget *
gedit_documents_document_row_new (GeditDocumentsPanel *panel,
                                  GeditTab *tab)
{
	GeditDocumentsDocumentRow *row;

	g_return_val_if_fail (GEDIT_IS_DOCUMENTS_PANEL (panel), NULL);
	g_return_val_if_fail (GEDIT_IS_TAB (tab), NULL);

	gedit_debug (DEBUG_PANEL);

	row = g_object_new (GEDIT_TYPE_DOCUMENTS_DOCUMENT_ROW, NULL);
	row->ref = GTK_WIDGET (tab);
	row->panel = panel;

	g_signal_connect (row->ref,
	                  "notify::name",
	                  G_CALLBACK (document_row_sync_tab_name_and_icon),
	                  row);
	g_signal_connect (row->ref,
	                  "notify::state",
	                  G_CALLBACK (document_row_sync_tab_name_and_icon),
	                  row);
	g_signal_connect (row,
	                  "query-tooltip",
	                  G_CALLBACK (document_row_query_tooltip),
	                  NULL);

	document_row_sync_tab_name_and_icon (GEDIT_TAB (row->ref), NULL, GTK_WIDGET (row));

	return GTK_WIDGET (row);
}

static GtkWidget *
gedit_documents_group_row_new (GeditDocumentsPanel *panel,
                               GeditNotebook       *notebook)
{
	GeditDocumentsGroupRow *row;

	g_return_val_if_fail (GEDIT_IS_DOCUMENTS_PANEL (panel), NULL);
	g_return_val_if_fail (GEDIT_IS_NOTEBOOK (notebook), NULL);

	gedit_debug (DEBUG_PANEL);

	row = g_object_new (GEDIT_TYPE_DOCUMENTS_GROUP_ROW, NULL);
	row->ref = GTK_WIDGET (notebook);
	row->panel = panel;

	group_row_set_notebook_name (GTK_WIDGET (row));

	return GTK_WIDGET (row);
}

/* ex:set ts=8 noet: */
