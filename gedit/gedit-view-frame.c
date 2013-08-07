/*
 * gedit-view-frame.c
 * This file is part of gedit
 *
 * Copyright (C) 2010 - Ignacio Casal Quinteiro
 * Copyright (C) 2013 - Sébastien Wilmet
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
 * along with gedit; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gedit-view-frame.h"
#include "gedit-debug.h"
#include "gedit-utils.h"
#include "libgd/gd.h"

#include <gtksourceview/gtksource.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>
#include <stdlib.h>

#define FLUSH_TIMEOUT_DURATION 30 /* in seconds */

#define SEARCH_POPUP_MARGIN 12

typedef enum
{
	GOTO_LINE,
	SEARCH
} SearchMode;

typedef enum
{
	SEARCH_STATE_NORMAL,
	SEARCH_STATE_NOT_FOUND
} SearchState;

struct _GeditViewFramePrivate
{
	GeditView *view;
	SearchMode search_mode;

	/* Where the search has started. When the user presses escape in the
	 * search entry (to cancel the search), we return to the start_mark.
	 */
	GtkTextMark *start_mark;

	/* Used to restore the search state if an incremental search is
	 * cancelled.
	 */
	gchar *old_search_text;

	GtkRevealer *revealer;
	GdTaggedEntry *search_entry;
	GdTaggedEntryTag *entry_tag;
	GtkWidget *go_up_button;
	GtkWidget *go_down_button;

	guint flush_timeout_id;
	glong view_scroll_event_id;
	glong search_entry_focus_out_id;
	glong search_entry_changed_id;
	guint idle_update_entry_tag_id;

	guint disable_popdown : 1;

	/* Used to remember the state of the last incremental search (the
	 * buffer search state may be changed by the search and replace dialog).
	 */
	guint case_sensitive_search : 1;
	guint search_at_word_boundaries : 1;
	guint search_wrap_around : 1;

	/* Used to restore the search state if an incremental search is
	 * cancelled.
	 */
	guint old_case_sensitive_search : 1;
	guint old_search_at_word_boundaries : 1;
	guint old_search_wrap_around : 1;
};

enum
{
	PROP_0,
	PROP_DOCUMENT,
	PROP_VIEW
};

G_DEFINE_TYPE_WITH_PRIVATE (GeditViewFrame, gedit_view_frame, GTK_TYPE_OVERLAY)

static void
gedit_view_frame_finalize (GObject *object)
{
	GeditViewFrame *frame = GEDIT_VIEW_FRAME (object);

	g_free (frame->priv->old_search_text);

	G_OBJECT_CLASS (gedit_view_frame_parent_class)->finalize (object);
}

static void
gedit_view_frame_dispose (GObject *object)
{
	GeditViewFrame *frame = GEDIT_VIEW_FRAME (object);

	if (frame->priv->flush_timeout_id != 0)
	{
		g_source_remove (frame->priv->flush_timeout_id);
		frame->priv->flush_timeout_id = 0;
	}

	if (frame->priv->idle_update_entry_tag_id != 0)
	{
		g_source_remove (frame->priv->idle_update_entry_tag_id);
		frame->priv->idle_update_entry_tag_id = 0;
	}

	g_clear_object (&frame->priv->entry_tag);

	G_OBJECT_CLASS (gedit_view_frame_parent_class)->dispose (object);
}

static void
gedit_view_frame_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
	GeditViewFrame *frame = GEDIT_VIEW_FRAME (object);

	switch (prop_id)
	{
		case PROP_DOCUMENT:
			g_value_set_object (value, gedit_view_frame_get_document (frame));
			break;

		case PROP_VIEW:
			g_value_set_object (value, gedit_view_frame_get_view (frame));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
hide_search_widget (GeditViewFrame *frame,
                    gboolean        cancel)
{
	GtkTextBuffer *buffer;

	if (frame->priv->disable_popdown)
	{
		return;
	}

	g_signal_handler_block (frame->priv->search_entry,
	                        frame->priv->search_entry_focus_out_id);

	if (frame->priv->view_scroll_event_id != 0)
	{
		g_signal_handler_disconnect (frame->priv->view,
		                             frame->priv->view_scroll_event_id);
		frame->priv->view_scroll_event_id = 0;
	}

	if (frame->priv->flush_timeout_id != 0)
	{
		g_source_remove (frame->priv->flush_timeout_id);
		frame->priv->flush_timeout_id = 0;
	}

	gtk_revealer_set_reveal_child (frame->priv->revealer, FALSE);

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (frame->priv->view));

	if (cancel)
	{
		GtkTextIter iter;

		gtk_text_buffer_get_iter_at_mark (buffer, &iter,
		                                  frame->priv->start_mark);
		gtk_text_buffer_place_cursor (buffer, &iter);

		gedit_view_scroll_to_cursor (frame->priv->view);
	}

	gtk_text_buffer_delete_mark (buffer, frame->priv->start_mark);
	frame->priv->start_mark = NULL;

	/* Make sure the view is the one who has the focus when we destroy
	   the search widget */
	gtk_widget_grab_focus (GTK_WIDGET (frame->priv->view));

	g_signal_handler_unblock (frame->priv->search_entry,
	                          frame->priv->search_entry_focus_out_id);
}

