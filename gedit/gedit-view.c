/*
 * gedit-view.c
 * This file is part of gedit
 *
 * Copyright (C) 1998, 1999 Alex Roberts, Evan Lawrence
 * Copyright (C) 2000, 2002 Chema Celorio, Paolo Maggi
 * Copyright (C) 2003-2005 Paolo Maggi
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
 * Modified by the gedit Team, 1998-2005. See the AUTHORS file for a
 * list of people on the gedit Team.
 * See the ChangeLog files for a list of changes.
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <stdlib.h>

#include <gdk/gdkkeysyms.h>
#include <libpeas/peas-extension-set.h>

#include <glib/gi18n.h>

#include "gedit-view.h"
#include "gedit-view-activatable.h"
#include "gedit-plugins-engine.h"
#include "gedit-debug.h"
#include "gedit-marshal.h"
#include "gedit-utils.h"
#include "gedit-settings.h"
#include "gedit-app.h"
#include "gedit-notebook.h"

#define GEDIT_VIEW_SCROLL_MARGIN 0.02

enum
{
	TARGET_URI_LIST = 100,
	TARGET_XDNDDIRECTSAVE,
	TARGET_TAB
};

struct _GeditViewPrivate
{
	GSettings *editor_settings;
	GtkTextBuffer *current_buffer;
	PeasExtensionSet *extensions;
	gchar *direct_save_uri;
};

G_DEFINE_TYPE_WITH_PRIVATE (GeditView, gedit_view, GTK_SOURCE_TYPE_VIEW)

/* Signals */
enum
{
	DROP_URIS,
	LAST_SIGNAL
};

static guint view_signals [LAST_SIGNAL] = { 0 };

static void
document_read_only_notify_handler (GeditDocument *document,
			           GParamSpec    *pspec,
				   GeditView     *view)
{
	gedit_debug (DEBUG_VIEW);

	gtk_text_view_set_editable (GTK_TEXT_VIEW (view),
				    !gedit_document_get_readonly (document));
}

static void
current_buffer_removed (GeditView *view)
{
	if (view->priv->current_buffer)
	{
		g_signal_handlers_disconnect_by_func (view->priv->current_buffer,
						      document_read_only_notify_handler,
						      view);

		g_object_unref (view->priv->current_buffer);
		view->priv->current_buffer = NULL;
	}
}

static void
on_notify_buffer_cb (GeditView  *view,
		     GParamSpec *arg1,
		     gpointer    userdata)
{
	GtkTextBuffer *buffer;

	current_buffer_removed (view);
	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

	if (buffer == NULL || !GEDIT_IS_DOCUMENT (buffer))
		return;

	view->priv->current_buffer = g_object_ref (buffer);
	g_signal_connect (buffer,
			  "notify::read-only",
			  G_CALLBACK (document_read_only_notify_handler),
			  view);

	gtk_text_view_set_editable (GTK_TEXT_VIEW (view),
				    !gedit_document_get_readonly (GEDIT_DOCUMENT (buffer)));
}

static void
gedit_view_init (GeditView *view)
{
	GtkTargetList *tl;

	gedit_debug (DEBUG_VIEW);

	view->priv = gedit_view_get_instance_private (view);

	view->priv->editor_settings = g_settings_new ("org.gnome.gedit.preferences.editor");

	/* Drag and drop support */
	view->priv->direct_save_uri = NULL;
	tl = gtk_drag_dest_get_target_list (GTK_WIDGET (view));

	if (tl != NULL)
	{
		gtk_target_list_add (tl,
		                     gdk_atom_intern ("XdndDirectSave0", FALSE),
		                     0,
		                     TARGET_XDNDDIRECTSAVE);
		gtk_target_list_add_uri_targets (tl, TARGET_URI_LIST);
		gtk_target_list_add (tl,
		                     gdk_atom_intern_static_string ("GTK_NOTEBOOK_TAB"),
		                     GTK_TARGET_SAME_APP,
		                     TARGET_TAB);
	}

	view->priv->extensions =
		peas_extension_set_new (PEAS_ENGINE (gedit_plugins_engine_get_default ()),
		                        GEDIT_TYPE_VIEW_ACTIVATABLE,
		                        "view", view,
		                        NULL);

	/* Act on buffer change */
	g_signal_connect (view,
			  "notify::buffer",
			  G_CALLBACK (on_notify_buffer_cb),
			  NULL);
}

