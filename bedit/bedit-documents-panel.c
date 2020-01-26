/*
 * gedit-documents-panel.c
 * This file is part of gedit
 *
 * Copyright (C) 2014 - Sébastien Lafargue <slafargue@gnome.org>
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

#include "config.h"

#include "gedit-documents-panel.h"

#include <glib/gi18n.h>

#include "gedit-debug.h"
#include "gedit-document.h"
#include "gedit-multi-notebook.h"
#include "gedit-notebook.h"
#include "gedit-notebook-popup-menu.h"
#include "gedit-tab.h"
#include "gedit-tab-private.h"
#include "gedit-utils.h"
#include "gedit-commands-private.h"

typedef struct _BeditDocumentsGenericRow BeditDocumentsGenericRow;
typedef struct _BeditDocumentsGenericRow BeditDocumentsGroupRow;
typedef struct _BeditDocumentsGenericRow BeditDocumentsDocumentRow;

struct _BeditDocumentsGenericRow
{
	GtkListBoxRow        parent_instance;

	BeditDocumentsPanel *panel;
	GtkWidget           *ref;

	GtkWidget           *box;
	GtkWidget           *label;
	GtkWidget           *close_button;

	/* Not used in BeditDocumentsGroupRow */
	GtkWidget           *image;
	GtkWidget           *status_label;
};

#define GEDIT_TYPE_DOCUMENTS_GROUP_ROW            (gedit_documents_group_row_get_type ())
#define GEDIT_DOCUMENTS_GROUP_ROW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GEDIT_TYPE_DOCUMENTS_GROUP_ROW, BeditDocumentsGroupRow))
#define GEDIT_DOCUMENTS_GROUP_ROW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GEDIT_TYPE_DOCUMENTS_GROUP_ROW, BeditDocumentsGroupRowClass))
#define GEDIT_IS_DOCUMENTS_GROUP_ROW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GEDIT_TYPE_DOCUMENTS_GROUP_ROW))
#define GEDIT_IS_DOCUMENTS_GROUP_ROW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GEDIT_TYPE_DOCUMENTS_GROUP_ROW))
#define GEDIT_DOCUMENTS_GROUP_ROW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GEDIT_TYPE_DOCUMENTS_GROUP_ROW, BeditDocumentsGroupRowClass))

typedef struct _BeditDocumentsGroupRowClass BeditDocumentsGroupRowClass;

struct _BeditDocumentsGroupRowClass
{
	GtkListBoxRowClass parent_class;
};

GType gedit_documents_group_row_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE (BeditDocumentsGroupRow, gedit_documents_group_row, GTK_TYPE_LIST_BOX_ROW)

#define GEDIT_TYPE_DOCUMENTS_DOCUMENT_ROW            (gedit_documents_document_row_get_type ())
#define GEDIT_DOCUMENTS_DOCUMENT_ROW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GEDIT_TYPE_DOCUMENTS_DOCUMENT_ROW, BeditDocumentsDocumentRow))
#define GEDIT_DOCUMENTS_DOCUMENT_ROW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GEDIT_TYPE_DOCUMENTS_DOCUMENT_ROW, BeditDocumentsDocumentRowClass))
#define GEDIT_IS_DOCUMENTS_DOCUMENT_ROW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GEDIT_TYPE_DOCUMENTS_DOCUMENT_ROW))
#define GEDIT_IS_DOCUMENTS_DOCUMENT_ROW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GEDIT_TYPE_DOCUMENTS_DOCUMENT_ROW))
#define GEDIT_DOCUMENTS_DOCUMENT_ROW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GEDIT_TYPE_DOCUMENTS_DOCUMENT_ROW, BeditDocumentsDocumentRowClass))

typedef struct _BeditDocumentsDocumentRowClass BeditDocumentsDocumentRowClass;

struct _BeditDocumentsDocumentRowClass
{
	GtkListBoxRowClass parent_class;
};

GType gedit_documents_document_row_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE (BeditDocumentsDocumentRow, gedit_documents_document_row, GTK_TYPE_LIST_BOX_ROW)

static GtkWidget *gedit_documents_document_row_new (BeditDocumentsPanel *panel,
                                                    BeditTab            *tab);
static GtkWidget *gedit_documents_group_row_new    (BeditDocumentsPanel *panel,
                                                    BeditNotebook       *notebook);

struct _BeditDocumentsPanel
{
	GtkBox              parent_instance;

	BeditWindow        *window;
	BeditMultiNotebook *mnb;
	GtkWidget          *listbox;

	guint               selection_changed_handler_id;
	guint               tab_switched_handler_id;
	gboolean            is_in_tab_switched;

	/* Flag to workaround first GroupRow selection at start ( we don't want to show it ) */
	gboolean            first_selection;
	GtkWidget          *current_selection;

	GtkAdjustment      *adjustment;

	guint               nb_row_notebook;
	guint               nb_row_tab;

	GtkTargetList      *source_targets;
	GtkWidget          *dnd_window;
	GtkWidget          *row_placeholder;
	guint               row_placeholder_index;
	guint               row_destination_index;
	GtkWidget          *drag_document_row;
	gint                row_source_row_offset;
	gint                document_row_height;
	gint                drag_document_row_x;
	gint                drag_document_row_y;
	gint                drag_root_x;
	gint                drag_root_y;
	gboolean            is_on_drag;
};

enum
{
	PROP_0,
	PROP_WINDOW,
	LAST_PROP
};

static GParamSpec *properties[LAST_PROP];

G_DEFINE_TYPE (BeditDocumentsPanel, gedit_documents_panel, GTK_TYPE_BOX)

static const GtkTargetEntry panel_targets [] = {
	{"GEDIT_DOCUMENTS_DOCUMENT_ROW", GTK_TARGET_SAME_APP, 0},
};