static gboolean
search_entry_flush_timeout (GeditViewFrame *frame)
{
	frame->priv->flush_timeout_id = 0;
	hide_search_widget (frame, FALSE);

	return G_SOURCE_REMOVE;
}

static void
renew_flush_timeout (GeditViewFrame *frame)
{
	if (frame->priv->flush_timeout_id != 0)
	{
		g_source_remove (frame->priv->flush_timeout_id);
	}

	frame->priv->flush_timeout_id =
		g_timeout_add_seconds (FLUSH_TIMEOUT_DURATION,
				       (GSourceFunc)search_entry_flush_timeout,
				       frame);
}

static void
set_search_state (GeditViewFrame *frame,
		  SearchState     state)
{
	GtkStyleContext *context;

	context = gtk_widget_get_style_context (GTK_WIDGET (frame->priv->search_entry));

	if (state == SEARCH_STATE_NOT_FOUND)
	{
		gtk_style_context_add_class (context, "not-found");
	}
	else
	{
		gtk_style_context_remove_class (context, "not-found");
	}
}

static void
finish_search (GeditViewFrame    *frame,
	       gboolean           found)
{
	const gchar *entry_text = gtk_entry_get_text (GTK_ENTRY (frame->priv->search_entry));

	if (found || (*entry_text == '\0'))
	{
		gedit_view_scroll_to_cursor (frame->priv->view);

		set_search_state (frame, SEARCH_STATE_NORMAL);
	}
	else
	{
		set_search_state (frame, SEARCH_STATE_NOT_FOUND);
	}
}

static void
start_search_finished (GtkSourceBuffer *buffer,
		       GAsyncResult    *result,
		       GeditViewFrame  *frame)
{
	GtkTextIter match_start;
	GtkTextIter match_end;
	gboolean found;

	found = gtk_source_buffer_forward_search_finish (buffer,
							 result,
							 &match_start,
							 &match_end,
							 NULL);

	if (found)
	{
		gtk_text_buffer_select_range (GTK_TEXT_BUFFER (buffer),
					      &match_start,
					      &match_end);
	}
	else
	{
		GtkTextIter start_at;

		gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer),
						  &start_at,
						  frame->priv->start_mark);

		gtk_text_buffer_select_range (GTK_TEXT_BUFFER (buffer),
					      &start_at,
					      &start_at);
	}

	finish_search (frame, found);
}

static void
start_search (GeditViewFrame *frame)
{
	GtkTextIter start_at;
	GtkSourceBuffer *buffer;

	g_return_if_fail (frame->priv->search_mode == SEARCH);

	buffer = GTK_SOURCE_BUFFER (gedit_view_frame_get_document (frame));

	gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer),
					  &start_at,
					  frame->priv->start_mark);

	gtk_source_buffer_forward_search_async (buffer,
						&start_at,
						NULL,
						(GAsyncReadyCallback)start_search_finished,
						frame);
}

static void
forward_search_finished (GtkSourceBuffer *buffer,
			 GAsyncResult    *result,
			 GeditViewFrame  *frame)
{
	GtkTextIter match_start;
	GtkTextIter match_end;
	gboolean found;

	found = gtk_source_buffer_forward_search_finish (buffer,
							 result,
							 &match_start,
							 &match_end,
							 NULL);

	if (found)
	{
		gtk_text_buffer_select_range (GTK_TEXT_BUFFER (buffer),
					      &match_start,
					      &match_end);
	}

	finish_search (frame, found);
}

static void
forward_search (GeditViewFrame *frame)
{
	GtkTextIter start_at;
	GtkSourceBuffer *buffer;

	g_return_if_fail (frame->priv->search_mode == SEARCH);

	buffer = GTK_SOURCE_BUFFER (gedit_view_frame_get_document (frame));

	gtk_text_buffer_get_selection_bounds (GTK_TEXT_BUFFER (buffer),
					      NULL,
					      &start_at);

	gtk_source_buffer_forward_search_async (buffer,
						&start_at,
						NULL,
						(GAsyncReadyCallback)forward_search_finished,
						frame);
}

static void
backward_search_finished (GtkSourceBuffer *buffer,
			  GAsyncResult    *result,
			  GeditViewFrame  *frame)
{
	GtkTextIter match_start;
	GtkTextIter match_end;
	gboolean found;

	found = gtk_source_buffer_backward_search_finish (buffer,
							  result,
							  &match_start,
							  &match_end,
							  NULL);

	if (found)
	{
		gtk_text_buffer_select_range (GTK_TEXT_BUFFER (buffer),
					      &match_start,
					      &match_end);
	}

	finish_search (frame, found);
}