static void
gedit_view_destroy (GtkWidget *widget)
{
	GeditView *view = GEDIT_VIEW (widget);

	/* Disconnect notify buffer because the destroy of the textview will
	   set the buffer to NULL, and we call get_buffer in the notify which
	   would reinstate a GtkTextBuffer which we don't want */
	current_buffer_removed (view);
	g_signal_handlers_disconnect_by_func (view, on_notify_buffer_cb, NULL);

	GTK_WIDGET_CLASS (gedit_view_parent_class)->destroy (widget);
}

static void
gedit_view_dispose (GObject *object)
{
	GeditView *view = GEDIT_VIEW (object);

	g_clear_object (&view->priv->extensions);
	g_clear_object (&view->priv->editor_settings);

	G_OBJECT_CLASS (gedit_view_parent_class)->dispose (object);
}

static void
gedit_view_finalize (GObject *object)
{
	GeditView *view = GEDIT_VIEW (object);

	current_buffer_removed (view);

	G_OBJECT_CLASS (gedit_view_parent_class)->finalize (object);
}

static void
gedit_view_constructed (GObject *object)
{
	GeditView *view;
	GeditViewPrivate *priv;
	gboolean use_default_font;

	view = GEDIT_VIEW (object);
	priv = view->priv;

	/* Get setting values */
	use_default_font = g_settings_get_boolean (view->priv->editor_settings,
	                                           GEDIT_SETTINGS_USE_DEFAULT_FONT);

	/*
	 *  Set tab, fonts, wrap mode, colors, etc. according
	 *  to preferences
	 */
	if (!use_default_font)
	{
		gchar *editor_font;

		editor_font = g_settings_get_string (view->priv->editor_settings,
		                                     GEDIT_SETTINGS_EDITOR_FONT);

		gedit_view_set_font (view, FALSE, editor_font);

		g_free (editor_font);
	}
	else
	{
		gedit_view_set_font (view, TRUE, NULL);
	}

	g_settings_bind (priv->editor_settings,
	                 GEDIT_SETTINGS_DISPLAY_LINE_NUMBERS,
	                 view,
	                 "show-line-numbers",
	                 G_SETTINGS_BIND_GET);

	g_settings_bind (priv->editor_settings,
	                 GEDIT_SETTINGS_AUTO_INDENT,
	                 view,
	                 "auto-indent",
	                 G_SETTINGS_BIND_GET);

	g_settings_bind (priv->editor_settings,
	                 GEDIT_SETTINGS_TABS_SIZE,
	                 view,
	                 "tab-width",
	                 G_SETTINGS_BIND_GET);

	g_settings_bind (priv->editor_settings,
	                 GEDIT_SETTINGS_INSERT_SPACES,
	                 view,
	                 "insert-spaces-instead-of-tabs",
	                 G_SETTINGS_BIND_GET);

	g_settings_bind (priv->editor_settings,
	                 GEDIT_SETTINGS_DISPLAY_RIGHT_MARGIN,
	                 view,
	                 "show-right-margin",
	                 G_SETTINGS_BIND_GET);

	g_settings_bind (priv->editor_settings,
	                 GEDIT_SETTINGS_RIGHT_MARGIN_POSITION,
	                 view,
	                 "right-margin-position",
	                 G_SETTINGS_BIND_GET);

	g_settings_bind (priv->editor_settings,
	                 GEDIT_SETTINGS_HIGHLIGHT_CURRENT_LINE,
	                 view,
	                 "highlight-current-line",
	                 G_SETTINGS_BIND_GET);

	g_settings_bind (priv->editor_settings,
	                 GEDIT_SETTINGS_WRAP_MODE,
	                 view,
	                 "wrap-mode",
	                 G_SETTINGS_BIND_GET);

	g_settings_bind (priv->editor_settings,
	                 GEDIT_SETTINGS_SMART_HOME_END,
	                 view,
	                 "smart-home-end",
	                 G_SETTINGS_BIND_GET);

	g_object_set (G_OBJECT (view),
	              "indent_on_tab", TRUE,
	              NULL);

	G_OBJECT_CLASS (gedit_view_parent_class)->constructed (object);
}

static gint
gedit_view_focus_out (GtkWidget *widget, GdkEventFocus *event)
{
	gtk_widget_queue_draw (widget);

	GTK_WIDGET_CLASS (gedit_view_parent_class)->focus_out_event (widget, event);

	return FALSE;
}