#define MAX_DOC_NAME_LENGTH 60

#define ROW_OUTSIDE_LISTBOX -1

static guint
get_nb_visible_rows (BeditDocumentsPanel *panel)
{
	guint nb = 0;

	if (panel->nb_row_notebook > 1)
	{
		nb += panel->nb_row_notebook;
	}

	nb += panel->nb_row_tab;

	return nb;
}

static guint
get_row_visible_index (BeditDocumentsPanel *panel,
                       GtkWidget           *searched_row)
{
	GList *children;
	GList *l;
	guint nb_notebook_row = 0;
	guint nb_tab_row = 0;

	children = gtk_container_get_children (GTK_CONTAINER (panel->listbox));

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

	if (panel->nb_row_notebook == 1)
	{
		nb_notebook_row = 0;
	}

	return nb_tab_row + nb_notebook_row - 1;
}

/* We do not grab focus on the row, so scroll it into view manually */
static void
make_row_visible (BeditDocumentsPanel *panel,
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

	adjustment_value = gtk_adjustment_get_value (panel->adjustment);
	adjustment_lower = gtk_adjustment_get_lower (panel->adjustment);
	adjustment_upper = gtk_adjustment_get_upper (panel->adjustment);
	adjustment_page_size = gtk_adjustment_get_page_size (panel->adjustment);

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

	gtk_adjustment_set_value (panel->adjustment, new_adjustment_value);
}

/* This function is a GCompareFunc to use with g_list_find_custom */
static gint
listbox_search_function (gconstpointer row,
                         gconstpointer item)
{
	BeditDocumentsGenericRow *generic_row = (BeditDocumentsGenericRow *)row;
	gpointer *searched_item = (gpointer *)generic_row->ref;

	return searched_item == item ? 0 : -1;
}

static GtkListBoxRow *
get_row_from_widget (BeditDocumentsPanel *panel,
                     GtkWidget           *widget)
{
	GList *children;
	GList *item;
	GtkListBoxRow *row;

	children = gtk_container_get_children (GTK_CONTAINER (panel->listbox));

	item = g_list_find_custom (children, widget, listbox_search_function);
	row = item ? item->data : NULL;

	g_list_free (children);

	return row;
}

static void
row_select (BeditDocumentsPanel *panel,
            GtkListBox          *listbox,
            GtkListBoxRow       *row)
{
	GtkListBoxRow *selected_row = gtk_list_box_get_selected_row (listbox);

	if (row != selected_row)
	{
		g_signal_handler_block (listbox, panel->selection_changed_handler_id);
		gtk_list_box_select_row (listbox, row);
		g_signal_handler_unblock (listbox, panel->selection_changed_handler_id);
	}

	panel->current_selection = GTK_WIDGET (row);
	make_row_visible (panel, GTK_WIDGET (row));
}

static void
insert_row (BeditDocumentsPanel *panel,
            GtkListBox          *listbox,
            GtkWidget           *row,
            gint                 position)
{
	g_signal_handler_block (listbox, panel->selection_changed_handler_id);
	gtk_list_box_insert (listbox, row, position);
	g_signal_handler_unblock (listbox, panel->selection_changed_handler_id);
}

static void
select_active_tab (BeditDocumentsPanel *panel)
{
	BeditNotebook *notebook;
	gboolean have_tabs;
	BeditTab *tab;

	notebook = gedit_multi_notebook_get_active_notebook (panel->mnb);
	have_tabs = gtk_notebook_get_n_pages (GTK_NOTEBOOK (notebook)) > 0;
	tab = gedit_multi_notebook_get_active_tab (panel->mnb);

	if (notebook != NULL && tab != NULL && have_tabs)
	{
		GtkListBoxRow *row = get_row_from_widget (panel, GTK_WIDGET (tab));

		if (row)
		{
			row_select (panel, GTK_LIST_BOX (panel->listbox), row);
		}
	}
}