static void
backward_search (GeditViewFrame *frame)
{
	GtkTextIter start_at;
	GtkSourceBuffer *buffer;

	g_return_if_fail (frame->priv->search_mode == SEARCH);

	buffer = GTK_SOURCE_BUFFER (gedit_view_frame_get_document (frame));

	gtk_text_buffer_get_selection_bounds (GTK_TEXT_BUFFER (buffer),
					      &start_at,
					      NULL);

	gtk_source_buffer_backward_search_async (buffer,
						 &start_at,
						 NULL,
						 (GAsyncReadyCallback)backward_search_finished,
						 frame);
}

static void
search_again (GeditViewFrame *frame,
              gboolean        search_backward)
{
	g_return_if_fail (frame->priv->search_mode == SEARCH);

	renew_flush_timeout (frame);

	if (search_backward)
	{
		backward_search (frame);
	}
	else
	{
		forward_search (frame);
	}
}

static gboolean
search_widget_scroll_event (GtkWidget      *widget,
                            GdkEventScroll *event,
                            GeditViewFrame *frame)
{
	if (frame->priv->search_mode == GOTO_LINE)
	{
		return GDK_EVENT_PROPAGATE;
	}

	if (event->state & GDK_CONTROL_MASK)
	{
		if (event->direction == GDK_SCROLL_UP)
		{
			search_again (frame, TRUE);
			return GDK_EVENT_STOP;
		}
		else if (event->direction == GDK_SCROLL_DOWN)
		{
			search_again (frame, FALSE);
			return GDK_EVENT_STOP;
		}
	}

	return GDK_EVENT_PROPAGATE;
}

static gboolean
search_widget_key_press_event (GtkWidget      *widget,
                               GdkEventKey    *event,
                               GeditViewFrame *frame)
{
	guint modifiers = gtk_accelerator_get_default_mod_mask ();

	/* Close window */
	if (event->keyval == GDK_KEY_Tab)
	{
		hide_search_widget (frame, FALSE);
		return GDK_EVENT_STOP;
	}

	/* Close window and cancel the search */
	if (event->keyval == GDK_KEY_Escape)
	{
		if (frame->priv->search_mode == SEARCH)
		{
			GtkSourceBuffer *buffer;
			gchar *unescaped_search_text;

			/* restore document search so that Find Next does the right thing */
			buffer = GTK_SOURCE_BUFFER (gedit_view_frame_get_document (frame));
			gtk_source_buffer_set_case_sensitive_search (buffer, frame->priv->old_case_sensitive_search);
			gtk_source_buffer_set_search_at_word_boundaries (buffer, frame->priv->old_search_at_word_boundaries);
			gtk_source_buffer_set_search_wrap_around (buffer, frame->priv->old_search_wrap_around);

			unescaped_search_text = gtk_source_utils_unescape_search_text (frame->priv->old_search_text);
			gtk_source_buffer_set_search_text (buffer, unescaped_search_text);
			g_free (unescaped_search_text);
		}

		hide_search_widget (frame, TRUE);
		return GDK_EVENT_STOP;
	}

	if (frame->priv->search_mode == GOTO_LINE)
	{
		return GDK_EVENT_PROPAGATE;
	}

	/* SEARCH mode */

	/* select previous matching iter */
	if (event->keyval == GDK_KEY_Up || event->keyval == GDK_KEY_KP_Up)
	{
		search_again (frame, TRUE);
		return GDK_EVENT_STOP;
	}

	if (((event->state & modifiers) == (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) &&
	    (event->keyval == GDK_KEY_g || event->keyval == GDK_KEY_G))
	{
		search_again (frame, TRUE);
		return GDK_EVENT_STOP;
	}

	/* select next matching iter */
	if (event->keyval == GDK_KEY_Down || event->keyval == GDK_KEY_KP_Down)
	{
		search_again (frame, FALSE);
		return GDK_EVENT_STOP;
	}

	if (((event->state & modifiers) == GDK_CONTROL_MASK) &&
	    (event->keyval == GDK_KEY_g || event->keyval == GDK_KEY_G))
	{
		search_again (frame, FALSE);
		return GDK_EVENT_STOP;
	}

	return GDK_EVENT_PROPAGATE;
}

static void
update_entry_tag (GeditViewFrame *frame)
{
	GtkTextBuffer *buffer;
	GtkTextIter select_start;
	GtkTextIter select_end;
	gint count;
	gint pos;
	gchar *label;

	if (frame->priv->search_mode == GOTO_LINE)
	{
		gd_tagged_entry_remove_tag (frame->priv->search_entry,
					    frame->priv->entry_tag);
		return;
	}

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (frame->priv->view));

	count = gtk_source_buffer_get_search_occurrences_count (GTK_SOURCE_BUFFER (buffer));

	gtk_text_buffer_get_selection_bounds (buffer, &select_start, &select_end);

	pos = gtk_source_buffer_get_search_occurrence_position (GTK_SOURCE_BUFFER (buffer),
								&select_start,
								&select_end);

	if (count == -1 || pos == -1)
	{
		/* Wait that the buffer is fully scanned. */
		return;
	}

	if (count == 0)
	{
		gd_tagged_entry_remove_tag (frame->priv->search_entry,
					    frame->priv->entry_tag);
		return;
	}

	if (pos == 0)
	{
		label = g_strdup_printf ("%d", count);
	}
	else
	{
		/* Translators: the first %d is the position of the current search
		 * occurrence, and the second %d is the total number of search
		 * occurrences.
		 */
		label = g_strdup_printf (_("%d of %d"), pos, count);
	}

	gd_tagged_entry_tag_set_label (frame->priv->entry_tag, label);

	gd_tagged_entry_add_tag (frame->priv->search_entry,
				 frame->priv->entry_tag);

	g_free (label);
}