static GdkAtom
drag_get_uri_target (GtkWidget      *widget,
		     GdkDragContext *context)
{
	GdkAtom target;
	GtkTargetList *tl;

	tl = gtk_target_list_new (NULL, 0);
	gtk_target_list_add_uri_targets (tl, 0);

	target = gtk_drag_dest_find_target (widget, context, tl);
	gtk_target_list_unref (tl);

	return target;
}

static gboolean
gedit_view_drag_motion (GtkWidget      *widget,
			GdkDragContext *context,
			gint            x,
			gint            y,
			guint           timestamp)
{
	gboolean result;

	/* Chain up to allow textview to scroll and position dnd mark, note
	 * that this needs to be checked if gtksourceview or gtktextview
	 * changes drag_motion behaviour */
	result = GTK_WIDGET_CLASS (gedit_view_parent_class)->drag_motion (widget, context, x, y, timestamp);

	/* If this is a URL, deal with it here */
	if (drag_get_uri_target (widget, context) != GDK_NONE)
	{
		gdk_drag_status (context,
				 gdk_drag_context_get_suggested_action (context),
				 timestamp);
		result = TRUE;
	}

	return result;
}

static GtkWidget *
get_notebook_from_view (GtkWidget *view)
{
	GtkWidget *widget;

	widget = view;

	do
	{
		widget = gtk_widget_get_parent (widget);
	}
	while (!GEDIT_IS_NOTEBOOK (widget));

	return widget;
}

static void
gedit_view_drag_data_received (GtkWidget        *widget,
		       	       GdkDragContext   *context,
			       gint              x,
			       gint              y,
			       GtkSelectionData *selection_data,
			       guint             info,
			       guint             timestamp)
{
	/* If this is an URL emit DROP_URIS, otherwise chain up the signal */
	switch (info)
	{
		case TARGET_URI_LIST:
		{
			gchar **uri_list;

			uri_list = gedit_utils_drop_get_uris (selection_data);

			if (uri_list != NULL)
			{
				g_signal_emit (widget, view_signals[DROP_URIS], 0, uri_list);
				g_strfreev (uri_list);

				gtk_drag_finish (context, TRUE, FALSE, timestamp);
			}

			break;

		}
		case TARGET_TAB:
		{
			GtkWidget *notebook;
			GtkWidget *new_notebook;
			GtkWidget *page;

			notebook = gtk_drag_get_source_widget (context);

			if (!GTK_IS_WIDGET (notebook))
			{
				return;
			}

			page = *(GtkWidget **) gtk_selection_data_get_data (selection_data);
			g_return_if_fail (page != NULL);

			/* We need to iterate and get the notebook of the target view
			   because we can have several notebooks per window */
			new_notebook = get_notebook_from_view (widget);

			if (notebook != new_notebook)
			{
				gedit_notebook_move_tab (GEDIT_NOTEBOOK (notebook),
							 GEDIT_NOTEBOOK (new_notebook),
							 GEDIT_TAB (page),
							 0);
			}

			gtk_drag_finish (context, TRUE, TRUE, timestamp);

			break;
		}
		case TARGET_XDNDDIRECTSAVE:
		{
			GeditView *view;

			view = GEDIT_VIEW (widget);

			/* Indicate that we don't provide "F" fallback */
			if (gtk_selection_data_get_format (selection_data) == 8 &&
			    gtk_selection_data_get_length (selection_data) == 1 &&
			    gtk_selection_data_get_data (selection_data)[0] == 'F')
			{
				gdk_property_change (gdk_drag_context_get_source_window (context),
						     gdk_atom_intern ("XdndDirectSave0", FALSE),
						     gdk_atom_intern ("text/plain", FALSE), 8,
						     GDK_PROP_MODE_REPLACE, (const guchar *) "", 0);
			}
			else if (gtk_selection_data_get_format (selection_data) == 8 &&
				 gtk_selection_data_get_length (selection_data) == 1 &&
				 gtk_selection_data_get_data (selection_data)[0] == 'S' &&
				 view->priv->direct_save_uri != NULL)
			{
				gchar **uris;

				uris = g_new (gchar *, 2);
				uris[0] = view->priv->direct_save_uri;
				uris[1] = NULL;
				g_signal_emit (widget, view_signals[DROP_URIS], 0, uris);
				g_free (uris);
			}

			g_free (view->priv->direct_save_uri);
			view->priv->direct_save_uri = NULL;

			gtk_drag_finish (context, TRUE, FALSE, timestamp);

			break;
		}
		default:
		{
			GTK_WIDGET_CLASS (gedit_view_parent_class)->drag_data_received (widget,
											context,
											x, y,
											selection_data,
											info,
											timestamp);
			break;
		}
	}
}