static GtkListBoxRow *
get_first_notebook_found (BeditDocumentsPanel *panel)
{
	GList *children;
	GList *l;
	GtkListBoxRow *row = NULL;

	children = gtk_container_get_children (GTK_CONTAINER (panel->listbox));

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
multi_notebook_tab_switched (BeditMultiNotebook  *mnb,
                             BeditNotebook       *old_notebook,
                             BeditTab            *old_tab,
                             BeditNotebook       *new_notebook,
                             BeditTab            *new_tab,
                             BeditDocumentsPanel *panel)
{
	gedit_debug (DEBUG_PANEL);

	if (!_gedit_window_is_removing_tabs (panel->window) &&
	    panel->is_in_tab_switched == FALSE)
	{
		GtkListBoxRow *row;

		panel->is_in_tab_switched = TRUE;

		row = get_row_from_widget (panel, GTK_WIDGET (new_tab));

		if (row)
		{
			row_select (panel, GTK_LIST_BOX (panel->listbox), row);
		}

		panel->is_in_tab_switched = FALSE;
	}
}

static void
group_row_set_notebook_name (GtkWidget *row)
{
	BeditNotebook *notebook;
	BeditMultiNotebook *mnb;
	guint num;
	gchar *name;
	BeditDocumentsGroupRow *group_row = GEDIT_DOCUMENTS_GROUP_ROW (row);

	notebook = GEDIT_NOTEBOOK (group_row->ref);

	mnb = group_row->panel->mnb;
	num = gedit_multi_notebook_get_notebook_num (mnb, notebook);

	name = g_strdup_printf (_("Tab Group %i"), num + 1);

	gtk_label_set_text (GTK_LABEL (group_row->label), name);

	g_free (name);
}

static void
group_row_update_names (BeditDocumentsPanel *panel,
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
group_row_refresh_visibility (BeditDocumentsPanel *panel)
{
	gboolean notebook_is_unique;
	GtkWidget *first_group_row;

	notebook_is_unique = gedit_multi_notebook_get_n_notebooks (panel->mnb) <= 1;
	first_group_row = GTK_WIDGET (get_first_notebook_found (panel));

	gtk_widget_set_no_show_all (first_group_row, notebook_is_unique);
	gtk_widget_set_visible (first_group_row, !notebook_is_unique);
}

static gchar *
doc_get_name (BeditDocument *doc)
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
document_row_sync_tab_name_and_icon (BeditTab   *tab,
                                     GParamSpec *pspec,
                                     GtkWidget  *row)
{
	BeditDocumentsDocumentRow *document_row = GEDIT_DOCUMENTS_DOCUMENT_ROW (row);
	BeditDocument *doc;
	GtkSourceFile *file;
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

	file = gedit_document_get_file (doc);

	/* The status has as separate label to prevent ellipsizing */
	if (!gtk_source_file_is_readonly (file))
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
refresh_notebook (BeditDocumentsPanel *panel,
                  BeditNotebook       *notebook)
{
	GList *tabs;
	GList *l;

	tabs = gtk_container_get_children (GTK_CONTAINER (notebook));

	for (l = tabs; l != NULL; l = g_list_next (l))
	{
		GtkWidget *row;

		row = gedit_documents_document_row_new (panel, GEDIT_TAB (l->data));
		insert_row (panel, GTK_LIST_BOX (panel->listbox), row, -1);
		panel->nb_row_tab += 1;
	}

	g_list_free (tabs);
}

static void
refresh_notebook_foreach (BeditNotebook       *notebook,
                          BeditDocumentsPanel *panel)
{
	GtkWidget *row;

	row = gedit_documents_group_row_new (panel, notebook);
	insert_row (panel, GTK_LIST_BOX (panel->listbox), row, -1);
	panel->nb_row_notebook += 1;

	group_row_refresh_visibility (panel);
	refresh_notebook (panel, notebook);
}

static void
refresh_list (BeditDocumentsPanel *panel)
{
	GList *children;
	GList *l;

	/* Clear the listbox */
	children = gtk_container_get_children (GTK_CONTAINER (panel->listbox));

	for (l = children; l != NULL; l = g_list_next (l))
	{
		BeditDocumentsGenericRow *row = l->data;

		if (GEDIT_IS_DOCUMENTS_DOCUMENT_ROW (row))
		{
			BeditTab *tab = GEDIT_TAB (row->ref);
			g_signal_handlers_disconnect_matched (tab,
			                                      G_SIGNAL_MATCH_FUNC,
			                                      0,
			                                      0,
			                                      NULL,
			                                      G_CALLBACK (document_row_sync_tab_name_and_icon),
			                                      NULL);
		}

		gtk_widget_destroy (GTK_WIDGET (row));
	}

	g_list_free (children);

	gedit_multi_notebook_foreach_notebook (panel->mnb,
	                                       (GtkCallback)refresh_notebook_foreach,
	                                       panel);
	select_active_tab (panel);
}

static void
multi_notebook_tab_removed (BeditMultiNotebook  *mnb,
                            BeditNotebook       *notebook,
                            BeditTab            *tab,
                            BeditDocumentsPanel *panel)
{
	GtkListBoxRow *row;

	gedit_debug (DEBUG_PANEL);

	row = get_row_from_widget (panel, GTK_WIDGET (tab));

	/* Disconnect before destroy it so document_row_sync_tab_name_and_icon()
	 * don't get invalid data */
	g_signal_handlers_disconnect_by_func (GEDIT_DOCUMENTS_DOCUMENT_ROW (row)->ref,
	                                      G_CALLBACK (document_row_sync_tab_name_and_icon),
	                                      row);

	gtk_widget_destroy (GTK_WIDGET (row));
	panel->nb_row_tab -= 1;
}

static gint
get_dest_position_for_tab (BeditDocumentsPanel *panel,
                           BeditNotebook       *notebook,
                           BeditTab            *tab)
{
	gint page_num;
	GList *children;
	GList *item;
	gint res = -1;

	/* Get tab's position in notebook and notebook's position in GtkListBox
	 * then return future tab's position in GtkListBox */

	page_num = gtk_notebook_page_num (GTK_NOTEBOOK (notebook), GTK_WIDGET (tab));

	children = gtk_container_get_children (GTK_CONTAINER (panel->listbox));
	item = g_list_find_custom (children, notebook, listbox_search_function);

	if (item)
	{
		res = 1 + page_num + g_list_position (children, item);
	}

	g_list_free (children);

	return res;
}

static void
multi_notebook_tab_added (BeditMultiNotebook  *mnb,
                          BeditNotebook       *notebook,
                          BeditTab            *tab,
                          BeditDocumentsPanel *panel)
{
	gint position;
	GtkWidget *row;

	gedit_debug (DEBUG_PANEL);

	position = get_dest_position_for_tab (panel, notebook, tab);

	if (position == -1)
	{
		panel->nb_row_tab = 0;
		panel->nb_row_notebook = 0;

		refresh_list (panel);
	}
	else
	{
		/* Add a new tab's row to the listbox */
		row = gedit_documents_document_row_new (panel, tab);
		insert_row (panel, GTK_LIST_BOX (panel->listbox), row, position);

		panel->nb_row_tab += 1;

		if (tab == gedit_multi_notebook_get_active_tab (mnb))
		{
			row_select (panel, GTK_LIST_BOX (panel->listbox), GTK_LIST_BOX_ROW (row));
		}
	}
}

static void
multi_notebook_notebook_removed (BeditMultiNotebook  *mnb,
                                 BeditNotebook       *notebook,
                                 BeditDocumentsPanel *panel)
{
	GtkListBoxRow *row;

	gedit_debug (DEBUG_PANEL);

	row = get_row_from_widget (panel, GTK_WIDGET (notebook));
	gtk_container_remove (GTK_CONTAINER (panel->listbox), GTK_WIDGET (row));

	panel->nb_row_notebook -= 1;

	group_row_refresh_visibility (panel);
	group_row_update_names (panel, panel->listbox);
}

static void
row_move (BeditDocumentsPanel *panel,
          BeditNotebook       *notebook,
          GtkWidget           *tab,
          GtkWidget           *row)
{
	gint position;

	g_object_ref (row);

	gtk_container_remove (GTK_CONTAINER (panel->listbox), GTK_WIDGET (row));
	position = get_dest_position_for_tab (panel, notebook, GEDIT_TAB (tab));

	g_signal_handler_block (panel->listbox, panel->selection_changed_handler_id);

	gtk_list_box_insert (GTK_LIST_BOX (panel->listbox), row, position);
	g_object_unref (row);

	g_signal_handler_unblock (GTK_LIST_BOX (panel->listbox), panel->selection_changed_handler_id);
}

static void
multi_notebook_tabs_reordered (BeditMultiNotebook  *mnb,
                               BeditNotebook       *notebook,
                               GtkWidget           *page,
                               gint                 page_num,
                               BeditDocumentsPanel *panel)
{
	GtkListBoxRow *row;

	gedit_debug (DEBUG_PANEL);

	row = get_row_from_widget (panel, GTK_WIDGET (page));

	row_move (panel, notebook, page, GTK_WIDGET (row));

	row_select (panel, GTK_LIST_BOX (panel->listbox), GTK_LIST_BOX_ROW (row));
}

static void
set_window (BeditDocumentsPanel *panel,
            BeditWindow         *window)
{
	panel->window = g_object_ref (window);
	panel->mnb = GEDIT_MULTI_NOTEBOOK (_gedit_window_get_multi_notebook (window));

	g_signal_connect (panel->mnb,
	                  "notebook-removed",
	                  G_CALLBACK (multi_notebook_notebook_removed),
	                  panel);
	g_signal_connect (panel->mnb,
	                  "tab-added",
	                  G_CALLBACK (multi_notebook_tab_added),
	                  panel);
	g_signal_connect (panel->mnb,
	                  "tab-removed",
	                  G_CALLBACK (multi_notebook_tab_removed),
	                  panel);
	g_signal_connect (panel->mnb,
	                  "page-reordered",
	                  G_CALLBACK (multi_notebook_tabs_reordered),
	                  panel);

	panel->tab_switched_handler_id = g_signal_connect (panel->mnb,
                                                           "switch-tab",
                                                           G_CALLBACK (multi_notebook_tab_switched),
                                                           panel);

	panel->first_selection = TRUE;

	refresh_list (panel);
	group_row_refresh_visibility (panel);
}

static void
listbox_selection_changed (GtkListBox          *listbox,
                           GtkListBoxRow       *row,
                           BeditDocumentsPanel *panel)
{
	if (row == NULL)
	{
		/* No selection on document panel */
		return;
	}

	/* When the window is shown, the first notebook row is selected
	 * and therefore also shown - we don't want this */

	if (panel->first_selection)
	{
		panel->first_selection = FALSE;
		group_row_refresh_visibility (panel);
	}

	g_signal_handler_block (panel->mnb, panel->tab_switched_handler_id);

	if (GEDIT_IS_DOCUMENTS_DOCUMENT_ROW (row))
	{
		gedit_multi_notebook_set_active_tab (panel->mnb,
		                                     GEDIT_TAB (GEDIT_DOCUMENTS_DOCUMENT_ROW (row)->ref));

		panel->current_selection = GTK_WIDGET (row);
	}
	else if (GEDIT_IS_DOCUMENTS_GROUP_ROW (row) && panel->current_selection)
	{
		row_select (panel,
		            GTK_LIST_BOX (panel->listbox),
		            GTK_LIST_BOX_ROW (panel->current_selection));

	}
	else
	{
		g_assert_not_reached ();
	}

	g_signal_handler_unblock (panel->mnb, panel->tab_switched_handler_id);
}

static void
gedit_documents_panel_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
	BeditDocumentsPanel *panel = GEDIT_DOCUMENTS_PANEL (object);

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
	BeditDocumentsPanel *panel = GEDIT_DOCUMENTS_PANEL (object);

	switch (prop_id)
	{
		case PROP_WINDOW:
			g_value_set_object (value, panel->window);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gedit_documents_panel_finalize (GObject *object)
{
	BeditDocumentsPanel *panel = GEDIT_DOCUMENTS_PANEL (object);

	g_signal_handlers_disconnect_by_func (panel->mnb,
	                                      G_CALLBACK (multi_notebook_notebook_removed),
	                                      panel);
	g_signal_handlers_disconnect_by_func (panel->mnb,
	                                      G_CALLBACK (multi_notebook_tab_added),
	                                      panel);
	g_signal_handlers_disconnect_by_func (panel->mnb,
	                                      G_CALLBACK (multi_notebook_tab_removed),
	                                      panel);
	g_signal_handlers_disconnect_by_func (panel->mnb,
	                                      G_CALLBACK (multi_notebook_tabs_reordered),
	                                      panel);
	g_signal_handlers_disconnect_by_func (panel->mnb,
	                                      G_CALLBACK (multi_notebook_tab_switched),
	                                      panel);

	G_OBJECT_CLASS (gedit_documents_panel_parent_class)->finalize (object);
}

static void
gedit_documents_panel_dispose (GObject *object)
{
	BeditDocumentsPanel *panel = GEDIT_DOCUMENTS_PANEL (object);

	g_clear_object (&panel->window);

	if (panel->source_targets)
	{
		gtk_target_list_unref (panel->source_targets);
		panel->source_targets = NULL;
	}

	G_OBJECT_CLASS (gedit_documents_panel_parent_class)->dispose (object);
}

static GtkWidget *
create_placeholder_row (gint height)
{
	GtkStyleContext *context;

	GtkWidget *placeholder_row = gtk_list_box_row_new ();

	context = gtk_widget_get_style_context (placeholder_row);
	gtk_style_context_add_class (context, "gedit-document-panel-placeholder-row");

	gtk_widget_set_size_request (placeholder_row, -1, height);

	return placeholder_row;
}

static void
panel_on_drag_begin (GtkWidget      *widget,
                     GdkDragContext *context)
{
	BeditDocumentsPanel *panel = GEDIT_DOCUMENTS_PANEL (widget);
	GtkWidget *drag_document_row;
	GtkAllocation allocation;
	const gchar *name;
	GtkWidget *label;
	gint width, height;
	GtkWidget *image_box;
	GtkWidget *box;
	GtkStyleContext *style_context;

	drag_document_row = panel->drag_document_row;
	gtk_widget_get_allocation (drag_document_row, &allocation);
	gtk_widget_hide (drag_document_row);

	panel->document_row_height = allocation.height;

	name = gtk_label_get_label (GTK_LABEL (GEDIT_DOCUMENTS_DOCUMENT_ROW (drag_document_row)->label));

	label = gtk_label_new (NULL);
	gtk_label_set_markup (GTK_LABEL (label), name);
	gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_widget_set_valign (label, GTK_ALIGN_CENTER);

	gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &width, &height);
	image_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_set_size_request (image_box, width, height);

	box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);

	gtk_box_pack_start (GTK_BOX (box), image_box, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);

	panel->dnd_window = gtk_window_new (GTK_WINDOW_POPUP);
	gtk_widget_set_size_request (panel->dnd_window, allocation.width, allocation.height);
	gtk_window_set_screen (GTK_WINDOW (panel->dnd_window),
	                       gtk_widget_get_screen (drag_document_row));

	style_context = gtk_widget_get_style_context (panel->dnd_window);
	gtk_style_context_add_class (style_context, "gedit-document-panel-dragged-row");

	gtk_container_add (GTK_CONTAINER (panel->dnd_window), box);
	gtk_widget_show_all (panel->dnd_window);
	gtk_widget_set_opacity (panel->dnd_window, 0.8);

	gtk_drag_set_icon_widget (context,
	                          panel->dnd_window,
	                          panel->drag_document_row_x,
	                          panel->drag_document_row_y);
}

static gboolean
panel_on_drag_motion (GtkWidget      *widget,
                      GdkDragContext *context,
                      gint            x,
                      gint            y,
                      guint           time)
{
	BeditDocumentsPanel *panel = GEDIT_DOCUMENTS_PANEL (widget);
	BeditDocumentsGenericRow *generic_row;
	GtkWidget *source_panel;
	gint dest_x, dest_y;
	gint row_placeholder_index;
	gint row_index;

	GdkAtom target = gtk_drag_dest_find_target (widget, context, NULL);

	if (target != gdk_atom_intern_static_string ("GEDIT_DOCUMENTS_DOCUMENT_ROW"))
	{
		gdk_drag_status (context, 0, time);
		return FALSE;
	}

	gtk_widget_translate_coordinates (widget, panel->listbox,
	                                  x, y,
	                                  &dest_x, &dest_y);

	generic_row = (BeditDocumentsGenericRow *)gtk_list_box_get_row_at_y (GTK_LIST_BOX (panel->listbox), dest_y);
	source_panel = gtk_drag_get_source_widget (context);

	if (!panel->row_placeholder)
	{
		if (!generic_row)
		{
			/* We don't have a row height to use, so use the source one */
			panel->document_row_height = GEDIT_DOCUMENTS_PANEL (source_panel)->document_row_height;
		}
		else
		{
			GtkAllocation allocation;

			gtk_widget_get_allocation (GTK_WIDGET (generic_row), &allocation);
			panel->document_row_height = allocation.height;
		}

		panel->row_placeholder = create_placeholder_row (panel->document_row_height);
		gtk_widget_show (panel->row_placeholder);
		g_object_ref_sink (panel->row_placeholder);
	}
	else if (GTK_WIDGET (generic_row) == panel->row_placeholder)
	{
		/* cursor on placeholder */
		gdk_drag_status (context, GDK_ACTION_MOVE, time);

		return TRUE;
	}

	if (!generic_row)
	{
		/* cursor on empty space => put the placeholder at end of list */
		GList *children = gtk_container_get_children (GTK_CONTAINER (panel->listbox));

		row_placeholder_index = g_list_length (children);
		g_list_free (children);
	}
	else
	{
		row_index = gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (generic_row));

		gtk_widget_translate_coordinates (widget, GTK_WIDGET (generic_row),
		                                  x, y,
		                                  &dest_x, &dest_y);

		if (dest_y <= panel->document_row_height / 2 && row_index > 0)
		{
			row_placeholder_index = row_index;
		}
		else
		{
			row_placeholder_index = row_index + 1;
		}
	}

	if (source_panel == widget)
	{
		/* Adjustment because of hidden source row */
		gint source_row_index = gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (panel->drag_document_row));
		panel->row_source_row_offset = source_row_index <  row_placeholder_index ? -1 : 0;
	}

	if (panel->row_placeholder_index != row_placeholder_index)
	{
		if (panel->row_placeholder_index != ROW_OUTSIDE_LISTBOX)
		{
			gtk_container_remove (GTK_CONTAINER (panel->listbox),
			                      panel->row_placeholder);

			if (panel->row_placeholder_index < row_placeholder_index)
			{
				/* Ajustment because of existing placeholder row */
				row_placeholder_index -= 1;
			}
		}

		panel->row_destination_index = panel->row_placeholder_index = row_placeholder_index;

		gtk_list_box_insert (GTK_LIST_BOX (panel->listbox),
		                     panel->row_placeholder,
		                     panel->row_placeholder_index);
	}

	gdk_drag_status (context, GDK_ACTION_MOVE, time);

	return TRUE;
}