static gboolean
update_entry_tag_idle_cb (GeditViewFrame *frame)
{
	frame->priv->idle_update_entry_tag_id = 0;

	update_entry_tag (frame);

	return G_SOURCE_REMOVE;
}

static void
install_update_entry_tag_idle (GeditViewFrame *frame)
{
	if (frame->priv->idle_update_entry_tag_id == 0)
	{
		frame->priv->idle_update_entry_tag_id = g_idle_add ((GSourceFunc)update_entry_tag_idle_cb,
								    frame);
	}
}

static void
update_search (GeditViewFrame *frame)
{
	GtkSourceBuffer *buffer;
	const gchar *entry_text;
	gchar *unescaped_entry_text;

	buffer = GTK_SOURCE_BUFFER (gedit_view_frame_get_document (frame));

	entry_text = gtk_entry_get_text (GTK_ENTRY (frame->priv->search_entry));
	unescaped_entry_text = gtk_source_utils_unescape_search_text (entry_text);

	gtk_source_buffer_set_search_text (buffer, unescaped_entry_text);
	gtk_source_buffer_set_case_sensitive_search (buffer, frame->priv->case_sensitive_search);
	gtk_source_buffer_set_search_at_word_boundaries (buffer, frame->priv->search_at_word_boundaries);
	gtk_source_buffer_set_search_wrap_around (buffer, frame->priv->search_wrap_around);

	g_free (unescaped_entry_text);
}

static void
wrap_around_toggled_cb (GtkCheckMenuItem *menu_item,
			GeditViewFrame   *frame)
{
	GtkSourceBuffer *buffer = GTK_SOURCE_BUFFER (gedit_view_frame_get_document (frame));

	frame->priv->search_wrap_around = gtk_check_menu_item_get_active (menu_item);

	gtk_source_buffer_set_search_wrap_around (buffer, frame->priv->search_wrap_around);
}

static void
entire_word_toggled_cb (GtkCheckMenuItem *menu_item,
			GeditViewFrame   *frame)
{
	GtkSourceBuffer *buffer = GTK_SOURCE_BUFFER (gedit_view_frame_get_document (frame));

	frame->priv->search_at_word_boundaries = gtk_check_menu_item_get_active (menu_item);

	gtk_source_buffer_set_search_at_word_boundaries (buffer, frame->priv->search_at_word_boundaries);
}

static void
match_case_toggled_cb (GtkCheckMenuItem *menu_item,
		       GeditViewFrame   *frame)
{
	GtkSourceBuffer *buffer = GTK_SOURCE_BUFFER (gedit_view_frame_get_document (frame));

	frame->priv->case_sensitive_search = gtk_check_menu_item_get_active (menu_item);

	gtk_source_buffer_set_case_sensitive_search (buffer, frame->priv->case_sensitive_search);
}

static void
add_popup_menu_items (GtkWidget      *menu,
                      GeditViewFrame *frame)
{
	GtkWidget *menu_item;

	/* create "Wrap Around" menu item. */
	menu_item = gtk_check_menu_item_new_with_mnemonic (_("_Wrap Around"));

	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), menu_item);

	g_signal_connect (menu_item,
			  "toggled",
			  G_CALLBACK (wrap_around_toggled_cb),
			  frame);

	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menu_item),
					frame->priv->search_wrap_around);

	gtk_widget_show (menu_item);

	/* create "Match Entire Word Only" menu item. */
	menu_item = gtk_check_menu_item_new_with_mnemonic (_("Match _Entire Word Only"));

	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), menu_item);

	g_signal_connect (menu_item,
			  "toggled",
			  G_CALLBACK (entire_word_toggled_cb),
			  frame);

	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menu_item),
					frame->priv->search_at_word_boundaries);

	gtk_widget_show (menu_item);

	/* create "Match Case" menu item. */
	menu_item = gtk_check_menu_item_new_with_mnemonic (_("_Match Case"));

	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), menu_item);

	g_signal_connect (menu_item,
			  "toggled",
			  G_CALLBACK (match_case_toggled_cb),
			  frame);

	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menu_item),
					frame->priv->case_sensitive_search);

	gtk_widget_show (menu_item);
}

static gboolean
real_search_enable_popdown (GeditViewFrame *frame)
{
	frame->priv->disable_popdown = FALSE;
	return FALSE;
}