static gboolean
gedit_view_drag_drop (GtkWidget      *widget,
		      GdkDragContext *context,
		      gint            x,
		      gint            y,
		      guint           timestamp)
{
	gboolean result;
	GdkAtom target;
	guint info;
	gboolean found;
	GtkTargetList *target_list;

	target_list = gtk_drag_dest_get_target_list (widget);
	target = gtk_drag_dest_find_target (widget, context, target_list);
	found = gtk_target_list_find (target_list, target, &info);

	if (found && (info == TARGET_URI_LIST || info == TARGET_XDNDDIRECTSAVE))
	{
		if (info == TARGET_XDNDDIRECTSAVE)
		{
			gchar *uri;
			uri = gedit_utils_set_direct_save_filename (context);

			if (uri != NULL)
			{
				g_free (GEDIT_VIEW (widget)->priv->direct_save_uri);
				GEDIT_VIEW (widget)->priv->direct_save_uri = uri;
			}
		}

		gtk_drag_get_data (widget, context, target, timestamp);
		result = TRUE;
	}
	else
	{
		/* Chain up */
		result = GTK_WIDGET_CLASS (gedit_view_parent_class)->drag_drop (widget,
										context,
										x, y,
										timestamp);
	}

	return result;
}

static GtkWidget *
create_line_numbers_menu (GtkWidget *view)
{
	GtkWidget *menu;
	GtkWidget *item;

	menu = gtk_menu_new ();

	item = gtk_check_menu_item_new_with_mnemonic (_("_Display line numbers"));
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item),
					gtk_source_view_get_show_line_numbers (GTK_SOURCE_VIEW (view)));

	g_settings_bind (GEDIT_VIEW (view)->priv->editor_settings,
			 GEDIT_SETTINGS_DISPLAY_LINE_NUMBERS,
			 item,
			 "active",
			 G_SETTINGS_BIND_SET);

	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

	gtk_widget_show_all (menu);

	return menu;
}

static void
show_line_numbers_menu (GtkWidget      *view,
			GdkEventButton *event)
{
	GtkWidget *menu;

	menu = create_line_numbers_menu (view);

	gtk_menu_popup (GTK_MENU (menu),
			NULL,
			NULL,
			NULL,
			NULL,
			event->button,
			event->time);
}

static gboolean
gedit_view_button_press_event (GtkWidget *widget, GdkEventButton *event)
{
	if ((event->type == GDK_BUTTON_PRESS) &&
	    (event->button == GDK_BUTTON_SECONDARY) &&
	    (event->window == gtk_text_view_get_window (GTK_TEXT_VIEW (widget),
						        GTK_TEXT_WINDOW_LEFT)))
	{
		show_line_numbers_menu (widget, event);

		return TRUE;
	}

	return GTK_WIDGET_CLASS (gedit_view_parent_class)->button_press_event (widget, event);
}

static void
extension_added (PeasExtensionSet *extensions,
		 PeasPluginInfo   *info,
		 PeasExtension    *exten,
		 GeditView        *view)
{
	gedit_view_activatable_activate (GEDIT_VIEW_ACTIVATABLE (exten));
}

static void
extension_removed (PeasExtensionSet *extensions,
		   PeasPluginInfo   *info,
		   PeasExtension    *exten,
		   GeditView        *view)
{
	gedit_view_activatable_deactivate (GEDIT_VIEW_ACTIVATABLE (exten));
}

static void
gedit_view_realize (GtkWidget *widget)
{
	GeditView *view = GEDIT_VIEW (widget);

	GTK_WIDGET_CLASS (gedit_view_parent_class)->realize (widget);

	g_signal_connect (view->priv->extensions,
	                  "extension-added",
	                  G_CALLBACK (extension_added),
	                  view);
	g_signal_connect (view->priv->extensions,
	                  "extension-removed",
	                  G_CALLBACK (extension_removed),
	                  view);

	/* We only activate the extensions when the view is realized,
	 * because most plugins will expect this behaviour, and we won't
	 * change the buffer later anyway. */
	peas_extension_set_foreach (view->priv->extensions,
	                            (PeasExtensionSetForeachFunc) extension_added,
	                            view);
}

