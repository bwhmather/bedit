/*
 * gedit-open-document-selector.c
 * This file is part of gedit
 *
 * Copyright (C) 2014 - Sébastien Lafargue
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

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <glib/gstdio.h>
#include <gio/gio.h>

#include "gedit-open-document-selector.h"
#include "gedit-recent.h"
#include "gedit-utils.h"
#include "gedit-window.h"
#include "gedit-debug.h"

struct _GeditOpenDocumentSelectorPrivate
{
	GtkWidget *open_button;
	GtkWidget *listbox;
	GtkWidget *scrolled_window;

	guint populate_listbox_id;
	gulong recent_manager_changed_id;

	gint row_height;

	GeditRecentConfiguration recent_config;
};

/* Signals */
enum
{
	RECENT_FILE_ACTIVATED,
	LAST_SIGNAL
};

#define OPEN_DOCUMENT_SELECTOR_ROW_BOX_SPACING 4
#define OPEN_DOCUMENT_SELECTOR_WIDTH 400
#define OPEN_DOCUMENT_SELECTOR_MAX_VISIBLE_ROWS 10

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (GeditOpenDocumentSelector, gedit_open_document_selector, GTK_TYPE_BOX)


static GtkWidget *
create_row_layout (GeditOpenDocumentSelector *open_document_selector,
                   const gchar               *name,
                   const gchar               *path)
{
	GtkWidget *row;
	GtkWidget *vbox;
	GtkWidget *name_label;
	GtkWidget *path_label;
	GtkStyleContext *context;

	row = gtk_list_box_row_new ();

	context = gtk_widget_get_style_context (GTK_WIDGET (row));
	gtk_style_context_add_class (context, "open-document-selector-listbox-row");

	name_label = gtk_label_new (name);

	context = gtk_widget_get_style_context (GTK_WIDGET (name_label));
	gtk_style_context_add_class (context, "name-label");

	gtk_label_set_ellipsize (GTK_LABEL (name_label), PANGO_ELLIPSIZE_END);
	gtk_widget_set_halign (name_label, GTK_ALIGN_START);
	gtk_widget_set_valign (name_label, GTK_ALIGN_CENTER);

	path_label = gtk_label_new (path);

	context = gtk_widget_get_style_context (GTK_WIDGET (path_label));
	gtk_style_context_add_class (context, "path-label");

	gtk_label_set_ellipsize (GTK_LABEL (path_label), PANGO_ELLIPSIZE_START);
	gtk_widget_set_halign (path_label, GTK_ALIGN_START);
	gtk_widget_set_valign (path_label, GTK_ALIGN_CENTER);

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, OPEN_DOCUMENT_SELECTOR_ROW_BOX_SPACING);
	gtk_container_add (GTK_CONTAINER (row), vbox);

	gtk_box_pack_start (GTK_BOX (vbox), name_label, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), path_label, FALSE, FALSE, 0);

	gtk_widget_show_all (row);

	return row;
}

static gint
calculate_row_height (GeditOpenDocumentSelector *open_document_selector)
{
	GtkWidget *row;
	gint minimum_height;
	gint natural_height;

	/* Creating a fake row to mesure its height */
	row = create_row_layout (open_document_selector, "Fake name", "Fake Path");

	gtk_widget_get_preferred_height (row, &minimum_height, &natural_height);
	gtk_widget_destroy (row);

	return natural_height;
}

static GtkWidget *
create_row (GeditOpenDocumentSelector *open_document_selector,
            GtkRecentInfo             *info)
{
	GtkWidget *row;
	gchar *name;
	gchar *path;
	gchar *uri;
	GFile *location;

	uri = g_strdup (gtk_recent_info_get_uri (info));
	location = g_file_new_for_uri (uri);

	name = gedit_utils_basename_for_display (location);
	path = gedit_utils_location_get_dirname_for_display (location);

	row = create_row_layout (open_document_selector, (const gchar*)name, (const gchar*)path);

	g_object_set_data (G_OBJECT(row), "uri", uri);
	g_free (name);
	g_free (path);
	g_object_unref (location);

	return row;
}

static void
dispose_row (GeditOpenDocumentSelector *open_document_selector,
             GtkWidget                 *row)
{
	gchar *uri;

	uri = g_object_get_data (G_OBJECT(row), "uri");
	gtk_widget_destroy (GTK_WIDGET (row));
	g_free (uri);
}