static void
panel_on_drag_leave (GtkWidget      *widget,
                     GdkDragContext *context,
                     guint           time)
{
	BeditDocumentsPanel *panel = GEDIT_DOCUMENTS_PANEL (widget);

	if (panel->row_placeholder_index != ROW_OUTSIDE_LISTBOX)
	{
		gtk_container_remove (GTK_CONTAINER (panel->listbox), panel->row_placeholder);
		panel->row_placeholder_index = ROW_OUTSIDE_LISTBOX;
	}
}

static gboolean
panel_on_drag_drop (GtkWidget        *widget,
                    GdkDragContext   *context,
                    gint              x,
                    gint              y,
                    guint             time)
{
	BeditDocumentsPanel *panel = GEDIT_DOCUMENTS_PANEL (widget);

	GdkAtom target = gtk_drag_dest_find_target (widget, context, NULL);
	GtkWidget *source_widget = gtk_drag_get_source_widget (context);

	if (GEDIT_IS_DOCUMENTS_PANEL (source_widget))
	{
		gtk_widget_show (GEDIT_DOCUMENTS_PANEL (source_widget)->drag_document_row);
	}

	if (target == gdk_atom_intern_static_string ("GEDIT_DOCUMENTS_DOCUMENT_ROW"))
	{
		gtk_drag_get_data (widget, context, target, time);
		return TRUE;
	}

	panel->row_placeholder_index = ROW_OUTSIDE_LISTBOX;
	return FALSE;
}