static void
gedit_view_unrealize (GtkWidget *widget)
{
	GeditView *view = GEDIT_VIEW (widget);

	g_signal_handlers_disconnect_by_func (view->priv->extensions, extension_added, view);
	g_signal_handlers_disconnect_by_func (view->priv->extensions, extension_removed, view);

	/* We need to deactivate the extension on unrealize because it is not
	   mandatory that a view has been realized when we dispose it, leading
	   to deactivating the plugin without being activated */
	peas_extension_set_foreach (view->priv->extensions,
	                            (PeasExtensionSetForeachFunc) extension_removed,
	                            view);

	GTK_WIDGET_CLASS (gedit_view_parent_class)->unrealize (widget);
}

static void
delete_line (GtkTextView *text_view,
	     gint         count)
{
	GtkTextIter start;
	GtkTextIter end;
	GtkTextBuffer *buffer;

	buffer = gtk_text_view_get_buffer (text_view);

	gtk_text_view_reset_im_context (text_view);

	/* If there is a selection delete the selected lines and
	 * ignore count */
	if (gtk_text_buffer_get_selection_bounds (buffer, &start, &end))
	{
		gtk_text_iter_order (&start, &end);

		if (gtk_text_iter_starts_line (&end))
		{
			/* Do no delete the line with the cursor if the cursor
			 * is at the beginning of the line */
			count = 0;
		}
		else
		{
			count = 1;
		}
	}

	gtk_text_iter_set_line_offset (&start, 0);

	if (count > 0)
	{
		gtk_text_iter_forward_lines (&end, count);

		if (gtk_text_iter_is_end (&end))
		{
			if (gtk_text_iter_backward_line (&start) && !gtk_text_iter_ends_line (&start))
				gtk_text_iter_forward_to_line_end (&start);
		}
	}
	else if (count < 0)
	{
		if (!gtk_text_iter_ends_line (&end))
			gtk_text_iter_forward_to_line_end (&end);

		while (count < 0)
		{
			if (!gtk_text_iter_backward_line (&start))
				break;

			++count;
		}

		if (count == 0)
		{
			if (!gtk_text_iter_ends_line (&start))
				gtk_text_iter_forward_to_line_end (&start);
		}
		else
		{
			gtk_text_iter_forward_line (&end);
		}
	}

	if (!gtk_text_iter_equal (&start, &end))
	{
		GtkTextIter cur = start;
		gtk_text_iter_set_line_offset (&cur, 0);

		gtk_text_buffer_begin_user_action (buffer);

		gtk_text_buffer_place_cursor (buffer, &cur);

		gtk_text_buffer_delete_interactive (buffer,
						    &start,
						    &end,
						    gtk_text_view_get_editable (text_view));

		gtk_text_buffer_end_user_action (buffer);

		gtk_text_view_scroll_mark_onscreen (text_view,
						    gtk_text_buffer_get_insert (buffer));
	}
	else
	{
		gtk_widget_error_bell (GTK_WIDGET (text_view));
	}
}

static void
gedit_view_delete_from_cursor (GtkTextView   *text_view,
			       GtkDeleteType  type,
			       gint           count)
{
	/* We override the standard handler for delete_from_cursor since
	   the GTK_DELETE_PARAGRAPHS case is not implemented as we like (i.e. it
	   does not remove the carriage return in the previous line)
	 */
	switch (type)
	{
		case GTK_DELETE_PARAGRAPHS:
			delete_line (text_view, count);
			break;
		default:
			GTK_TEXT_VIEW_CLASS (gedit_view_parent_class)->delete_from_cursor(text_view, type, count);
			break;
	}
}