static void
search_enable_popdown (GtkWidget      *widget,
                       GeditViewFrame *frame)
{
	g_timeout_add (200,
		       (GSourceFunc)real_search_enable_popdown,
		       frame);

	renew_flush_timeout (frame);
}

static void
search_entry_populate_popup (GtkEntry       *entry,
                             GtkMenu        *menu,
                             GeditViewFrame *frame)
{
	GtkWidget *menu_item;

	frame->priv->disable_popdown = TRUE;

	g_signal_connect (menu,
			  "hide",
			  G_CALLBACK (search_enable_popdown),
			  frame);

	if (frame->priv->search_mode == GOTO_LINE)
	{
		return;
	}

	/* separator */
	menu_item = gtk_separator_menu_item_new ();
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), menu_item);
	gtk_widget_show (menu_item);

	add_popup_menu_items (GTK_WIDGET (menu), frame);
}

static void
search_entry_icon_release (GtkEntry             *entry,
                           GtkEntryIconPosition  icon_pos,
                           GdkEventButton       *event,
                           GeditViewFrame       *frame)
{
	GtkWidget *menu;

	if (frame->priv->search_mode == GOTO_LINE ||
	    icon_pos != GTK_ENTRY_ICON_PRIMARY)
	{
		return;
	}

	menu = gtk_menu_new ();
	gtk_widget_show (menu);

	frame->priv->disable_popdown = TRUE;

	g_signal_connect (menu,
			  "hide",
	                  G_CALLBACK (search_enable_popdown),
			  frame);

	add_popup_menu_items (menu, frame);

	gtk_menu_popup (GTK_MENU (menu),
	                NULL, NULL,
	                NULL, NULL,
	                event->button, event->time);
}

static void
search_entry_activate (GtkEntry       *entry,
                       GeditViewFrame *frame)
{
	hide_search_widget (frame, FALSE);
}

static void
search_entry_insert_text (GtkEditable    *editable,
                          const gchar    *text,
                          gint            length,
                          gint           *position,
                          GeditViewFrame *frame)
{
	gunichar c;
	const gchar *p;
	const gchar *end;
	const gchar *next;

	if (frame->priv->search_mode == SEARCH)
	{
		return;
	}

	p = text;
	end = text + length;

	if (p == end)
	{
		return;
	}

	c = g_utf8_get_char (p);

	if (((c == '-' || c == '+') && *position == 0) ||
	    (c == ':' && *position != 0))
	{
		gchar *s = NULL;

		if (c == ':')
		{
			s = gtk_editable_get_chars (editable, 0, -1);
			s = g_utf8_strchr (s, -1, ':');
		}

		if (s == NULL || s == p)
		{
			next = g_utf8_next_char (p);
			p = next;
		}

		g_free (s);
	}

	while (p != end)
	{
		next = g_utf8_next_char (p);

		c = g_utf8_get_char (p);

		if (!g_unichar_isdigit (c))
		{
			g_signal_stop_emission_by_name (editable, "insert_text");
			gtk_widget_error_bell (GTK_WIDGET (frame->priv->search_entry));
			break;
		}

		p = next;
	}
}

static void
customize_for_search_mode (GeditViewFrame *frame)
{
	GIcon *icon;

	if (frame->priv->search_mode == SEARCH)
	{
		icon = g_themed_icon_new_with_default_fallbacks ("edit-find-symbolic");

		gtk_widget_set_tooltip_text (GTK_WIDGET (frame->priv->search_entry),
		                             _("String you want to search for"));

		gtk_widget_show (frame->priv->go_up_button);
		gtk_widget_show (frame->priv->go_down_button);
	}
	else
	{
		icon = g_themed_icon_new_with_default_fallbacks ("go-jump-symbolic");

		gtk_widget_set_tooltip_text (GTK_WIDGET (frame->priv->search_entry),
		                             _("Line you want to move the cursor to"));

		gtk_widget_hide (frame->priv->go_up_button);
		gtk_widget_hide (frame->priv->go_down_button);
	}

	gtk_entry_set_icon_from_gicon (GTK_ENTRY (frame->priv->search_entry),
	                               GTK_ENTRY_ICON_PRIMARY,
	                               icon);

	g_object_unref (icon);
}