static void
panel_on_drag_data_get (GtkWidget        *widget,
                        GdkDragContext   *context,
                        GtkSelectionData *data,
                        guint             info,
                        guint             time)
{
	BeditDocumentsPanel *panel = GEDIT_DOCUMENTS_PANEL (widget);
	GdkAtom target = gtk_selection_data_get_target (data);
	GdkAtom result;

	if (target == gdk_atom_intern_static_string ("GEDIT_DOCUMENTS_DOCUMENT_ROW"))
	{
		gtk_selection_data_set (data,
		                        target,
		                        8,
		                        (void*)&panel->drag_document_row,
		                        sizeof (gpointer));
		return;
	}

	result = gtk_drag_dest_find_target (widget, context, panel->source_targets);

	if (result != GDK_NONE)
	{
		BeditTab *tab;
		BeditDocument *doc;
		gchar *full_name;

		tab = GEDIT_TAB (GEDIT_DOCUMENTS_DOCUMENT_ROW (panel->drag_document_row)->ref);
		doc = gedit_tab_get_document (tab);

		if (!gedit_document_is_untitled (doc))
		{
			GtkSourceFile *file = gedit_document_get_file (doc);
			GFile *location = gtk_source_file_get_location (file);
			full_name = g_file_get_parse_name (location);

			gtk_selection_data_set (data,
			                        target,
			                        8,
			                        (guchar *)full_name,
			                        strlen (full_name));
			g_free (full_name);
		}
	}

	gtk_widget_show (panel->drag_document_row);
}