static void
gedit_view_class_init (GeditViewClass *klass)
{
	GObjectClass     *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass   *widget_class = GTK_WIDGET_CLASS (klass);
	GtkTextViewClass *text_view_class = GTK_TEXT_VIEW_CLASS (klass);
	GtkBindingSet    *binding_set;

	object_class->dispose = gedit_view_dispose;
	object_class->finalize = gedit_view_finalize;
	object_class->constructed = gedit_view_constructed;

	widget_class->destroy = gedit_view_destroy;
	widget_class->focus_out_event = gedit_view_focus_out;

	/*
	 * Override the gtk_text_view_drag_motion and drag_drop
	 * functions to get URIs
	 *
	 * If the mime type is text/uri-list, then we will accept
	 * the potential drop, or request the data (depending on the
	 * function).
	 *
	 * If the drag context has any other mime type, then pass the
	 * information onto the GtkTextView's standard handlers.
	 * (widget_class->function_name).
	 *
	 * See bug #89881 for details
	 */
	widget_class->drag_motion = gedit_view_drag_motion;
	widget_class->drag_data_received = gedit_view_drag_data_received;
	widget_class->drag_drop = gedit_view_drag_drop;
	widget_class->button_press_event = gedit_view_button_press_event;
	widget_class->realize = gedit_view_realize;
	widget_class->unrealize = gedit_view_unrealize;

	text_view_class->delete_from_cursor = gedit_view_delete_from_cursor;

	/* A new signal DROP_URIS has been added to allow plugins to intercept
	 * the default dnd behaviour of 'text/uri-list'. GeditView now handles
	 * dnd in the default handlers of drag_drop, drag_motion and
	 * drag_data_received. The view emits drop_uris from drag_data_received
	 * if valid uris have been dropped. Plugins should connect to
	 * drag_motion, drag_drop and drag_data_received to change this
	 * default behaviour. They should _NOT_ use this signal because this
	 * will not prevent gedit from loading the uri
	 */
	view_signals[DROP_URIS] =
		g_signal_new ("drop_uris",
		              G_TYPE_FROM_CLASS (object_class),
		              G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		              G_STRUCT_OFFSET (GeditViewClass, drop_uris),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__BOXED,
		              G_TYPE_NONE, 1, G_TYPE_STRV);

	binding_set = gtk_binding_set_by_class (klass);

	gtk_binding_entry_add_signal (binding_set,
	                              GDK_KEY_d,
	                              GDK_CONTROL_MASK,
	                              "delete_from_cursor", 2,
	                              G_TYPE_ENUM, GTK_DELETE_PARAGRAPHS,
	                              G_TYPE_INT, 1);
}

/**
 * gedit_view_new:
 * @doc: a #GeditDocument
 *
 * Creates a new #GeditView object displaying the @doc document.
 * @doc cannot be %NULL.
 *
 * Return value: a new #GeditView
 **/
GtkWidget *
gedit_view_new (GeditDocument *doc)
{
	g_return_val_if_fail (GEDIT_IS_DOCUMENT (doc), NULL);

	return GTK_WIDGET (g_object_new (GEDIT_TYPE_VIEW, "buffer", doc, NULL));
}

void
gedit_view_cut_clipboard (GeditView *view)
{
	GtkTextBuffer *buffer;
	GtkClipboard *clipboard;

	gedit_debug (DEBUG_VIEW);

	g_return_if_fail (GEDIT_IS_VIEW (view));

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
	g_return_if_fail (buffer != NULL);

	clipboard = gtk_widget_get_clipboard (GTK_WIDGET (view),
	                                      GDK_SELECTION_CLIPBOARD);

	/* FIXME: what is default editability of a buffer? */
	gtk_text_buffer_cut_clipboard (buffer,
	                               clipboard,
	                               !gedit_document_get_readonly (
	                               		GEDIT_DOCUMENT (buffer)));

	gtk_text_view_scroll_to_mark (GTK_TEXT_VIEW (view),
	                              gtk_text_buffer_get_insert (buffer),
	                              GEDIT_VIEW_SCROLL_MARGIN,
	                              FALSE,
	                              0.0,
	                              0.0);
}

void
gedit_view_copy_clipboard (GeditView *view)
{
	GtkTextBuffer *buffer;
	GtkClipboard *clipboard;

	gedit_debug (DEBUG_VIEW);

	g_return_if_fail (GEDIT_IS_VIEW (view));

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
	g_return_if_fail (buffer != NULL);

	clipboard = gtk_widget_get_clipboard (GTK_WIDGET (view),
	                                      GDK_SELECTION_CLIPBOARD);

	gtk_text_buffer_copy_clipboard (buffer, clipboard);

	/* on copy do not scroll, we are already on screen */
}