static void
search_init (GtkWidget      *entry,
             GeditViewFrame *frame)
{
	const gchar *entry_text = gtk_entry_get_text (GTK_ENTRY (entry));

	renew_flush_timeout (frame);

	if (frame->priv->search_mode == SEARCH)
	{
		update_search (frame);
		start_search (frame);
	}
	else if (*entry_text != '\0')
	{
		gboolean moved, moved_offset;
		gint line;
		gint offset_line = 0;
		gint line_offset = 0;
		gchar **split_text = NULL;
		const gchar *text;
		GtkTextIter iter;
		GeditDocument *doc;

		doc = gedit_view_frame_get_document (frame);

		gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (doc),
						  &iter,
						  frame->priv->start_mark);

		split_text = g_strsplit (entry_text, ":", -1);

		if (g_strv_length (split_text) > 1)
		{
			text = split_text[0];
		}
		else
		{
			text = entry_text;
		}

		if (*text == '-')
		{
			gint cur_line = gtk_text_iter_get_line (&iter);

			if (*(text + 1) != '\0')
				offset_line = MAX (atoi (text + 1), 0);

			line = MAX (cur_line - offset_line, 0);
		}
		else if (*entry_text == '+')
		{
			gint cur_line = gtk_text_iter_get_line (&iter);

			if (*(text + 1) != '\0')
				offset_line = MAX (atoi (text + 1), 0);

			line = cur_line + offset_line;
		}
		else
		{
			line = MAX (atoi (text) - 1, 0);
		}

		if (split_text[1] != NULL)
		{
			line_offset = atoi (split_text[1]);
		}

		g_strfreev (split_text);

		moved = gedit_document_goto_line (doc, line);
		moved_offset = gedit_document_goto_line_offset (doc, line,
								line_offset);

		gedit_view_scroll_to_cursor (frame->priv->view);

		if (!moved || !moved_offset)
		{
			set_search_state (frame, SEARCH_STATE_NOT_FOUND);
		}
		else
		{
			set_search_state (frame, SEARCH_STATE_NORMAL);
		}
	}
}

static gboolean
search_entry_focus_out_event (GtkWidget      *widget,
                              GdkEventFocus  *event,
                              GeditViewFrame *frame)
{
	hide_search_widget (frame, FALSE);
	return GDK_EVENT_PROPAGATE;
}

static void
on_go_up_button_clicked (GtkWidget      *button,
                         GeditViewFrame *frame)
{
	search_again (frame, TRUE);
}

static void
on_go_down_button_clicked (GtkWidget      *button,
                           GeditViewFrame *frame)
{
	search_again (frame, FALSE);
}

static void
mark_set_cb (GtkTextBuffer  *buffer,
	     GtkTextIter    *location,
	     GtkTextMark    *mark,
	     GeditViewFrame *frame)
{
	GtkTextMark *insert;
	GtkTextMark *selection_bound;

	insert = gtk_text_buffer_get_insert (buffer);
	selection_bound = gtk_text_buffer_get_selection_bound (buffer);

	if (mark == insert || mark == selection_bound)
	{
		install_update_entry_tag_idle (frame);
	}
}

static gboolean
get_selected_text (GtkTextBuffer  *doc,
		   gchar         **selected_text,
		   gint           *len)
{
	GtkTextIter start;
	GtkTextIter end;

	g_return_val_if_fail (selected_text != NULL, FALSE);
	g_return_val_if_fail (*selected_text == NULL, FALSE);

	if (!gtk_text_buffer_get_selection_bounds (doc, &start, &end))
	{
		if (len != NULL)
		{
			*len = 0;
		}

		return FALSE;
	}

	*selected_text = gtk_text_buffer_get_slice (doc, &start, &end, TRUE);

	if (len != NULL)
	{
		*len = g_utf8_strlen (*selected_text, -1);
	}

	return TRUE;
}

static void
init_search_entry (GeditViewFrame *frame)
{
	GtkTextBuffer *buffer;

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (frame->priv->view));

	customize_for_search_mode (frame);

	if (frame->priv->search_mode == GOTO_LINE)
	{
		gint   line;
		gchar *line_str;
		GtkTextIter iter;

		gtk_text_buffer_get_iter_at_mark (buffer,
		                                  &iter,
		                                  frame->priv->start_mark);

		line = gtk_text_iter_get_line (&iter);

		line_str = g_strdup_printf ("%d", line + 1);

		gtk_entry_set_text (GTK_ENTRY (frame->priv->search_entry), line_str);

		gtk_editable_select_region (GTK_EDITABLE (frame->priv->search_entry),
		                            0, -1);

		g_free (line_str);
	}
	else
	{
		/* SEARCH mode */
		gboolean selection_exists;
		gchar *search_text = NULL;
		const gchar *old_search_text;
		gint selection_len = 0;

		old_search_text = gtk_source_buffer_get_search_text (GTK_SOURCE_BUFFER (buffer));

		frame->priv->old_case_sensitive_search =
			gtk_source_buffer_get_case_sensitive_search (GTK_SOURCE_BUFFER (buffer));

		frame->priv->old_search_at_word_boundaries =
			gtk_source_buffer_get_search_at_word_boundaries (GTK_SOURCE_BUFFER (buffer));

		frame->priv->old_search_wrap_around =
			gtk_source_buffer_get_search_wrap_around (GTK_SOURCE_BUFFER (buffer));

		selection_exists = get_selected_text (buffer,
		                                      &search_text,
		                                      &selection_len);

		if (selection_exists && (search_text != NULL) && (selection_len <= 160))
		{
			gchar *search_text_escaped = gtk_source_utils_escape_search_text (search_text);

			gtk_entry_set_text (GTK_ENTRY (frame->priv->search_entry),
					    search_text_escaped);

			gtk_editable_set_position (GTK_EDITABLE (frame->priv->search_entry),
			                           -1);

			g_free (search_text_escaped);
		}
		else if (old_search_text != NULL)
		{
			gchar *old_search_text_escaped = gtk_source_utils_escape_search_text (old_search_text);

			g_free (frame->priv->old_search_text);
			frame->priv->old_search_text = old_search_text_escaped;
			g_signal_handler_block (frame->priv->search_entry,
			                        frame->priv->search_entry_changed_id);

			gtk_entry_set_text (GTK_ENTRY (frame->priv->search_entry),
					    old_search_text_escaped);

			gtk_editable_select_region (GTK_EDITABLE (frame->priv->search_entry),
			                            0, -1);

			g_signal_handler_unblock (frame->priv->search_entry,
			                          frame->priv->search_entry_changed_id);
		}

		g_free (search_text);
	}
}