static BeditNotebook *
get_notebook_and_position_from_document_row (BeditDocumentsPanel *panel,
                                             gint                 row_index,
                                             gint                *position)
{
	GList *l;
	gint index = 0;
	BeditDocumentsGroupRow *row;

	GList *children = gtk_container_get_children (GTK_CONTAINER (panel->listbox));
	gint nb_elements = g_list_length (children);

	if (nb_elements == 1)
	{
		row = children->data;
	}
	else
	{
		l = g_list_nth (children, row_index - 1);

		while (TRUE)
		{
			row = l->data;

			if (GEDIT_IS_DOCUMENTS_GROUP_ROW (row))
			{
				break;
			}

			l = g_list_previous (l);
			index += 1;
		}
	}

	g_list_free (children);

	*position = index;
	return GEDIT_NOTEBOOK (row->ref);
}

static void
panel_on_drag_data_received (GtkWidget        *widget,
                             GdkDragContext   *context,
                             gint              x,
                             gint              y,
                             GtkSelectionData *data,
                             guint             info,
                             guint             time)
{
	BeditDocumentsPanel *panel = GEDIT_DOCUMENTS_PANEL (widget);
	BeditDocumentsPanel *source_panel = NULL;

	GtkWidget *source_widget = gtk_drag_get_source_widget (context);

	if (GEDIT_IS_DOCUMENTS_PANEL (source_widget))
	{
		source_panel = GEDIT_DOCUMENTS_PANEL (source_widget);
	}

	GtkWidget **source_row = (void*) gtk_selection_data_get_data (data);

	if (source_panel &&
	    gtk_selection_data_get_target (data) == gdk_atom_intern_static_string ("GEDIT_DOCUMENTS_DOCUMENT_ROW"))
	{
		gint source_index = gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (*source_row));

		/* And finally, we can move the row */
		if (source_panel != panel ||
		    (panel->row_destination_index != source_index &&
		     panel->row_destination_index != source_index + 1))
		{
			BeditNotebook *old_notebook, *new_notebook;
			gint position;

			BeditTab *tab = GEDIT_TAB (GEDIT_DOCUMENTS_DOCUMENT_ROW (*source_row)->ref);

			old_notebook = gedit_multi_notebook_get_notebook_for_tab (source_panel->mnb, tab);
			new_notebook = get_notebook_and_position_from_document_row (panel,
			                                                            panel->row_destination_index,
			                                                            &position);
			if (old_notebook == new_notebook)
			{
				gtk_widget_show (*source_row);

				gtk_notebook_reorder_child (GTK_NOTEBOOK (new_notebook),
				                            GTK_WIDGET (tab),
				                            position + panel->row_source_row_offset);
			}
			else
			{
				gedit_notebook_move_tab (old_notebook, new_notebook, tab, position);
			}

			if (tab != gedit_multi_notebook_get_active_tab (panel->mnb))
			{
				g_signal_handler_block (panel->mnb, panel->tab_switched_handler_id);
				gedit_multi_notebook_set_active_tab (panel->mnb, tab);
				g_signal_handler_unblock (panel->mnb, panel->tab_switched_handler_id);
			}
		}

		gtk_drag_finish (context, TRUE, FALSE, time);
	}
	else
	{
		gtk_drag_finish (context, FALSE, FALSE, time);
	}

	panel->row_destination_index = panel->row_placeholder_index = ROW_OUTSIDE_LISTBOX;

	if (panel->row_placeholder)
	{
		gtk_widget_destroy (panel->row_placeholder);
		panel->row_placeholder = NULL;
	}
}

