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

#include "gedit-open-document-selector.h"
#include "gedit-open-document-selector-store.h"
#include "gedit-open-document-selector-helper.h"

#include <time.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <glib/gstdio.h>
#include <gio/gio.h>

#include "gedit-recent.h"
#include "gedit-utils.h"
#include "gedit-window.h"
#include "gedit-debug.h"

struct _GeditOpenDocumentSelectorPrivate
{
	GtkWidget *open_button;
	GtkWidget *treeview;
	GtkListStore *liststore;
	GtkCellRenderer *name_renderer;
	GtkCellRenderer *path_renderer;
	GtkWidget *placeholder_box;
	GtkWidget *scrolled_window;

	GeditOpenDocumentSelectorStore *selector_store;
	GList *recent_items;
	GList *home_dir_items;
	GList *desktop_dir_items;
	GList *local_bookmarks_dir_items;
	GList *file_browser_root_items;
	GList *active_doc_dir_items;
	GList *current_docs_items;
	GList *all_items;

	gint populate_liststore_is_idle : 1;
	gint populate_scheduled : 1;
};

typedef enum
{
	SELECTOR_TAG_NONE,
	SELECTOR_TAG_MATCH
} SelectorTag;

enum
{
	NAME_COLUMN,
	PATH_COLUMN,
	URI_COLUMN,
	N_COLUMNS
};