static void
start_interactive_search_real (GeditViewFrame *frame,
			       SearchMode      request_search_mode)
{
	GtkTextBuffer *buffer;
	GtkTextIter iter;
	GtkTextMark *mark;

	if (gtk_revealer_get_reveal_child (frame->priv->revealer))
	{
		if (frame->priv->search_mode != request_search_mode)
		{
			hide_search_widget (frame, TRUE);
		}
		else
		{
			gtk_editable_select_region (GTK_EDITABLE (frame->priv->search_entry),
			                            0, -1);
			return;
		}
	}

	frame->priv->search_mode = request_search_mode;

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (frame->priv->view));

	if (frame->priv->search_mode == SEARCH)
	{
		gtk_text_buffer_get_selection_bounds (buffer, &iter, NULL);
	}
	else
	{
		mark = gtk_text_buffer_get_insert (buffer);
		gtk_text_buffer_get_iter_at_mark (buffer, &iter, mark);
	}

	frame->priv->start_mark = gtk_text_buffer_create_mark (buffer, NULL,
	                                                       &iter, FALSE);

	gtk_revealer_set_reveal_child (frame->priv->revealer, TRUE);

	/* NOTE: we must be very careful here to not have any text before
	   focusing the entry because when the entry is focused the text is
	   selected, and gtk+ doesn't allow us to have more than one selection
	   active */
	g_signal_handler_block (frame->priv->search_entry,
	                        frame->priv->search_entry_changed_id);

	gtk_entry_set_text (GTK_ENTRY (frame->priv->search_entry), "");

	g_signal_handler_unblock (frame->priv->search_entry,
	                          frame->priv->search_entry_changed_id);

	/* We need to grab the focus after the widget has been added */
	gtk_widget_grab_focus (GTK_WIDGET (frame->priv->search_entry));

	init_search_entry (frame);

	/* Manage the scroll also for the view */
	frame->priv->view_scroll_event_id =
		g_signal_connect (frame->priv->view, "scroll-event",
			          G_CALLBACK (search_widget_scroll_event),
			          frame);

	renew_flush_timeout (frame);

	install_update_entry_tag_idle (frame);
}