static gboolean
real_populate_listbox (gpointer data)
{
	GeditOpenDocumentSelector *open_document_selector = GEDIT_OPEN_DOCUMENT_SELECTOR (data);
	GeditOpenDocumentSelectorPrivate *priv = open_document_selector->priv;
	GtkWidget *row = NULL;
	GtkRecentInfo *info;
	GList *children, *l, *items;

	g_assert (priv->recent_config.manager != NULL);

	/* Clear the listbox */
	children = gtk_container_get_children (GTK_CONTAINER (priv->listbox));

	for (l = children; l != NULL; l = l->next)
	{
		row = l->data;
		dispose_row (open_document_selector, row);
	}

	g_list_free (children);

	items = gedit_recent_get_items (&priv->recent_config);

	for (l = items; l != NULL; l = l->next)
	{
		info = l->data;
		row = create_row (open_document_selector, info);
		gtk_recent_info_unref(info);

		/* add a class until gtk implements :last-child */
		if (l->next == NULL)
		{
			GtkStyleContext *context;

			context = gtk_widget_get_style_context (row);
			gtk_style_context_add_class (context, "last-child");
		}

		gtk_list_box_insert (GTK_LIST_BOX (priv->listbox), row, -1);
	}

	g_list_free (items);
	priv->populate_listbox_id = 0;

	return FALSE;
}

static void
populate_listbox (GeditOpenDocumentSelector *open_document_selector)
{
	GeditOpenDocumentSelectorPrivate *priv = open_document_selector->priv;

	if (priv->populate_listbox_id)
	{
		return;
	}

	priv->populate_listbox_id = gdk_threads_add_idle_full (G_PRIORITY_HIGH_IDLE + 30,
	                                                       real_populate_listbox,
	                                                       open_document_selector,
	                                                       NULL);
}

static void
on_entry_changed (GtkEntry                  *entry,
                  GeditOpenDocumentSelector *open_document_selector)
{
	GeditOpenDocumentSelectorPrivate *priv = open_document_selector->priv;
	const gchar *entry_text;

	entry_text = gtk_entry_get_text (entry);

	g_free (priv->recent_config.substring_filter);
	priv->recent_config.substring_filter = g_utf8_strdown (entry_text, -1);

	populate_listbox (open_document_selector);
}

static void
on_recent_manager_changed (GtkRecentManager *manager,
                           gpointer          user_data)
{
	GeditOpenDocumentSelector  *open_document_selector = GEDIT_OPEN_DOCUMENT_SELECTOR (user_data);

	populate_listbox (open_document_selector);
}

static void
gedit_open_document_selector_finalize (GObject *object)
{
	GeditOpenDocumentSelector *open_document_selector = GEDIT_OPEN_DOCUMENT_SELECTOR (object);
	GeditOpenDocumentSelectorPrivate *priv = open_document_selector->priv;

	gedit_recent_configuration_destroy (&priv->recent_config);

	G_OBJECT_CLASS (gedit_open_document_selector_parent_class)->finalize (object);
}

static void
gedit_open_document_selector_dispose (GObject *object)
{
	GeditOpenDocumentSelector *open_document_selector = GEDIT_OPEN_DOCUMENT_SELECTOR (object);
	GeditOpenDocumentSelectorPrivate *priv = open_document_selector->priv;

	if (priv->recent_manager_changed_id)
	{
		g_signal_handler_disconnect (priv->recent_config.manager, priv->recent_manager_changed_id);
		priv->recent_manager_changed_id = 0;
	}

	if (priv->populate_listbox_id)
	{
		g_source_remove (priv->populate_listbox_id);
		priv->populate_listbox_id = 0;
	}

	G_OBJECT_CLASS (gedit_open_document_selector_parent_class)->dispose (object);
}

static void
on_row_activated (GtkWidget                 *listbox,
                  GtkListBoxRow             *row,
                  GeditOpenDocumentSelector *open_document_selector)
{
	gchar *uri;

	uri = g_object_get_data (G_OBJECT (row), "uri");

	gtk_list_box_unselect_all (GTK_LIST_BOX (listbox));

	g_signal_emit (G_OBJECT (open_document_selector), signals[RECENT_FILE_ACTIVATED], 0, uri);
}

static void
gedit_open_document_selector_constructed (GObject *object)
{
	GeditOpenDocumentSelector *open_document_selector = GEDIT_OPEN_DOCUMENT_SELECTOR (object);
	GeditOpenDocumentSelectorPrivate *priv = open_document_selector->priv;

	G_OBJECT_CLASS (gedit_open_document_selector_parent_class)->constructed (object);

	g_assert (priv->recent_config.manager);

	populate_listbox (open_document_selector);
}

static GtkSizeRequestMode
gedit_open_document_selector_get_request_mode (GtkWidget *widget)
{
	return GTK_SIZE_REQUEST_CONSTANT_SIZE;
}

static void
gedit_open_document_selector_get_preferred_width (GtkWidget *widget,
                                                  gint      *minimum_width,
                                                  gint      *natural_width)
{
	*minimum_width = *natural_width = OPEN_DOCUMENT_SELECTOR_WIDTH;
}

