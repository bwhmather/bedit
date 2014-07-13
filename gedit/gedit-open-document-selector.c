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
#include "gedit-settings.h"
#include "gedit-utils.h"
#include "gedit-window.h"
#include "gedit-debug.h"

struct _GeditOpenDocumentSelectorPrivate
{
	GSettings *ui_settings;
	GtkRecentManager *manager;
	gint limit;

	GtkWidget *open_button;
	GtkWidget *listbox;
	GtkWidget *scrolled_window;

	GtkRecentSortType sort_type;
	GtkRecentSortFunc sort_func;
	gpointer sort_data;
	GDestroyNotify sort_data_destroy;

	GtkRecentFilter *current_filter;

	guint populate_listbox_id;
	gulong recent_manager_changed_id;

	/* RecentChooser properties */
	guint show_private : 1;
	guint show_not_found : 1;
	guint show_tips : 1;
	guint show_icons : 1;
	guint local_only : 1;
};

static void gtk_recent_chooser_iface_init (GtkRecentChooserIface *iface);

/* Properties,  gtkrecentchooserutils.h defines PROP_FIRST at this value */
enum {
	GTK_RECENT_CHOOSER_PROP_FIRST = 0x3000,
	GTK_RECENT_CHOOSER_PROP_RECENT_MANAGER,
	GTK_RECENT_CHOOSER_PROP_SHOW_PRIVATE,
	GTK_RECENT_CHOOSER_PROP_SHOW_NOT_FOUND,
	GTK_RECENT_CHOOSER_PROP_SHOW_TIPS,
	GTK_RECENT_CHOOSER_PROP_SHOW_ICONS,
	GTK_RECENT_CHOOSER_PROP_SELECT_MULTIPLE,
	GTK_RECENT_CHOOSER_PROP_LIMIT,
	GTK_RECENT_CHOOSER_PROP_LOCAL_ONLY,
	GTK_RECENT_CHOOSER_PROP_SORT_TYPE,
	GTK_RECENT_CHOOSER_PROP_FILTER,
	GTK_RECENT_CHOOSER_PROP_LAST
};

/* Signals */
enum
{
	RECENT_FILE_ACTIVATED,
	LAST_SIGNAL
};

#define OPEN_DOCUMENT_SELECTOR_WIDTH 400
#define OPEN_DOCUMENT_SELECTOR_LISTBOX_HEIGHT 300

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_CODE (GeditOpenDocumentSelector,
                         gedit_open_document_selector,
                         GTK_TYPE_BOX,
                         G_ADD_PRIVATE (GeditOpenDocumentSelector)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_RECENT_CHOOSER,
                                                gtk_recent_chooser_iface_init))

static void
on_entry_changed (GtkEntry                  *entry,
                  GeditOpenDocumentSelector *open_document_selector)
{
	GtkRecentFilter *filter;
	gchar *pattern;

	pattern = g_strdup (gtk_entry_get_text (entry));

	if (*pattern == '\0')
	{
		g_free (pattern);
		pattern = g_strdup ("*");
	}

	filter = gtk_recent_filter_new ();
	gtk_recent_filter_add_pattern (filter, pattern);

	gtk_recent_chooser_set_filter (GTK_RECENT_CHOOSER (open_document_selector), filter);
}