static void
gedit_view_frame_class_init (GeditViewFrameClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->finalize = gedit_view_frame_finalize;
	object_class->dispose = gedit_view_frame_dispose;
	object_class->get_property = gedit_view_frame_get_property;

	g_object_class_install_property (object_class, PROP_DOCUMENT,
	                                 g_param_spec_object ("document",
	                                                      "Document",
	                                                      "The Document",
	                                                      GEDIT_TYPE_DOCUMENT,
	                                                      G_PARAM_READABLE |
	                                                      G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (object_class, PROP_VIEW,
	                                 g_param_spec_object ("view",
	                                                      "View",
	                                                      "The View",
	                                                      GEDIT_TYPE_VIEW,
	                                                      G_PARAM_READABLE |
	                                                      G_PARAM_STATIC_STRINGS));

	/* Bind class to template */
	gtk_widget_class_set_template_from_resource (widget_class,
	                                             "/org/gnome/gedit/ui/gedit-view-frame.ui");
	gtk_widget_class_bind_template_child_private (widget_class, GeditViewFrame, view);
	gtk_widget_class_bind_template_child_private (widget_class, GeditViewFrame, revealer);
	gtk_widget_class_bind_template_child_private (widget_class, GeditViewFrame, search_entry);
	gtk_widget_class_bind_template_child_private (widget_class, GeditViewFrame, go_up_button);
	gtk_widget_class_bind_template_child_private (widget_class, GeditViewFrame, go_down_button);
}

static GMountOperation *
view_frame_mount_operation_factory (GeditDocument *doc,
				    gpointer       user_data)
{
	GtkWidget *frame = user_data;
	GtkWidget *window = gtk_widget_get_toplevel (frame);

	return gtk_mount_operation_new (GTK_WINDOW (window));
}

static void
gedit_view_frame_init (GeditViewFrame *frame)
{
	GeditDocument *doc;
	GdkRGBA transparent = {0, 0, 0, 0};

	frame->priv = gedit_view_frame_get_instance_private (frame);

	frame->priv->case_sensitive_search = FALSE;
	frame->priv->search_at_word_boundaries = FALSE;
	frame->priv->search_wrap_around = TRUE;

	gtk_widget_init_template (GTK_WIDGET (frame));

	gtk_widget_override_background_color (GTK_WIDGET (frame), 0, &transparent);

	doc = gedit_view_frame_get_document (frame);
	_gedit_document_set_mount_operation_factory (doc,
						     view_frame_mount_operation_factory,
						     frame);

	frame->priv->entry_tag = gd_tagged_entry_tag_new ("");

	gd_tagged_entry_tag_set_style (frame->priv->entry_tag,
				       "gedit-search-entry-occurrences-tag");

	gd_tagged_entry_tag_set_has_close_button (frame->priv->entry_tag, FALSE);

	if (gtk_widget_get_direction (GTK_WIDGET (frame->priv->revealer)) == GTK_TEXT_DIR_LTR)
	{
		gtk_widget_set_margin_right (GTK_WIDGET (frame->priv->revealer),
					     SEARCH_POPUP_MARGIN);
	}
	else
	{
		gtk_widget_set_margin_left (GTK_WIDGET (frame->priv->revealer),
					    SEARCH_POPUP_MARGIN);
	}

	g_signal_connect (doc,
			  "mark-set",
			  G_CALLBACK (mark_set_cb),
			  frame);

	g_signal_connect_swapped (doc,
				  "notify::search-occurrences-count",
				  G_CALLBACK (install_update_entry_tag_idle),
				  frame);

	g_signal_connect (frame->priv->revealer,
			  "key-press-event",
	                  G_CALLBACK (search_widget_key_press_event),
	                  frame);

	g_signal_connect (frame->priv->revealer,
			  "scroll-event",
	                  G_CALLBACK (search_widget_scroll_event),
	                  frame);

	g_signal_connect (frame->priv->search_entry,
			  "populate-popup",
	                  G_CALLBACK (search_entry_populate_popup),
	                  frame);

	g_signal_connect (frame->priv->search_entry,
			  "icon-release",
	                  G_CALLBACK (search_entry_icon_release),
	                  frame);

	g_signal_connect (frame->priv->search_entry,
			  "activate",
	                  G_CALLBACK (search_entry_activate),
	                  frame);

	g_signal_connect (frame->priv->search_entry,
			  "insert-text",
	                  G_CALLBACK (search_entry_insert_text),
	                  frame);

	frame->priv->search_entry_changed_id =
		g_signal_connect (frame->priv->search_entry,
				  "changed",
		                  G_CALLBACK (search_init),
		                  frame);

	frame->priv->search_entry_focus_out_id =
		g_signal_connect (frame->priv->search_entry,
				  "focus-out-event",
		                  G_CALLBACK (search_entry_focus_out_event),
		                  frame);

	g_signal_connect (frame->priv->go_up_button,
	                  "clicked",
	                  G_CALLBACK (on_go_up_button_clicked),
	                  frame);

	g_signal_connect (frame->priv->go_down_button,
	                  "clicked",
	                  G_CALLBACK (on_go_down_button_clicked),
	                  frame);
}

GeditViewFrame *
gedit_view_frame_new ()
{
	return g_object_new (GEDIT_TYPE_VIEW_FRAME, NULL);
}

GeditDocument *
gedit_view_frame_get_document (GeditViewFrame *frame)
{
	g_return_val_if_fail (GEDIT_IS_VIEW_FRAME (frame), NULL);

	return GEDIT_DOCUMENT (gtk_text_view_get_buffer (GTK_TEXT_VIEW (frame->priv->view)));
}

GeditView *
gedit_view_frame_get_view (GeditViewFrame *frame)
{
	g_return_val_if_fail (GEDIT_IS_VIEW_FRAME (frame), NULL);

	return frame->priv->view;
}

void
gedit_view_frame_popup_search (GeditViewFrame *frame)
{
	g_return_if_fail (GEDIT_IS_VIEW_FRAME (frame));

	start_interactive_search_real (frame, SEARCH);
}

void
gedit_view_frame_popup_goto_line (GeditViewFrame *frame)
{
	g_return_if_fail (GEDIT_IS_VIEW_FRAME (frame));

	start_interactive_search_real (frame, GOTO_LINE);
}

void
gedit_view_frame_clear_search (GeditViewFrame *frame)
{
	GeditDocument *doc;

	g_return_if_fail (GEDIT_IS_VIEW_FRAME (frame));

	doc = gedit_view_frame_get_document (frame);

	gtk_source_buffer_set_search_text (GTK_SOURCE_BUFFER (doc), "");

	g_signal_handler_block (frame->priv->search_entry,
	                        frame->priv->search_entry_changed_id);

	gtk_entry_set_text (GTK_ENTRY (frame->priv->search_entry), "");

	g_signal_handler_unblock (frame->priv->search_entry,
	                          frame->priv->search_entry_changed_id);

	gtk_widget_grab_focus (GTK_WIDGET (frame->priv->view));
}