/* Signals */
enum
{
	SELECTOR_FILE_ACTIVATED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

enum
{
	PROP_0,
	PROP_WINDOW
};

/* Value 0xFF is reserved to mark the end of the array */
#define BYTE_ARRAY_END 0xFF

#define OPEN_DOCUMENT_SELECTOR_WIDTH 400
#define OPEN_DOCUMENT_SELECTOR_MAX_VISIBLE_ROWS 10

G_DEFINE_TYPE_WITH_PRIVATE (GeditOpenDocumentSelector, gedit_open_document_selector, GTK_TYPE_BOX)

static inline const guint8 *
get_byte_run (const guint8 *byte_array,
              gsize        *count,
              SelectorTag  *tag)
{
	guint8 tag_found;
	gsize c = 1;

	tag_found = *byte_array++;

	while ( *byte_array != BYTE_ARRAY_END && *byte_array == tag_found)
	{
		c++;
		byte_array++;
	}

	*count = c;
	*tag = tag_found;

	return ( *byte_array != BYTE_ARRAY_END) ? byte_array : NULL;
}

static gchar*
get_markup_from_tagged_byte_array (const gchar  *str,
                                   const guint8 *byte_array)
{
	gchar *txt;
	GString *string;
	gchar *result_str;
	SelectorTag tag;
	gsize count;

	string = g_string_sized_new (255);

	while (TRUE)
	{
		byte_array = get_byte_run (byte_array, &count, &tag);
		txt = g_markup_escape_text (str, count);
		if (tag == SELECTOR_TAG_MATCH)
		{
			g_string_append_printf (string, "<span weight =\"heavy\" color =\"black\">%s</span>", txt);
		}
		else
		{
			g_string_append (string, txt);
		}

		g_free (txt);

		if (!byte_array)
		{
			break;
		}

		str = (const gchar *)((gsize)str + count);
	}

	result_str = g_string_free (string, FALSE);
	return result_str;
}

static guint8 *
get_tagged_byte_array (const gchar *uri,
                       GRegex      *filter_regex)
{
	guint8 *byte_array;
	gsize uri_len;
	GMatchInfo *match_info;
	gboolean no_match = TRUE;

	g_return_val_if_fail (uri != NULL, NULL);

	uri_len = strlen (uri);
	byte_array = g_malloc0 (uri_len + 1);
	byte_array[uri_len] = BYTE_ARRAY_END;

	if (g_regex_match (filter_regex, uri, 0, &match_info) == TRUE)
	{
		while (g_match_info_matches (match_info) == TRUE)
		{
			guint8 *p;
			gint match_len;
			gint start_pos;
			gint end_pos;

			if (g_match_info_fetch_pos (match_info, 0, &start_pos, &end_pos) == TRUE)
			{
				match_len = end_pos - start_pos;
				no_match = FALSE;

				p = (guint8 *)((gsize)byte_array + start_pos);
				memset (p, SELECTOR_TAG_MATCH, match_len);
			}

			g_match_info_next (match_info, NULL);
		}
	}

	g_match_info_free (match_info);

	if (no_match)
	{
		g_free (byte_array);
		return NULL;
	}

	return byte_array;
}

static void
get_markup_for_path_and_name (GRegex       *filter_regex,
                              const gchar  *src_path,
                              const gchar  *src_name,
                              gchar       **dst_path,
                              gchar       **dst_name)
{
	gchar *filename;
	gsize path_len;
	gsize name_len;
	gsize path_separator_len;
	guint8 *byte_array;
	guint8 *path_byte_array;
	guint8 *name_byte_array;

	filename = g_build_filename (src_path, src_name, NULL);

	path_len = g_utf8_strlen (src_path, -1);
	name_len = g_utf8_strlen (src_name, -1);
	path_separator_len = g_utf8_strlen (filename, -1) - ( path_len + name_len);

	byte_array = get_tagged_byte_array (filename, filter_regex);
	if (byte_array)
	{
		path_byte_array = g_memdup (byte_array, path_len + 1);
		path_byte_array[path_len] = BYTE_ARRAY_END;

		/* name_byte_array is part of byte_array, so released with it */
		name_byte_array = (guint8 *)((gsize)byte_array + path_len + path_separator_len);

		*dst_path = get_markup_from_tagged_byte_array (src_path, path_byte_array);
		*dst_name = get_markup_from_tagged_byte_array (src_name, name_byte_array);

		g_free (byte_array);
		g_free (path_byte_array);
	}
	else
	{
		*dst_path = g_strdup (src_path);
		*dst_name = g_strdup (src_name);
	}

	g_free (filename);
}

static void
create_row (GeditOpenDocumentSelector *selector,
            const FileItem            *item,
            GRegex                    *filter_regex)
{
	GeditOpenDocumentSelectorPrivate *priv = selector->priv;
	GtkTreeIter iter;
	gchar *uri;
	gchar *dst_path;
	gchar *dst_name;

	uri =item->uri;

	if (filter_regex)
	{
		get_markup_for_path_and_name (filter_regex,
		                              (const gchar *)item->path,
		                              (const gchar *)item->name,
		                              &dst_path,
		                              &dst_name);
	}
	else
	{
		dst_path = g_markup_escape_text (item->path, -1);
		dst_name = g_markup_escape_text (item->name, -1);
	}

	gtk_list_store_append (priv->liststore, &iter);
	gtk_list_store_set (priv->liststore, &iter,
	                    URI_COLUMN, uri,
	                    NAME_COLUMN, dst_name,
	                    PATH_COLUMN, dst_path,
	                    -1);

	g_free (dst_path);
	g_free (dst_name);
}

static gint
sort_items_by_mru (FileItem *a,
                   FileItem *b,
                   gpointer  unused)
{
	glong diff;

	g_assert (a != NULL && b != NULL);
	diff = b->access_time.tv_sec - a->access_time.tv_sec;

	if (diff == 0)
	{
		return (b->access_time.tv_usec - a->access_time.tv_usec);
	}
	else
	{
		return diff;
	}
}

static GList *
compute_all_items_list (GeditOpenDocumentSelector *selector)
{
	GeditOpenDocumentSelectorPrivate *priv = selector->priv;
	GList *recent_items;
	GList *home_dir_items;
	GList *desktop_dir_items;
	GList *local_bookmarks_dir_items;
	GList *file_browser_root_items;
	GList *active_doc_dir_items;
	GList *current_docs_items;
	GList *all_items = NULL;

	/* Copy/concat the whole list */
	recent_items = gedit_open_document_selector_copy_file_items_list ((const GList *)priv->recent_items);
	home_dir_items = gedit_open_document_selector_copy_file_items_list ((const GList *)priv->home_dir_items);
	desktop_dir_items = gedit_open_document_selector_copy_file_items_list ((const GList *)priv->desktop_dir_items);
	local_bookmarks_dir_items = gedit_open_document_selector_copy_file_items_list ((const GList *)priv->local_bookmarks_dir_items);
	file_browser_root_items = gedit_open_document_selector_copy_file_items_list ((const GList *)priv->file_browser_root_items);
	active_doc_dir_items = gedit_open_document_selector_copy_file_items_list ((const GList *)priv->active_doc_dir_items);
	current_docs_items = gedit_open_document_selector_copy_file_items_list ((const GList *)priv->current_docs_items);

	if (priv->all_items)
	{
		gedit_open_document_selector_free_file_items_list (priv->all_items);
		priv->all_items = NULL;
	}

	all_items = g_list_concat (all_items, recent_items);
	all_items = g_list_concat (all_items, home_dir_items);
	all_items = g_list_concat (all_items, desktop_dir_items);
	all_items = g_list_concat (all_items, local_bookmarks_dir_items);
	all_items = g_list_concat (all_items, file_browser_root_items);
	all_items = g_list_concat (all_items, active_doc_dir_items);
	all_items = g_list_concat (all_items, current_docs_items);

	return all_items;
}

static GList *
clamp_recent_items_list (GList *recent_items,
                         gint   limit)
{
	GList *recent_items_capped = NULL;
	GList *l;
	FileItem *item;

	l = recent_items;
	while (limit > 0 && l != NULL)
	{
		item = gedit_open_document_selector_copy_fileitem_item (l->data);
		recent_items_capped = g_list_prepend (recent_items_capped, item);
		l = l->next;
		limit -= 1;
	}

	recent_items_capped = g_list_reverse (recent_items_capped);
	return recent_items_capped;
}

/* Setup the fileitem, depending uri's scheme
 * Return a string to search in.
 */
static gchar *
fileitem_setup (FileItem *item)
{
	gchar *scheme;
	gchar *filename;
	gchar *candidate = NULL;
	gchar *path;
	gchar *name;

	scheme = g_uri_parse_scheme (item->uri);
	if (g_strcmp0 (scheme, "file") == 0)
	{
		filename = g_filename_from_uri ((const gchar *)item->uri, NULL, NULL);
		if (filename)
		{
			path = g_path_get_dirname (filename);
			item->path = g_filename_to_utf8 (path, -1, NULL, NULL, NULL);
			g_free (path);

			name = g_path_get_basename (filename);
			item->name = g_filename_to_utf8 (name, -1, NULL, NULL, NULL);
			g_free (name);

			candidate = g_utf8_strdown (filename, -1);
			g_free (filename);
		}
	}

	g_free (scheme);

	return candidate;
}

/* If filter == NULL then items are
 * not checked against the filter.
 */
static GList *
fileitem_list_filter (GList       *items,
                      const gchar *filter)
{
	GList *new_items = NULL;
	GList *l;

	for (l = items; l != NULL; l = l->next)
	{
		FileItem *item;
		gchar *candidate;

		item = l->data;
		candidate = fileitem_setup (item);

		if (candidate && (filter ==  NULL || strstr (candidate, filter)))
		{
			new_items = g_list_prepend (new_items,
			                            gedit_open_document_selector_copy_fileitem_item (item));
		}

		g_free (candidate);
	}

	new_items = g_list_reverse (new_items);
	return new_items;
}

/* Remove duplicated, the HEAD of the list never change,
 * the list passed in is modified.
 */
static void
fileitem_list_remove_duplicates (GList *items)
{
	GList *l;
	G_GNUC_UNUSED GList *dummy_ptr;

	l = items;
	while (l != NULL)
	{
		gchar *l_uri, *l1_uri;
		GList *l1;

		if ((l1 = l->next) == NULL)
		{
			break;
		}

		l_uri = ((FileItem *)l->data)->uri;
		l1_uri = ((FileItem *)l1->data)->uri;
		if (g_strcmp0 (l_uri, l1_uri) == 0)
		{
			gedit_open_document_selector_free_fileitem_item ((FileItem *)l1->data);
			dummy_ptr = g_list_delete_link (items, l1);
		}
		else
		{
			l = l->next;
		}
	}
}

static gboolean
real_populate_liststore (gpointer data)
{
	GeditOpenDocumentSelector *selector = GEDIT_OPEN_DOCUMENT_SELECTOR (data);
	GeditOpenDocumentSelectorPrivate *priv = selector->priv;
	GeditOpenDocumentSelectorStore *selector_store = priv->selector_store;
	GList *l;
	GList *filter_items = NULL;
	gchar *filter;
	GRegex *filter_regex = NULL;
	priv->populate_liststore_is_idle = FALSE;

	DEBUG_SELECTOR_TIMER_DECL
	DEBUG_SELECTOR_TIMER_NEW

	gtk_list_store_clear (priv->liststore);

	filter = gedit_open_document_selector_store_get_recent_filter (selector_store);
	if (filter && *filter != '\0')
	{
		DEBUG_SELECTOR (g_print ("Selector(%p): populate liststore: all lists\n", selector););

		filter_items = fileitem_list_filter (priv->all_items, (const gchar *)filter);
		filter_items = g_list_sort_with_data (filter_items, (GCompareDataFunc)sort_items_by_mru, NULL);
		fileitem_list_remove_duplicates (filter_items);

		filter_regex = g_regex_new (filter, G_REGEX_CASELESS, 0, NULL);
	}
	else
	{
		gint recent_limit;
		GList *recent_items;

		DEBUG_SELECTOR (g_print ("Selector(%p): populate liststore: recent files list\n", selector););

		recent_limit = gedit_open_document_selector_store_get_recent_limit (selector_store);

		if (recent_limit > 0 )
		{
			recent_items = fileitem_list_filter (priv->recent_items, NULL);
			filter_items = clamp_recent_items_list (recent_items, recent_limit);
			gedit_open_document_selector_free_file_items_list (recent_items);
		}
		else
		{
			filter_items = fileitem_list_filter (priv->recent_items, NULL);
		}
	}

	g_free (filter);

	DEBUG_SELECTOR (g_print ("Selector(%p): populate liststore: length:%i\n",
	                         selector, g_list_length (filter_items)););

	/* Show the placeholder if no results, show the treeview otherwise */
	gtk_widget_set_visible (priv->scrolled_window, (filter_items != NULL));
	gtk_widget_set_visible (priv->placeholder_box, (filter_items == NULL));

	for (l = filter_items; l != NULL; l = l->next)
	{
		FileItem *item;

		item = l->data;
		create_row (selector, (const FileItem *)item, filter_regex);
	}

	if (filter_regex)
	{
		g_regex_unref (filter_regex);
	}

	gedit_open_document_selector_free_file_items_list (filter_items);

	DEBUG_SELECTOR (g_print ("Selector(%p): populate liststore: time:%lf\n\n",
	                          selector, DEBUG_SELECTOR_TIMER_GET););
	DEBUG_SELECTOR_TIMER_DESTROY

	if (priv->populate_scheduled == TRUE)
	{
		priv->populate_scheduled = FALSE;
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

static void
populate_liststore (GeditOpenDocumentSelector *selector)
{
	GeditOpenDocumentSelectorPrivate *priv = selector->priv;

	/* Populate requests are compressed */
	if (priv->populate_liststore_is_idle)
	{
		DEBUG_SELECTOR (g_print ("Selector(%p): populate liststore: idle\n", selector););

		priv->populate_scheduled = TRUE;
		return;
	}

	DEBUG_SELECTOR (g_print ("Selector(%p): populate liststore: scheduled\n", selector););

	priv->populate_liststore_is_idle = TRUE;
	gdk_threads_add_idle_full (G_PRIORITY_HIGH_IDLE + 30, real_populate_liststore, selector, NULL);
}

static gboolean
on_treeview_key_press (GtkTreeView               *treeview,
                       GdkEventKey               *event,
                       GeditOpenDocumentSelector *selector)
{
	guint keyval;
	gboolean is_control_pressed;
	GtkTreeSelection *tree_selection;
	GtkTreePath *root_path;
	GdkModifierType modifiers;

	if (gdk_event_get_keyval ((GdkEvent *)event, &keyval) == TRUE)
	{
		tree_selection = gtk_tree_view_get_selection (treeview);
		root_path = gtk_tree_path_new_from_string ("0");

		modifiers = gtk_accelerator_get_default_mod_mask ();
		is_control_pressed = (event->state & modifiers) == GDK_CONTROL_MASK;

		if ((keyval == GDK_KEY_Up || keyval == GDK_KEY_KP_Up) &&
		    !is_control_pressed)
		{
			if (gtk_tree_selection_path_is_selected (tree_selection, root_path))
			{
				gtk_tree_selection_unselect_all (tree_selection);
				gtk_widget_grab_focus (selector->recent_search_entry);

				return GDK_EVENT_STOP;
			}
		}
	}

	return GDK_EVENT_PROPAGATE;
}

static void
on_entry_changed (GtkEntry                  *entry,
                  GeditOpenDocumentSelector *selector)
{
	GeditOpenDocumentSelectorPrivate *priv = selector->priv;
	const gchar *entry_text;

	entry_text = gtk_entry_get_text (entry);
	gedit_open_document_selector_store_set_recent_filter (priv->selector_store,
	                                                      g_utf8_strdown (entry_text, -1));

	if (gtk_widget_get_mapped ( GTK_WIDGET (selector)))
	{
		populate_liststore (selector);
	}
}

static void
on_entry_activated (GtkEntry                  *entry,
                    GeditOpenDocumentSelector *selector)
{
	const gchar *entry_text;
	GtkTreeSelection *selection;
	gchar *uri;
	GFile *file;
	gchar *scheme;

	entry_text = gtk_entry_get_text (entry);
	scheme = g_uri_parse_scheme (entry_text);
	if (!scheme)
	{
		const gchar *home_dir = g_get_home_dir ();

		if ( home_dir != NULL && g_str_has_prefix (entry_text, "~/"))
		{
			uri = g_strconcat ("file://", home_dir, "/", entry_text + 2, NULL);
		}
		else
		{
			uri = g_strconcat ("file://", entry_text, NULL);
		}
	}
	else
	{
		g_free (scheme);
		uri = g_strdup (entry_text);
	}

	file = g_file_new_for_uri (uri);
	if (g_file_query_exists (file, NULL))
	{
		DEBUG_SELECTOR (g_print ("Selector(%p): search entry activated : loading '%s'\n",
		                         selector, uri););

		gtk_entry_set_text (entry, "");
		selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (selector->priv->treeview));
		gtk_tree_selection_unselect_all (selection);

		g_signal_emit (G_OBJECT (selector), signals[SELECTOR_FILE_ACTIVATED], 0, uri);
	}

	g_object_unref (file);
}

static void
gedit_open_document_selector_dispose (GObject *object)
{
	GeditOpenDocumentSelector *selector = GEDIT_OPEN_DOCUMENT_SELECTOR (object);
	GeditOpenDocumentSelectorPrivate *priv = selector->priv;

	while (TRUE)
	{
		if (!g_idle_remove_by_data (selector))
		{
			break;
		}
	}

	if (priv->recent_items)
	{
		gedit_open_document_selector_free_file_items_list (priv->recent_items);
		priv->recent_items = NULL;
	}

	if (priv->home_dir_items)
	{
		gedit_open_document_selector_free_file_items_list (priv->home_dir_items);
		priv->home_dir_items = NULL;
	}

	if (priv->desktop_dir_items)
	{
		gedit_open_document_selector_free_file_items_list (priv->desktop_dir_items);
		priv->desktop_dir_items = NULL;
	}

	if (priv->local_bookmarks_dir_items)
	{
		gedit_open_document_selector_free_file_items_list (priv->local_bookmarks_dir_items);
		priv->local_bookmarks_dir_items = NULL;
	}

	if (priv->file_browser_root_items)
	{
		gedit_open_document_selector_free_file_items_list (priv->file_browser_root_items);
		priv->file_browser_root_items = NULL;
	}

	if (priv->active_doc_dir_items)
	{
		gedit_open_document_selector_free_file_items_list (priv->active_doc_dir_items);
		priv->active_doc_dir_items = NULL;
	}

	if (priv->current_docs_items)
	{
		gedit_open_document_selector_free_file_items_list (priv->current_docs_items);
		priv->current_docs_items = NULL;
	}

	if (priv->all_items)
	{
		gedit_open_document_selector_free_file_items_list (priv->all_items);
		priv->all_items = NULL;
	}

	G_OBJECT_CLASS (gedit_open_document_selector_parent_class)->dispose (object);
}

static void
on_row_activated (GtkTreeView               *treeview,
                  GtkTreePath               *path,
                  GtkTreeViewColumn         *column,
                  GeditOpenDocumentSelector *selector)
{
	GeditOpenDocumentSelectorPrivate *priv = selector->priv;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	gchar *uri;

	gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->liststore), &iter, path);
	gtk_tree_model_get (GTK_TREE_MODEL (priv->liststore),
	                    &iter,
	                    URI_COLUMN, &uri,
	                    -1);