static void
panel_on_drag_end (GtkWidget      *widget,
                   GdkDragContext *context)
{
	BeditDocumentsPanel *panel = GEDIT_DOCUMENTS_PANEL (widget);

	panel->drag_document_row = NULL;
	panel->is_on_drag = FALSE;

	gtk_widget_destroy (panel->dnd_window);
	panel->dnd_window = NULL;
}

static gboolean
panel_on_drag_failed (GtkWidget      *widget,
                      GdkDragContext *context,
                      GtkDragResult   result)
{
	GtkWidget *source_widget = gtk_drag_get_source_widget (context);

	if (GEDIT_IS_DOCUMENTS_PANEL (source_widget))
	{
		gtk_widget_show (GEDIT_DOCUMENTS_PANEL (source_widget)->drag_document_row);
	}

	return FALSE;
}

static gboolean
panel_on_motion_notify (GtkWidget      *widget,
                        GdkEventMotion *event)
{
	BeditDocumentsPanel *panel = GEDIT_DOCUMENTS_PANEL (widget);

	if (panel->drag_document_row == NULL || panel->is_on_drag)
	{
		return FALSE;
	}

	if (!(event->state & GDK_BUTTON1_MASK))
	{
		panel->drag_document_row = NULL;

		return FALSE;
	}

	if (gtk_drag_check_threshold (widget,
	                              panel->drag_root_x, panel->drag_root_y,
	                              event->x_root, event->y_root))
	{
		panel->is_on_drag = TRUE;

		gtk_drag_begin_with_coordinates (widget, panel->source_targets, GDK_ACTION_MOVE,
		                                 GDK_BUTTON_PRIMARY, (GdkEvent*)event,
		                                 -1, -1);
	}

	return FALSE;
}