static GtkWidget *
create_row (GeditOpenDocumentSelector *open_document_selector,
            GtkRecentInfo             *info)
{
	GtkWidget *row;
	GtkWidget *vbox;
	gchar *name;
	GtkWidget *name_label;
	gchar *path;
	GtkWidget *path_label;
	gchar *uri;
	GtkStyleContext *context;
	GFile *location;

	row = gtk_list_box_row_new ();
	gtk_widget_set_has_tooltip (row, TRUE);

	uri = g_strdup (gtk_recent_info_get_uri (info));
	location = g_file_new_for_uri (uri);

	g_object_set_data (G_OBJECT(row), "uri", uri);

	name = gedit_utils_basename_for_display (location);
	name_label = gtk_label_new (name);
	g_free (name);

	context = gtk_widget_get_style_context (GTK_WIDGET (name_label));
	gtk_style_context_add_class (context, "name-label");

	gtk_label_set_ellipsize (GTK_LABEL (name_label), PANGO_ELLIPSIZE_END);
	gtk_widget_set_halign (name_label, GTK_ALIGN_START);
	gtk_widget_set_valign (name_label, GTK_ALIGN_CENTER);

	path = gedit_utils_location_get_dirname_for_display (location);
	path_label = gtk_label_new (path);

	g_free (path);
	g_object_unref (location);

	context = gtk_widget_get_style_context (GTK_WIDGET (path_label));
	gtk_style_context_add_class (context, "path-label");

	gtk_label_set_ellipsize (GTK_LABEL (path_label), PANGO_ELLIPSIZE_START);
	gtk_widget_set_halign (path_label, GTK_ALIGN_START);
	gtk_widget_set_valign (path_label, GTK_ALIGN_CENTER);

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
	gtk_container_add (GTK_CONTAINER (row), vbox);

	gtk_box_pack_start (GTK_BOX (vbox), name_label, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), path_label, FALSE, FALSE, 0);

	gtk_widget_show_all (row);

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

	g_assert (priv->manager != NULL);

	/* Clear the listbox */
	children = gtk_container_get_children (GTK_CONTAINER (priv->listbox));

	for (l = children; l != NULL; l = l->next)
	{
		row = l->data;
		dispose_row (open_document_selector, row);
	}

	g_list_free (children);

	items = gtk_recent_chooser_get_items (GTK_RECENT_CHOOSER (open_document_selector));

	for (l = items; l != NULL; l = l->next)
	{
		info = l->data;
		row = create_row (open_document_selector, info);
		gtk_recent_info_unref(info);

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

static GtkRecentManager *
gedit_open_document_selector_get_recent_manager (GtkRecentChooser *chooser)
{
	GeditOpenDocumentSelector *open_document_selector = GEDIT_OPEN_DOCUMENT_SELECTOR (chooser);
	GeditOpenDocumentSelectorPrivate *priv = open_document_selector->priv;

	return priv->manager;
}

static gint
sort_recent_items_mru (GtkRecentInfo *a,
                       GtkRecentInfo *b,
                       gpointer       unused)
{
  g_assert (a != NULL && b != NULL);

  return gtk_recent_info_get_modified (b) - gtk_recent_info_get_modified (a);
}

static void
gedit_open_document_selector_set_sort_func (GtkRecentChooser  *chooser,
                                            GtkRecentSortFunc  sort_func,
                                            gpointer           sort_data,
                                            GDestroyNotify     data_destroy)
{
	GeditOpenDocumentSelector *open_document_selector = GEDIT_OPEN_DOCUMENT_SELECTOR (chooser);
	GeditOpenDocumentSelectorPrivate *priv = open_document_selector->priv;

	if (priv->sort_data_destroy)
	{
		priv->sort_data_destroy (priv->sort_data);
		priv->sort_data_destroy = NULL;
	}

	if (sort_func)
	{
		priv->sort_func = sort_func;
		priv->sort_data = sort_data;
		priv->sort_data_destroy = data_destroy;
	}
	else
	{
		priv->sort_func = NULL;
		priv->sort_data = NULL;
		priv->sort_data_destroy = NULL;
	}
}

static gboolean
get_is_recent_filtered (GtkRecentFilter *filter,
                        GtkRecentInfo   *info)
{
	GtkRecentFilterInfo filter_info;
	GtkRecentFilterFlags needed;
	gboolean retval;

	g_assert (info != NULL);

	needed = gtk_recent_filter_get_needed (filter);

	filter_info.contains = GTK_RECENT_FILTER_URI | GTK_RECENT_FILTER_MIME_TYPE;

	filter_info.uri = gtk_recent_info_get_uri (info);
	filter_info.mime_type = gtk_recent_info_get_mime_type (info);

	if (needed & GTK_RECENT_FILTER_DISPLAY_NAME)
	{
		filter_info.display_name = gtk_recent_info_get_display_name (info);
		filter_info.contains |= GTK_RECENT_FILTER_DISPLAY_NAME;
	}
	else
	{
		filter_info.uri = NULL;
	}

	if (needed & GTK_RECENT_FILTER_APPLICATION)
	{
		filter_info.applications = (const gchar **) gtk_recent_info_get_applications (info, NULL);
		filter_info.contains |= GTK_RECENT_FILTER_APPLICATION;
	}
	else
	{
		filter_info.applications = NULL;
	}

	if (needed & GTK_RECENT_FILTER_GROUP)
	{
		filter_info.groups = (const gchar **) gtk_recent_info_get_groups (info, NULL);
		filter_info.contains |= GTK_RECENT_FILTER_GROUP;
	}
	else
	{
		filter_info.groups = NULL;
	}

	if (needed & GTK_RECENT_FILTER_AGE)
	{
		filter_info.age = gtk_recent_info_get_age (info);
		filter_info.contains |= GTK_RECENT_FILTER_AGE;
	}
	else
	{
		filter_info.age = -1;
	}

	retval = gtk_recent_filter_filter (filter, &filter_info);

	/* these we own */
	if (filter_info.applications)
	{
		g_strfreev ((gchar **) filter_info.applications);
	}

	if (filter_info.groups)
	{
		g_strfreev ((gchar **) filter_info.groups);
	}

	return !retval;
}

static GList *
gedit_open_document_selector_get_items (GtkRecentChooser *chooser)
{
	GeditOpenDocumentSelector *open_document_selector = GEDIT_OPEN_DOCUMENT_SELECTOR (chooser);
	GeditOpenDocumentSelectorPrivate *priv = open_document_selector->priv;
	GtkRecentManager *manager;
	GtkRecentSortType sort_type;
	GList *items;
	GList *filter_items = NULL, *l;
	GCompareDataFunc compare_func;
	gint limit;
	gint length;
	gboolean local_only = FALSE;
	gboolean show_private = FALSE;
	gboolean show_not_found = FALSE;

	g_return_val_if_fail (GTK_IS_RECENT_CHOOSER (chooser), NULL);

	manager = gedit_open_document_selector_get_recent_manager (chooser);

	items = gtk_recent_manager_get_items (manager);
	if (!items)
	{
		return NULL;
	}

	limit = gtk_recent_chooser_get_limit (chooser);
	if (limit == 0)
	{
		return NULL;
	}

	g_object_get (G_OBJECT (chooser),
	              "local-only", &local_only,
	              "show-private", &show_private,
	              "show-not-found", &show_not_found,
	              NULL);

	for (l = items; l != NULL; l = l->next)
	{
		GtkRecentInfo *info = l->data;
		gboolean remove_item = FALSE;

		if (priv->current_filter && get_is_recent_filtered (priv->current_filter, info))
		{
			remove_item = TRUE;
		}

		if (local_only && !gtk_recent_info_is_local (info))
		{
			remove_item = TRUE;
		}

		if (!show_private && gtk_recent_info_get_private_hint (info))
		{
			remove_item = TRUE;
		}

		if (!show_not_found && !gtk_recent_info_exists (info))
		{
			remove_item = TRUE;
		}

		if (!remove_item)
		{
			filter_items = g_list_prepend (filter_items, info);
		}
		else
		{
			gtk_recent_info_unref (info);
		}
	}

	g_list_free (items);
	items = filter_items;

	if (!items)
	{
		return NULL;
	}

	sort_type = gtk_recent_chooser_get_sort_type (chooser);
	switch (sort_type)
	{
		case GTK_RECENT_SORT_NONE:
			compare_func = NULL;
			break;
		case GTK_RECENT_SORT_MRU:
			compare_func = (GCompareDataFunc) sort_recent_items_mru;
			break;
		default:
			g_assert_not_reached ();
			break;
	}

	if (compare_func)
	{
		SortRecentData sort_recent;

		sort_recent.func = priv->sort_func;
		sort_recent.data = priv->sort_data;

		items = g_list_sort_with_data (items, compare_func, &sort_recent);
	}

	length = g_list_length (items);
	if ((limit != -1) && (length > limit))
	{
		GList *clamp, *l;

		clamp = g_list_nth (items, limit - 1);
		if (!clamp)
		{
			return items;
		}

		l = clamp->next;
		clamp->next = NULL;

		g_list_free_full (l, (GDestroyNotify) gtk_recent_info_unref);
	}

	return items;
}

static void
gedit_open_document_selector_select_all (GtkRecentChooser *chooser)
{
	g_warning ("This function is not implemented for widgets of class '%s'",
	           g_type_name (G_OBJECT_TYPE (chooser)));
}

static void
gedit_open_document_selector_unselect_all (GtkRecentChooser *chooser)
{
	g_warning ("This function is not implemented for widgets of class '%s'",
	           g_type_name (G_OBJECT_TYPE (chooser)));
}

static gboolean
gedit_open_document_selector_set_current_uri (GtkRecentChooser  *chooser,
                                   const gchar       *uri,
                                   GError           **error)
{
	g_warning ("This function is not implemented for widgets of class '%s'",
	           g_type_name (G_OBJECT_TYPE (chooser)));

	return FALSE;
}

static gchar *
gedit_open_document_selector_get_current_uri (GtkRecentChooser  *chooser)
{
	g_warning ("This function is not implemented for widgets of class '%s'",
	           g_type_name (G_OBJECT_TYPE (chooser)));

	return NULL;
}

static gboolean
gedit_open_document_selector_select_uri (GtkRecentChooser  *chooser,
                              const gchar       *uri,
                              GError           **error)
{
	g_warning ("This function is not implemented for widgets of class '%s'",
	           g_type_name (G_OBJECT_TYPE (chooser)));

	return FALSE;
}

static void
gedit_open_document_selector_unselect_uri (GtkRecentChooser *chooser,
				       const gchar     *uri)
{
	g_warning ("This function is not implemented for widgets of class '%s'",
	           g_type_name (G_OBJECT_TYPE (chooser)));
}

static void
set_current_filter (GeditOpenDocumentSelector *open_document_selector,
                    GtkRecentFilter           *filter)
{
	GeditOpenDocumentSelectorPrivate *priv = open_document_selector->priv;

	if (priv->current_filter)
	{
		g_object_unref (G_OBJECT (priv->current_filter));
	}

	priv->current_filter = filter;

	if (priv->current_filter)
	{
		g_object_ref_sink (priv->current_filter);
	}

	populate_listbox (open_document_selector);

	g_object_notify (G_OBJECT (open_document_selector), "filter");
}

static void
gedit_open_document_selector_add_filter (GtkRecentChooser *chooser,
                                         GtkRecentFilter  *filter)
{
	set_current_filter (GEDIT_OPEN_DOCUMENT_SELECTOR (chooser), filter);
}

static void
gedit_open_document_selector_remove_filter (GtkRecentChooser *chooser,
                                            GtkRecentFilter  *filter)
{
	GeditOpenDocumentSelector *open_document_selector = GEDIT_OPEN_DOCUMENT_SELECTOR (chooser);
	GeditOpenDocumentSelectorPrivate *priv = open_document_selector->priv;

	if (filter == priv->current_filter)
	{
		g_object_unref (priv->current_filter);
		priv->current_filter = NULL;

		g_object_notify (G_OBJECT (open_document_selector), "filter");
	}
}

static GSList *
gedit_open_document_selector_list_filters (GtkRecentChooser *chooser)
{
	GeditOpenDocumentSelector *open_document_selector = GEDIT_OPEN_DOCUMENT_SELECTOR (chooser);
	GeditOpenDocumentSelectorPrivate *priv = open_document_selector->priv;
	GSList *retval = NULL;

	if (priv->current_filter)
	{
		retval = g_slist_prepend (retval, priv->current_filter);
	}

	return retval;
}

static void
on_recent_manager_changed (GtkRecentManager *manager,
                           gpointer          user_data)
{
	GeditOpenDocumentSelector  *open_document_selector = GEDIT_OPEN_DOCUMENT_SELECTOR (user_data);

	populate_listbox (open_document_selector);
}

static void
set_recent_manager (GeditOpenDocumentSelector  *open_document_selector,
                    GtkRecentManager           *manager)
{
	GeditOpenDocumentSelectorPrivate *priv = open_document_selector->priv;

	g_return_if_fail (GTK_IS_RECENT_MANAGER (manager) || manager == NULL);

	if (priv->recent_manager_changed_id)
	{
		g_signal_handler_disconnect (priv->manager, priv->recent_manager_changed_id);
		priv->recent_manager_changed_id = 0;
	}

	priv->manager = manager ? manager : gtk_recent_manager_get_default ();

	priv->recent_manager_changed_id = g_signal_connect (priv->manager,
	                                                    "changed",
	                                                    G_CALLBACK (on_recent_manager_changed),
	                                                    open_document_selector);
}

static void
gedit_open_document_selector_finalize (GObject *object)
{
	GeditOpenDocumentSelector *open_document_selector = GEDIT_OPEN_DOCUMENT_SELECTOR (object);
	GeditOpenDocumentSelectorPrivate *priv = open_document_selector->priv;

	priv->manager = NULL;

	G_OBJECT_CLASS (gedit_open_document_selector_parent_class)->finalize (object);
}

static void
gedit_open_document_selector_dispose (GObject *object)
{
	GeditOpenDocumentSelector *open_document_selector = GEDIT_OPEN_DOCUMENT_SELECTOR (object);
	GeditOpenDocumentSelectorPrivate *priv = open_document_selector->priv;

	g_clear_object (&priv->ui_settings);
	g_clear_object (&priv->current_filter);

	if (priv->sort_data_destroy)
	{
		priv->sort_data_destroy (priv->sort_data);
		priv->sort_data_destroy = NULL;
	}

	priv->sort_data = NULL;
	priv->sort_func = NULL;

	if (priv->recent_manager_changed_id)
	{
		g_signal_handler_disconnect (priv->manager, priv->recent_manager_changed_id);
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

	g_assert (priv->manager);

	populate_listbox (open_document_selector);
}

static void
gedit_open_document_selector_set_property (GObject      *object,
                                           guint         prop_id,
                                           const GValue *value,
                                           GParamSpec   *pspec)
{
	GeditOpenDocumentSelector *open_document_selector = GEDIT_OPEN_DOCUMENT_SELECTOR (object);
	GeditOpenDocumentSelectorPrivate *priv = open_document_selector->priv;

	switch (prop_id)
	{
		case GTK_RECENT_CHOOSER_PROP_RECENT_MANAGER:
			set_recent_manager (open_document_selector, g_value_get_object (value));
			break;
		case GTK_RECENT_CHOOSER_PROP_SHOW_PRIVATE:
			if (priv->show_private != g_value_get_boolean (value))
			{
				priv->show_private = g_value_get_boolean (value);
				g_object_notify_by_pspec (object, pspec);
			}
			break;
		case GTK_RECENT_CHOOSER_PROP_SHOW_NOT_FOUND:
			if (priv->show_not_found != g_value_get_boolean (value))
			{
				priv->show_not_found = g_value_get_boolean (value);
				g_object_notify_by_pspec (object, pspec);
			}
			break;
		case GTK_RECENT_CHOOSER_PROP_SHOW_TIPS:
			g_warning ("%s: Choosers of type `%s' do not support tooltips.",
			           G_STRFUNC,
			           G_OBJECT_TYPE_NAME (object));
			break;
		case GTK_RECENT_CHOOSER_PROP_SHOW_ICONS:
			g_warning ("%s: Choosers of type `%s' do not support icons.",
			           G_STRFUNC,
			           G_OBJECT_TYPE_NAME (object));
			break;
		case GTK_RECENT_CHOOSER_PROP_SELECT_MULTIPLE:
			g_warning ("%s: Choosers of type `%s' do not support selecting multiple items.",
			           G_STRFUNC,
			           G_OBJECT_TYPE_NAME (object));
			break;
		case GTK_RECENT_CHOOSER_PROP_LOCAL_ONLY:
			if (priv->local_only != g_value_get_boolean (value))
			{
				priv->local_only = g_value_get_boolean (value);
				g_object_notify_by_pspec (object, pspec);
			}
			break;
		case GTK_RECENT_CHOOSER_PROP_LIMIT:
			if (priv->limit != g_value_get_int (value))
			{
				priv->limit = g_value_get_int (value);
				g_object_notify_by_pspec (object, pspec);
			}
			break;
		case GTK_RECENT_CHOOSER_PROP_SORT_TYPE:
			if (priv->sort_type != g_value_get_enum (value))
			{
				priv->sort_type = g_value_get_enum (value);
				g_object_notify_by_pspec (object, pspec);
			}
			break;
		case GTK_RECENT_CHOOSER_PROP_FILTER:
			set_current_filter (open_document_selector, g_value_get_object (value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gedit_open_document_selector_get_property (GObject    *object,
                                           guint       prop_id,
                                           GValue     *value,
                                           GParamSpec *pspec)
{
	GeditOpenDocumentSelector *open_document_selector = GEDIT_OPEN_DOCUMENT_SELECTOR (object);
	GeditOpenDocumentSelectorPrivate *priv = open_document_selector->priv;

	switch (prop_id)
	{
		case GTK_RECENT_CHOOSER_PROP_SHOW_TIPS:
			g_value_set_boolean (value, FALSE);
			break;
		case GTK_RECENT_CHOOSER_PROP_LIMIT:
			g_value_set_int (value, priv->limit);
			break;
		case GTK_RECENT_CHOOSER_PROP_LOCAL_ONLY:
			g_value_set_boolean (value, priv->local_only);
			break;
		case GTK_RECENT_CHOOSER_PROP_SORT_TYPE:
			g_value_set_enum (value, priv->sort_type);
			break;
		case GTK_RECENT_CHOOSER_PROP_SHOW_PRIVATE:
			g_value_set_boolean (value, priv->show_private);
			break;
		case GTK_RECENT_CHOOSER_PROP_SHOW_NOT_FOUND:
			g_value_set_boolean (value, priv->show_not_found);
			break;
		case GTK_RECENT_CHOOSER_PROP_SHOW_ICONS:
			g_value_set_boolean (value, FALSE);
			break;
		case GTK_RECENT_CHOOSER_PROP_SELECT_MULTIPLE:
			g_value_set_boolean (value, FALSE);
			break;
		case GTK_RECENT_CHOOSER_PROP_FILTER:
			g_value_set_object (value, priv->current_filter);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
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
gtk_recent_chooser_iface_init (GtkRecentChooserIface *iface)
{
	iface->set_current_uri = gedit_open_document_selector_set_current_uri;
	iface->get_current_uri = gedit_open_document_selector_get_current_uri;
	iface->select_uri = gedit_open_document_selector_select_uri;
	iface->unselect_uri = gedit_open_document_selector_unselect_uri;
	iface->select_all = gedit_open_document_selector_select_all;
	iface->unselect_all = gedit_open_document_selector_unselect_all;
	iface->set_sort_func = gedit_open_document_selector_set_sort_func;
	iface->get_recent_manager = gedit_open_document_selector_get_recent_manager;
	iface->add_filter = gedit_open_document_selector_add_filter;
	iface->remove_filter = gedit_open_document_selector_remove_filter;
	iface->list_filters = gedit_open_document_selector_list_filters;
	iface->get_items = gedit_open_document_selector_get_items;
}

static void
gedit_open_document_selector_class_init (GeditOpenDocumentSelectorClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	gobject_class->constructed = gedit_open_document_selector_constructed;
	gobject_class->finalize = gedit_open_document_selector_finalize;
	gobject_class->dispose = gedit_open_document_selector_dispose;
	gobject_class->set_property = gedit_open_document_selector_set_property;
	gobject_class->get_property = gedit_open_document_selector_get_property;

	widget_class->get_request_mode = gedit_open_document_selector_get_request_mode;
	widget_class->get_preferred_width = gedit_open_document_selector_get_preferred_width;

	g_object_class_override_property (gobject_class, GTK_RECENT_CHOOSER_PROP_RECENT_MANAGER, "recent-manager");
	g_object_class_override_property (gobject_class, GTK_RECENT_CHOOSER_PROP_SHOW_PRIVATE, "show-private");
	g_object_class_override_property (gobject_class, GTK_RECENT_CHOOSER_PROP_SHOW_TIPS, "show-tips");
	g_object_class_override_property (gobject_class, GTK_RECENT_CHOOSER_PROP_SHOW_ICONS, "show-icons");
	g_object_class_override_property (gobject_class, GTK_RECENT_CHOOSER_PROP_SHOW_NOT_FOUND, "show-not-found");
	g_object_class_override_property (gobject_class, GTK_RECENT_CHOOSER_PROP_SELECT_MULTIPLE, "select-multiple");
	g_object_class_override_property (gobject_class, GTK_RECENT_CHOOSER_PROP_LIMIT, "limit");
	g_object_class_override_property (gobject_class, GTK_RECENT_CHOOSER_PROP_LOCAL_ONLY, "local-only");
	g_object_class_override_property (gobject_class, GTK_RECENT_CHOOSER_PROP_SORT_TYPE, "sort-type");
	g_object_class_override_property (gobject_class, GTK_RECENT_CHOOSER_PROP_FILTER, "filter");

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
gedit_open_document_selector_init (GeditOpenDocumentSelector *open_document_selector)
{
	GeditOpenDocumentSelectorPrivate *priv;
	GtkWidget *placeholder_label;
	GtkStyleContext *context;

	gedit_debug (DEBUG_WINDOW);

	open_document_selector->priv = gedit_open_document_selector_get_instance_private (open_document_selector);
	priv = open_document_selector->priv;

	gtk_widget_init_template (GTK_WIDGET (open_document_selector));

	priv->ui_settings = g_settings_new ("org.gnome.gedit.preferences.ui");

	/* gedit-open-document-selector initial state */
	priv->show_icons= FALSE;
	priv->show_tips = FALSE;
	priv->show_not_found = TRUE;
	priv->show_private = FALSE;
	priv->local_only = FALSE;

	priv->sort_type = GTK_RECENT_SORT_MRU;

	priv->current_filter = NULL;
	priv->sort_func = NULL;
	priv->sort_data = NULL;

	priv->populate_listbox_id = 0;
	priv->recent_manager_changed_id = 0;

	g_settings_bind (priv->ui_settings,
	                 GEDIT_SETTINGS_MAX_RECENTS,
	                 open_document_selector,
	                 "limit",
	                 G_SETTINGS_BIND_GET);

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

	gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (priv->scrolled_window),
	                                            OPEN_DOCUMENT_SELECTOR_LISTBOX_HEIGHT);


	context = gtk_widget_get_style_context (GTK_WIDGET (priv->listbox));
	gtk_style_context_add_class (context, "gedit-open-document-selector-listbox");
}

GeditOpenDocumentSelector *
gedit_open_document_selector_new (void)
{
	return g_object_new (GEDIT_TYPE_OPEN_DOCUMENT_SELECTOR,
	                     "recent-manager", NULL,
	                     NULL);
}

GeditOpenDocumentSelector *
gedit_open_document_selector_new_for_manager (GtkRecentManager *manager)
{
	g_return_val_if_fail (manager == NULL || GTK_IS_RECENT_MANAGER (manager), NULL);

	return g_object_new (GEDIT_TYPE_OPEN_DOCUMENT_SELECTOR,
	                     "recent-manager", manager,
	                     NULL);
}

/* ex:set ts=8 noet: */