	selection = gtk_tree_view_get_selection (treeview);
	gtk_tree_selection_unselect_all (selection);

	/* Leak of uri */
	g_signal_emit (G_OBJECT (selector), signals[SELECTOR_FILE_ACTIVATED], 0, uri);
}

static void
update_list_cb (GeditOpenDocumentSelectorStore *selector_store,
                GAsyncResult                   *res,
                gpointer                        user_data)
{
	GList *list;
	GError *error;
	PushMessage *message;
	ListType type;
	GeditOpenDocumentSelector *selector;
	GeditOpenDocumentSelectorPrivate *priv;

	list = gedit_open_document_selector_store_update_list_finish (selector_store, res, &error);
	message = g_task_get_task_data (G_TASK (res));
	selector = message->selector;
	priv = selector->priv;
	type = message->type;

	DEBUG_SELECTOR (g_print ("Selector(%p): update_list_cb - type:%s, length:%i\n",
	                         selector, list_type_string[type], g_list_length (list)););

	switch (type)
	{
		case GEDIT_OPEN_DOCUMENT_SELECTOR_RECENT_FILES_LIST:
			gedit_open_document_selector_free_file_items_list (priv->recent_items);
			priv->recent_items = list;
			break;

		case GEDIT_OPEN_DOCUMENT_SELECTOR_HOME_DIR_LIST:
			gedit_open_document_selector_free_file_items_list (priv->home_dir_items);
			priv->home_dir_items = list;
			break;

		case GEDIT_OPEN_DOCUMENT_SELECTOR_DESKTOP_DIR_LIST:
			gedit_open_document_selector_free_file_items_list (priv->desktop_dir_items);
			priv->desktop_dir_items = list;
			break;

		case GEDIT_OPEN_DOCUMENT_SELECTOR_LOCAL_BOOKMARKS_DIR_LIST:
			gedit_open_document_selector_free_file_items_list (priv->local_bookmarks_dir_items);
			priv->local_bookmarks_dir_items = list;
			break;

		case GEDIT_OPEN_DOCUMENT_SELECTOR_FILE_BROWSER_ROOT_DIR_LIST:
			gedit_open_document_selector_free_file_items_list (priv->file_browser_root_items);
			priv->file_browser_root_items = list;
			break;

		case GEDIT_OPEN_DOCUMENT_SELECTOR_ACTIVE_DOC_DIR_LIST:
			gedit_open_document_selector_free_file_items_list (priv->active_doc_dir_items);
			priv->active_doc_dir_items = list;
			break;

		case GEDIT_OPEN_DOCUMENT_SELECTOR_CURRENT_DOCS_LIST:
			gedit_open_document_selector_free_file_items_list (priv->current_docs_items);
			priv->current_docs_items = list;
			break;

		default:
			g_return_if_reached ();
	}

	priv->all_items = compute_all_items_list (selector);
	populate_liststore (selector);
}