void
gedit_view_paste_clipboard (GeditView *view)
{
	GtkTextBuffer *buffer;
	GtkClipboard *clipboard;

	gedit_debug (DEBUG_VIEW);

	g_return_if_fail (GEDIT_IS_VIEW (view));

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
	g_return_if_fail (buffer != NULL);

	clipboard = gtk_widget_get_clipboard (GTK_WIDGET (view),
	                                      GDK_SELECTION_CLIPBOARD);

	/* FIXME: what is default editability of a buffer? */
	gtk_text_buffer_paste_clipboard (buffer,
	                                 clipboard,
	                                 NULL,
	                                 !gedit_document_get_readonly (
	                                 	GEDIT_DOCUMENT (buffer)));

	gtk_text_view_scroll_to_mark (GTK_TEXT_VIEW (view),
	                              gtk_text_buffer_get_insert (buffer),
	                              GEDIT_VIEW_SCROLL_MARGIN,
	                              FALSE,
	                              0.0,
	                              0.0);
}

/**
 * gedit_view_delete_selection:
 * @view: a #GeditView
 *
 * Deletes the text currently selected in the #GtkTextBuffer associated
 * to the view and scroll to the cursor position.
 **/
void
gedit_view_delete_selection (GeditView *view)
{
	GtkTextBuffer *buffer;

	gedit_debug (DEBUG_VIEW);

	g_return_if_fail (GEDIT_IS_VIEW (view));

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
	g_return_if_fail (buffer != NULL);

	/* FIXME: what is default editability of a buffer? */
	gtk_text_buffer_delete_selection (buffer,
	                                  TRUE,
	                                  !gedit_document_get_readonly (
	                                  	GEDIT_DOCUMENT (buffer)));

	gtk_text_view_scroll_to_mark (GTK_TEXT_VIEW (view),
	                              gtk_text_buffer_get_insert (buffer),
	                              GEDIT_VIEW_SCROLL_MARGIN,
	                              FALSE,
	                              0.0,
	                              0.0);
}

/**
 * gedit_view_select_all:
 * @view: a #GeditView
 *
 * Selects all the text displayed in the @view.
 **/
void
gedit_view_select_all (GeditView *view)
{
	GtkTextBuffer *buffer ;
	GtkTextIter start, end;

	gedit_debug (DEBUG_VIEW);

	g_return_if_fail (GEDIT_IS_VIEW (view));

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
	g_return_if_fail (buffer != NULL);

	gtk_text_buffer_get_bounds (buffer, &start, &end);
	gtk_text_buffer_select_range (buffer, &start, &end);
}

/**
 * gedit_view_scroll_to_cursor:
 * @view: a #GeditView
 *
 * Scrolls the @view to the cursor position.
 **/
void
gedit_view_scroll_to_cursor (GeditView *view)
{
	GtkTextBuffer* buffer;

	gedit_debug (DEBUG_VIEW);

	g_return_if_fail (GEDIT_IS_VIEW (view));

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
	g_return_if_fail (buffer != NULL);

	gtk_text_view_scroll_to_mark (GTK_TEXT_VIEW (view),
	                              gtk_text_buffer_get_insert (buffer),
	                              0.25,
	                              FALSE,
	                              0.0,
	                              0.0);
}

/* FIXME this is an issue for introspection */
/**
 * gedit_view_set_font:
 * @view: a #GeditView
 * @def: whether to reset the default font
 * @font_name: the name of the font to use
 *
 * If @def is #TRUE, resets the font of the @view to the default font
 * otherwise sets it to @font_name.
 **/
void
gedit_view_set_font (GeditView   *view,
		     gboolean     def,
		     const gchar *font_name)
{
	PangoFontDescription *font_desc;

	gedit_debug (DEBUG_VIEW);

	g_return_if_fail (GEDIT_IS_VIEW (view));

	if (def)
	{
		GObject *settings;
		gchar *font;

		settings = _gedit_app_get_settings (GEDIT_APP (g_application_get_default ()));
		font = gedit_settings_get_system_font (GEDIT_SETTINGS (settings));

		font_desc = pango_font_description_from_string (font);
		g_free (font);
	}
	else
	{
		g_return_if_fail (font_name != NULL);

		font_desc = pango_font_description_from_string (font_name);
	}

	g_return_if_fail (font_desc != NULL);

	gtk_widget_override_font (GTK_WIDGET (view), font_desc);

	pango_font_description_free (font_desc);
}

/* ex:set ts=8 noet: */