static void
gedit_documents_panel_class_init (BeditDocumentsPanelClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->finalize = gedit_documents_panel_finalize;
	object_class->dispose = gedit_documents_panel_dispose;
	object_class->get_property = gedit_documents_panel_get_property;
	object_class->set_property = gedit_documents_panel_set_property;

	widget_class->motion_notify_event = panel_on_motion_notify;

	widget_class->drag_begin = panel_on_drag_begin;
	widget_class->drag_end = panel_on_drag_end;
	widget_class->drag_failed = panel_on_drag_failed;
	widget_class->drag_motion = panel_on_drag_motion;
	widget_class->drag_leave = panel_on_drag_leave;
	widget_class->drag_drop = panel_on_drag_drop;
	widget_class->drag_data_get = panel_on_drag_data_get;
	widget_class->drag_data_received = panel_on_drag_data_received;

	properties[PROP_WINDOW] =
		g_param_spec_object ("window",
		                     "Window",
		                     "The BeditWindow this BeditDocumentsPanel is associated with",
		                     GEDIT_TYPE_WINDOW,
		                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
gedit_documents_panel_init (BeditDocumentsPanel *panel)
{
	GtkWidget *sw;
	GtkStyleContext *context;

	gedit_debug (DEBUG_PANEL);

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
	panel->listbox = gtk_list_box_new ();

	gtk_container_add (GTK_CONTAINER (sw), panel->listbox);

	panel->adjustment = gtk_list_box_get_adjustment (GTK_LIST_BOX (panel->listbox));

	/* Disable focus so it doesn't steal focus each time from the view */
	gtk_widget_set_can_focus (panel->listbox, FALSE);

	/* Css style */
	context = gtk_widget_get_style_context (panel->listbox);
	gtk_style_context_add_class (context, "gedit-document-panel");

	panel->selection_changed_handler_id = g_signal_connect (panel->listbox,
	                                                        "row-selected",
	                                                        G_CALLBACK (listbox_selection_changed),
	                                                        panel);
	panel->is_in_tab_switched = FALSE;
	panel->current_selection = NULL;
	panel->nb_row_notebook = 0;
	panel->nb_row_tab = 0;

	/* Drag and drop support */
	panel->source_targets = gtk_target_list_new (panel_targets, G_N_ELEMENTS (panel_targets));
	gtk_target_list_add_text_targets (panel->source_targets, 0);

	gtk_drag_dest_set (GTK_WIDGET (panel), 0,
	                   panel_targets, G_N_ELEMENTS (panel_targets),
	                   GDK_ACTION_MOVE);

	gtk_drag_dest_set_track_motion (GTK_WIDGET (panel), TRUE);

	panel->drag_document_row = NULL;
	panel->row_placeholder = NULL;
	panel->row_placeholder_index = ROW_OUTSIDE_LISTBOX;
	panel->row_destination_index = ROW_OUTSIDE_LISTBOX;
	panel->row_source_row_offset = 0;
	panel->is_on_drag = FALSE;
}

GtkWidget *
gedit_documents_panel_new (BeditWindow *window)
{
	g_return_val_if_fail (GEDIT_IS_WINDOW (window), NULL);

	return g_object_new (GEDIT_TYPE_DOCUMENTS_PANEL,
	                     "window", window,
	                     NULL);
}

static void
row_on_close_button_clicked (GtkWidget *close_button,
                             GtkWidget *row)
{
	BeditDocumentsGenericRow *generic_row = (BeditDocumentsGenericRow *)row;
	BeditWindow *window = generic_row->panel->window;
	GtkWidget *ref;

	if (GEDIT_IS_DOCUMENTS_GROUP_ROW (row))
	{
		ref = GEDIT_DOCUMENTS_GROUP_ROW (row)->ref;
		_gedit_cmd_file_close_notebook (window, GEDIT_NOTEBOOK (ref));
	}
	else if (GEDIT_IS_DOCUMENTS_DOCUMENT_ROW (row))
	{
		ref = GEDIT_DOCUMENTS_DOCUMENT_ROW (row)->ref;
		_gedit_cmd_file_close_tab (GEDIT_TAB (ref), window);
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
	if (gdk_event_get_event_type ((GdkEvent *)event) == GDK_BUTTON_PRESS &&
	    GEDIT_IS_DOCUMENTS_DOCUMENT_ROW (row))
	{
		BeditDocumentsDocumentRow *document_row = GEDIT_DOCUMENTS_DOCUMENT_ROW (row);
		BeditDocumentsPanel *panel = document_row->panel;

		if (event->button == GDK_BUTTON_PRIMARY)
		{
			/* memorize row and clicked position for possible drag'n drop */
			panel->drag_document_row = row;
			panel->drag_document_row_x = (gint)event->x;
			panel->drag_document_row_y = (gint)event->y;

			panel->drag_root_x = event->x_root;
			panel->drag_root_y = event->y_root;

			return FALSE;
		}

		panel->drag_document_row = NULL;

		if (gdk_event_triggers_context_menu ((GdkEvent *)event))
		{
			BeditWindow *window = panel->window;
			BeditTab *tab = GEDIT_TAB (document_row->ref);
			GtkWidget *menu = gedit_notebook_popup_menu_new (window, tab);

			g_signal_connect (menu,
					  "selection-done",
					  G_CALLBACK (gtk_widget_destroy),
					  NULL);

			gtk_menu_popup_at_pointer (GTK_MENU (menu), (GdkEvent *)event);

			return TRUE;
		}
	}

	return FALSE;
}

static void
document_row_create_header (GtkWidget *row)
{
	BeditDocumentsDocumentRow *document_row = GEDIT_DOCUMENTS_DOCUMENT_ROW (row);
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
	BeditDocumentsGenericRow *generic_row = (BeditDocumentsGenericRow *)row;
	GtkWidget *event_box;
	GtkStyleContext *context;
	GtkWidget *image;
	GIcon *icon;

	gedit_debug (DEBUG_PANEL);

	event_box = gtk_event_box_new ();
	generic_row->box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);

	gtk_container_add (GTK_CONTAINER (event_box), generic_row->box);

	generic_row->label = gtk_label_new (NULL);
	gtk_label_set_ellipsize (GTK_LABEL (generic_row->label), PANGO_ELLIPSIZE_END);
	gtk_widget_set_halign (generic_row->label, GTK_ALIGN_START);
	gtk_widget_set_valign (generic_row->label, GTK_ALIGN_CENTER);

	generic_row->status_label = gtk_label_new (NULL);
	gtk_widget_set_halign (generic_row->status_label, GTK_ALIGN_END);
	gtk_widget_set_valign (generic_row->status_label, GTK_ALIGN_CENTER);

	generic_row->close_button = GTK_WIDGET (g_object_new (GTK_TYPE_BUTTON,
	                                                      "relief", GTK_RELIEF_NONE,
	                                                      "focus-on-click", FALSE,
	                                                      NULL));

	context = gtk_widget_get_style_context (generic_row->close_button);
	gtk_style_context_add_class (context, "flat");
	gtk_style_context_add_class (context, "small-button");

	icon = g_themed_icon_new_with_default_fallbacks ("window-close-symbolic");
	image = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_MENU);
	gtk_widget_show (image);
	g_object_unref (icon);

	gtk_container_add (GTK_CONTAINER (generic_row->close_button), image);

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
	BeditDocumentsGenericRow *generic_row = (BeditDocumentsGenericRow *)row;
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

/* Bedit Document Row */
static void
gedit_documents_document_row_class_init (BeditDocumentsDocumentRowClass *klass)
{
}

static void
gedit_documents_document_row_init (BeditDocumentsDocumentRow *row)
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

	gtk_widget_set_can_focus (GTK_WIDGET (row), FALSE);
}

/* Bedit Group Row */
static void
gedit_documents_group_row_class_init (BeditDocumentsGroupRowClass *klass)
{
}

static void
gedit_documents_group_row_init (BeditDocumentsGroupRow *row)
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

	gtk_widget_set_can_focus (GTK_WIDGET (row), FALSE);
}

static GtkWidget *
gedit_documents_document_row_new (BeditDocumentsPanel *panel,
                                  BeditTab *tab)
{
	BeditDocumentsDocumentRow *row;

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
gedit_documents_group_row_new (BeditDocumentsPanel *panel,
                               BeditNotebook       *notebook)
{
	BeditDocumentsGroupRow *row;

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