static void
gedit_open_document_selector_constructed (GObject *object)
{
	GeditOpenDocumentSelector *selector = GEDIT_OPEN_DOCUMENT_SELECTOR (object);
	GeditOpenDocumentSelectorPrivate *priv = selector->priv;

	G_OBJECT_CLASS (gedit_open_document_selector_parent_class)->constructed (object);

	DEBUG_SELECTOR (g_print ("Selector(%p): constructed - ask recent file list\n", selector););

	gedit_open_document_selector_store_update_list_async (priv->selector_store,
	                                                      selector,
	                                                      NULL,
	                                                      (GAsyncReadyCallback)update_list_cb,
	                                                      GEDIT_OPEN_DOCUMENT_SELECTOR_RECENT_FILES_LIST,
	                                                      selector);
}

static void
gedit_open_document_selector_mapped (GtkWidget *widget)
{
	GeditOpenDocumentSelector *selector = GEDIT_OPEN_DOCUMENT_SELECTOR (widget);
	GeditOpenDocumentSelectorPrivate *priv = selector->priv;
	ListType list_number;

	/* We update all the lists */
	DEBUG_SELECTOR (g_print ("Selector(%p): mapped - ask all lists\n", selector););

	for (list_number = 0; list_number < GEDIT_OPEN_DOCUMENT_SELECTOR_LIST_TYPE_NUM_OF_LISTS; list_number++)
	{
		gedit_open_document_selector_store_update_list_async (priv->selector_store,
		                                                      selector,
		                                                      NULL,
		                                                      (GAsyncReadyCallback)update_list_cb,
		                                                      list_number,
		                                                      selector);
	}

	GTK_WIDGET_CLASS (gedit_open_document_selector_parent_class)->map (widget);
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
gedit_open_document_selector_set_property (GObject      *object,
                                           guint         prop_id,
                                           const GValue *value,
                                           GParamSpec   *pspec)
{
	GeditOpenDocumentSelector *selector = GEDIT_OPEN_DOCUMENT_SELECTOR (object);

	switch (prop_id)
	{
		case PROP_WINDOW:
			selector->window = g_value_get_object (value);
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
	GeditOpenDocumentSelector *selector = GEDIT_OPEN_DOCUMENT_SELECTOR (object);

	switch (prop_id)
	{
		case PROP_WINDOW:
			g_value_set_object (value, selector->window);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gedit_open_document_selector_class_init (GeditOpenDocumentSelectorClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	gobject_class->constructed = gedit_open_document_selector_constructed;
	gobject_class->dispose = gedit_open_document_selector_dispose;

	gobject_class->get_property = gedit_open_document_selector_get_property;
	gobject_class->set_property = gedit_open_document_selector_set_property;

	widget_class->get_request_mode = gedit_open_document_selector_get_request_mode;
	widget_class->get_preferred_width = gedit_open_document_selector_get_preferred_width;
	widget_class->map = gedit_open_document_selector_mapped;

	signals[SELECTOR_FILE_ACTIVATED] =
		g_signal_new ("file-activated",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		              G_STRUCT_OFFSET (GeditOpenDocumentSelectorClass, selector_file_activated),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__STRING,
		              G_TYPE_NONE, 1,
		              G_TYPE_STRING);

	gtk_widget_class_set_template_from_resource (widget_class,
	                                             "/org/gnome/gedit/ui/gedit-open-document-selector.ui");

	gtk_widget_class_bind_template_child_private (widget_class, GeditOpenDocumentSelector, open_button);
	gtk_widget_class_bind_template_child_private (widget_class, GeditOpenDocumentSelector, treeview);
	gtk_widget_class_bind_template_child_private (widget_class, GeditOpenDocumentSelector, placeholder_box);
	gtk_widget_class_bind_template_child_private (widget_class, GeditOpenDocumentSelector, scrolled_window);
	gtk_widget_class_bind_template_child (widget_class, GeditOpenDocumentSelector, recent_search_entry);

	g_object_class_install_property (gobject_class,
	                                 PROP_WINDOW,
	                                 g_param_spec_object ("window",
	                                                      "Window",
	                                                      "The GeditWindow this GeditOpenDocumentSelector is associated with",
	                                                      GEDIT_TYPE_WINDOW,
	                                                      G_PARAM_READWRITE |
	                                                      G_PARAM_CONSTRUCT_ONLY |
	                                                      G_PARAM_STATIC_STRINGS));
}

static void
on_treeview_allocate (GtkWidget                 *widget,
                      GdkRectangle              *allocation,
                      GeditOpenDocumentSelector *selector)
{
	GeditOpenDocumentSelectorPrivate *priv = selector->priv;
	GeditOpenDocumentSelectorStore *selector_store = priv->selector_store;
	GtkStyleContext *context;
	gint name_renderer_natural_size;
	gint path_renderer_natural_size;
	GtkBorder padding;
	gint ypad;
	gint limit_capped;
	gint treeview_height;
	gint grid_line_width;
	gint row_height;
	gint recent_limit;

	context = gtk_widget_get_style_context (priv->treeview);

	/* Treeview height computation */
	gtk_cell_renderer_get_preferred_height (priv->name_renderer,
	                                        priv->treeview,
	                                        NULL,
	                                        &name_renderer_natural_size);

	gtk_cell_renderer_get_preferred_height (priv->path_renderer,
	                                        priv->treeview,
	                                        NULL,
	                                        &path_renderer_natural_size);

	gtk_style_context_get_padding (context, GTK_STATE_FLAG_NORMAL, &padding);
	gtk_cell_renderer_get_padding (priv->name_renderer, NULL, &ypad);
	gtk_widget_style_get (priv->treeview, "grid-line-width", &grid_line_width, NULL);

	recent_limit = gedit_open_document_selector_store_get_recent_limit (selector_store);

	limit_capped = (recent_limit > 0 ) ? MIN (recent_limit, OPEN_DOCUMENT_SELECTOR_MAX_VISIBLE_ROWS) :
	                                     OPEN_DOCUMENT_SELECTOR_MAX_VISIBLE_ROWS;

	row_height = name_renderer_natural_size +
	             path_renderer_natural_size +
	             2 * (padding.top + padding.bottom) +
	             ypad +
	             grid_line_width;

	treeview_height = row_height * limit_capped;
	gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (priv->scrolled_window),
	                                            treeview_height);

	gtk_widget_set_size_request (priv->placeholder_box, -1, treeview_height);
}

static void
name_renderer_datafunc (GtkTreeViewColumn         *column,
                        GtkCellRenderer           *name_renderer,
                        GtkTreeModel              *liststore,
                        GtkTreeIter               *iter,
                        GeditOpenDocumentSelector *selector)
{
	GeditOpenDocumentSelectorPrivate *priv = selector->priv;
	GtkStyleContext *context;
	GdkRGBA label_color;
	gdouble font_size;

	context = gtk_widget_get_style_context (priv->treeview);

	/* Name label foreground and font size styling */
	gtk_style_context_add_class (context, "open-document-selector-name-label");

	gtk_style_context_get_color (context, GTK_STATE_FLAG_NORMAL, &label_color);
	g_object_set (priv->name_renderer, "foreground-rgba", &label_color, NULL);

	gtk_style_context_get (context, GTK_STATE_FLAG_NORMAL, "font-size", &font_size, NULL);
	g_object_set (priv->name_renderer, "size-points", font_size, NULL);

	gtk_style_context_remove_class (context, "open-document-selector-name-label");
}

static void
path_renderer_datafunc (GtkTreeViewColumn         *column,
                        GtkCellRenderer           *path_renderer,
                        GtkTreeModel              *liststore,
                        GtkTreeIter               *iter,
                        GeditOpenDocumentSelector *selector)
{
	GeditOpenDocumentSelectorPrivate *priv = selector->priv;
	GtkStyleContext *context;
	GdkRGBA label_color;
	gdouble font_size;

	context = gtk_widget_get_style_context (priv->treeview);

	/* Path label foreground and font size styling */
	gtk_style_context_add_class (context, "open-document-selector-path-label");

	gtk_style_context_get_color (context, GTK_STATE_FLAG_NORMAL, &label_color);
	g_object_set (priv->path_renderer, "foreground-rgba", &label_color, NULL);

	gtk_style_context_get (context, GTK_STATE_FLAG_NORMAL, "font-size", &font_size, NULL);
	g_object_set (priv->path_renderer, "size-points", font_size, NULL);

	gtk_style_context_remove_class (context, "open-document-selector-path-label");
}

static void
setup_treeview (GeditOpenDocumentSelector *selector)
{
	GeditOpenDocumentSelectorPrivate *priv = selector->priv;
	GtkTreeViewColumn *column;
	GtkCellArea *cell_area;
	GtkStyleContext *context;

	gtk_tree_view_set_model (GTK_TREE_VIEW (priv->treeview), GTK_TREE_MODEL (priv->liststore));
	g_object_unref(GTK_TREE_MODEL (priv->liststore));

	priv->name_renderer = gtk_cell_renderer_text_new ();
	priv->path_renderer = gtk_cell_renderer_text_new ();

	g_object_set (priv->name_renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	g_object_set (priv->path_renderer, "ellipsize", PANGO_ELLIPSIZE_START, NULL);

	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_FIXED);

	gtk_tree_view_column_pack_start (column, priv->name_renderer, TRUE);
	gtk_tree_view_column_pack_start (column, priv->path_renderer, TRUE);

	gtk_tree_view_column_set_attributes (column, priv->name_renderer, "markup", NAME_COLUMN, NULL);
	gtk_tree_view_column_set_attributes (column, priv->path_renderer, "markup", PATH_COLUMN, NULL);

	gtk_tree_view_append_column (GTK_TREE_VIEW (priv->treeview), column);
	cell_area = gtk_cell_layout_get_area (GTK_CELL_LAYOUT (column));
	gtk_orientable_set_orientation (GTK_ORIENTABLE (cell_area), GTK_ORIENTATION_VERTICAL);

	context = gtk_widget_get_style_context (priv->treeview);
	gtk_style_context_add_class (context, "open-document-selector-treeview");

	gtk_tree_view_column_set_cell_data_func (column,
	                                         priv->name_renderer,
	                                         (GtkTreeCellDataFunc)name_renderer_datafunc,
	                                         selector,
	                                         NULL);

	gtk_tree_view_column_set_cell_data_func (column,
	                                         priv->path_renderer,
	                                         (GtkTreeCellDataFunc)path_renderer_datafunc,
	                                         selector,
	                                         NULL);
}

static void
gedit_open_document_selector_init (GeditOpenDocumentSelector *selector)
{
	GeditOpenDocumentSelectorPrivate *priv;

	gedit_debug (DEBUG_WINDOW);

	selector->priv = gedit_open_document_selector_get_instance_private (selector);
	priv = selector->priv;

	gtk_widget_init_template (GTK_WIDGET (selector));

	priv->selector_store = gedit_open_document_selector_store_get_default ();

	priv->liststore = gtk_list_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
	setup_treeview (selector);

	g_signal_connect (selector->recent_search_entry,
	                  "changed",
	                  G_CALLBACK (on_entry_changed),
	                  selector);

	g_signal_connect (selector->recent_search_entry,
	                  "activate",
	                  G_CALLBACK (on_entry_activated),
	                  selector);

	g_signal_connect (priv->treeview,
	                  "row-activated",
	                  G_CALLBACK (on_row_activated),
	                  selector);

	g_signal_connect (priv->treeview,
	                  "size-allocate",
	                  G_CALLBACK (on_treeview_allocate),
	                  selector);

	g_signal_connect (priv->treeview,
	                  "key-press-event",
	                  G_CALLBACK (on_treeview_key_press),
	                  selector);
}

GeditOpenDocumentSelector *
gedit_open_document_selector_new (GeditWindow *window)
{
	g_return_val_if_fail (GEDIT_IS_WINDOW (window), NULL);

	return g_object_new (GEDIT_TYPE_OPEN_DOCUMENT_SELECTOR,
	                     "window", window,
	                     NULL);
}

/* ex:set ts=8 noet: */