static void
gedit_open_document_selector_class_init (GeditOpenDocumentSelectorClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	gobject_class->constructed = gedit_open_document_selector_constructed;
	gobject_class->finalize = gedit_open_document_selector_finalize;
	gobject_class->dispose = gedit_open_document_selector_dispose;

	widget_class->get_request_mode = gedit_open_document_selector_get_request_mode;
	widget_class->get_preferred_width = gedit_open_document_selector_get_preferred_width;

	signals[RECENT_FILE_ACTIVATED] =
		g_signal_new ("recent-file-activated",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		              G_STRUCT_OFFSET (GeditOpenDocumentSelectorClass, recent_file_activated),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__STRING,
		              G_TYPE_NONE, 1,
		              G_TYPE_STRING);

	gtk_widget_class_set_template_from_resource (widget_class,
	                                             "/org/gnome/gedit/ui/gedit-open-document-selector.ui");

	gtk_widget_class_bind_template_child_private (widget_class, GeditOpenDocumentSelector, open_button);
	gtk_widget_class_bind_template_child_private (widget_class, GeditOpenDocumentSelector, listbox);
	gtk_widget_class_bind_template_child_private (widget_class, GeditOpenDocumentSelector, scrolled_window);
	gtk_widget_class_bind_template_child (widget_class, GeditOpenDocumentSelector, recent_search_entry);
}

static void
on_listbox_allocate (GtkWidget                 *widget,
                     GdkRectangle              *allocation,
                     GeditOpenDocumentSelector *open_document_selector)
{
	GeditOpenDocumentSelectorPrivate *priv = open_document_selector->priv;
	gint row_height;
	gint limit_capped;
	gint listbox_height;

	row_height = calculate_row_height (open_document_selector);
	limit_capped = MIN (priv->recent_config.limit, OPEN_DOCUMENT_SELECTOR_MAX_VISIBLE_ROWS);
	listbox_height = row_height * limit_capped;

	gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (priv->scrolled_window),
	                                            listbox_height - 2);
}

static void
gedit_open_document_selector_init (GeditOpenDocumentSelector *open_document_selector)
{
	GeditOpenDocumentSelectorPrivate *priv;
	GtkWidget *placeholder_label;
	GtkStyleContext *context;
	gint row_height;
	gint limit_capped;
	gint listbox_height;

	gedit_debug (DEBUG_WINDOW);

	open_document_selector->priv = gedit_open_document_selector_get_instance_private (open_document_selector);
	priv = open_document_selector->priv;

	gtk_widget_init_template (GTK_WIDGET (open_document_selector));

	/* gedit-open-document-selector initial state */
	gedit_recent_configuration_init_default (&priv->recent_config);

	priv->recent_manager_changed_id = g_signal_connect (priv->recent_config.manager,
	                                                    "changed",
	                                                    G_CALLBACK (on_recent_manager_changed),
	                                                    open_document_selector);

	priv->populate_listbox_id = 0;

	g_signal_connect (open_document_selector->recent_search_entry,
	                  "changed",
	                  G_CALLBACK (on_entry_changed),
	                  open_document_selector);

	g_signal_connect (priv->listbox,
	                  "row-activated",
	                  G_CALLBACK (on_row_activated),
	                  open_document_selector);

	placeholder_label = gtk_label_new (_("No results"));
	gtk_widget_set_halign (placeholder_label, GTK_ALIGN_CENTER);
	gtk_widget_set_valign (placeholder_label, GTK_ALIGN_CENTER);

	gtk_widget_show (placeholder_label);
	gtk_list_box_set_placeholder (GTK_LIST_BOX (priv->listbox), placeholder_label);

	row_height = calculate_row_height (open_document_selector);

	limit_capped = MIN (priv->recent_config.limit, OPEN_DOCUMENT_SELECTOR_MAX_VISIBLE_ROWS);
	listbox_height = row_height * limit_capped;

	/* We substract 2px, no idea where they come from */
	gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (priv->scrolled_window),
	                                            listbox_height - 2);

	context = gtk_widget_get_style_context (GTK_WIDGET (priv->listbox));
	gtk_style_context_add_class (context, "gedit-open-document-selector-listbox");

	g_signal_connect (priv->listbox,
	                  "size-allocate",
	                  G_CALLBACK (on_listbox_allocate),
	                  open_document_selector);
}

GeditOpenDocumentSelector *
gedit_open_document_selector_new (void)
{
	return g_object_new (GEDIT_TYPE_OPEN_DOCUMENT_SELECTOR,
	                     NULL);
}

/* ex:set ts=8 noet: */
