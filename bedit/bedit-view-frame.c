/*
 * bedit-view-frame.c
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * Based on gedit-view-frame.c from Gedit.
 *
 * Copyright (C) 2010-2015 - Ignacio Casal Quinteiro
 * Copyright (C) 2011 - Dan Williams, Garrett Regier, Jesse van den Kieboom,
 *   Kjartan Maraas
 * Copyright (C) 2011-2015 - Paolo Borelli
 * Copyright (C) 2013 - Matthias Clasen
 * Copyright (C) 2013-2019 - Sébastien Wilmet
 * Copyright (C) 2014 - Oliver Gerlich, Robert Roth, Stefano Facchini, ghostroot
 * Copyright (C) 2014-2018 - Sebastien Lafargue
 * Copyright (C) 2018 - Christian Hergert
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

#include "bedit-view-frame.h"

#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>
#include <gtksourceview/gtksource.h>
#include <stdlib.h>

#include "bedit-debug.h"
#include "bedit-settings.h"
#include "bedit-utils.h"
#include "libgd/gd.h"

#define FLUSH_TIMEOUT_DURATION 30 /* in seconds */

#define SEARCH_POPUP_MARGIN 12

typedef enum { SEARCH_STATE_NORMAL, SEARCH_STATE_NOT_FOUND } SearchState;

struct _BeditViewFrame {
    GtkOverlay parent_instance;

    BeditView *view;

    /* Where the search has started. When the user presses escape in the
     * search entry (to cancel the search), we return to the start_mark.
     */
    GtkTextMark *start_mark;

    GtkRevealer *revealer;
    GtkSearchEntry *goto_line_entry;

    guint flush_timeout_id;
    gulong goto_line_entry_focus_out_id;
    gulong goto_line_entry_changed_id;
};

G_DEFINE_TYPE(BeditViewFrame, bedit_view_frame, GTK_TYPE_OVERLAY)

static BeditDocument *get_document(BeditViewFrame *frame) {
    return BEDIT_DOCUMENT(gtk_text_view_get_buffer(GTK_TEXT_VIEW(frame->view)));
}

static void get_iter_at_start_mark(BeditViewFrame *frame, GtkTextIter *iter) {
    GtkTextBuffer *buffer;

    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(frame->view));

    if (frame->start_mark != NULL) {
        gtk_text_buffer_get_iter_at_mark(buffer, iter, frame->start_mark);
    } else {
        g_warn_if_reached();
        gtk_text_buffer_get_start_iter(buffer, iter);
    }
}

static void bedit_view_frame_dispose(GObject *object) {
    BeditViewFrame *frame = BEDIT_VIEW_FRAME(object);
    GtkTextBuffer *buffer = NULL;

    if (frame->view != NULL) {
        buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(frame->view));
    }

    if (frame->start_mark != NULL && buffer != NULL) {
        gtk_text_buffer_delete_mark(buffer, frame->start_mark);
        frame->start_mark = NULL;
    }

    if (frame->flush_timeout_id != 0) {
        g_source_remove(frame->flush_timeout_id);
        frame->flush_timeout_id = 0;
    }

    if (buffer != NULL) {
        GtkSourceFile *file = bedit_document_get_file(BEDIT_DOCUMENT(buffer));
        gtk_source_file_set_mount_operation_factory(file, NULL, NULL, NULL);
    }

    G_OBJECT_CLASS(bedit_view_frame_parent_class)->dispose(object);
}

static void hide_goto_line_widget(BeditViewFrame *frame, gboolean cancel) {
    GtkTextBuffer *buffer;

    if (!gtk_revealer_get_reveal_child(frame->revealer)) {
        return;
    }

    if (frame->flush_timeout_id != 0) {
        g_source_remove(frame->flush_timeout_id);
        frame->flush_timeout_id = 0;
    }

    gtk_revealer_set_reveal_child(frame->revealer, FALSE);

    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(frame->view));

    if (cancel && frame->start_mark != NULL) {
        GtkTextIter iter;

        gtk_text_buffer_get_iter_at_mark(buffer, &iter, frame->start_mark);
        gtk_text_buffer_place_cursor(buffer, &iter);

        bedit_view_scroll_to_cursor(frame->view);
    }

    if (frame->start_mark != NULL) {
        gtk_text_buffer_delete_mark(buffer, frame->start_mark);
        frame->start_mark = NULL;
    }
}

static gboolean goto_line_entry_flush_timeout(BeditViewFrame *frame) {
    frame->flush_timeout_id = 0;
    hide_goto_line_widget(frame, FALSE);

    return G_SOURCE_REMOVE;
}

static void renew_flush_timeout(BeditViewFrame *frame) {
    if (frame->flush_timeout_id != 0) {
        g_source_remove(frame->flush_timeout_id);
    }

    frame->flush_timeout_id = g_timeout_add_seconds(
        FLUSH_TIMEOUT_DURATION,
        (GSourceFunc)goto_line_entry_flush_timeout,
        frame);
}

static void set_goto_line_state(BeditViewFrame *frame, SearchState state) {
    GtkStyleContext *context;

    context = gtk_widget_get_style_context(GTK_WIDGET(frame->goto_line_entry));

    if (state == SEARCH_STATE_NOT_FOUND) {
        gtk_style_context_add_class(context, GTK_STYLE_CLASS_ERROR);
    } else {
        gtk_style_context_remove_class(context, GTK_STYLE_CLASS_ERROR);
    }
}

static gboolean goto_line_widget_key_press_event(
    GtkWidget *widget, GdkEventKey *event, BeditViewFrame *frame) {
    /* Close window */
    if (event->keyval == GDK_KEY_Tab) {
        hide_goto_line_widget(frame, FALSE);
        gtk_widget_grab_focus(GTK_WIDGET(frame->view));

        return GDK_EVENT_STOP;
    }

    return GDK_EVENT_PROPAGATE;
}

static void goto_line_entry_escaped(
    GtkSearchEntry *entry, BeditViewFrame *frame) {
    hide_goto_line_widget(frame, TRUE);
    gtk_widget_grab_focus(GTK_WIDGET(frame->view));
}

static void goto_line_entry_activate(GtkEntry *entry, BeditViewFrame *frame) {
    hide_goto_line_widget(frame, FALSE);
    gtk_widget_grab_focus(GTK_WIDGET(frame->view));
}

static void goto_line_entry_insert_text(
    GtkEditable *editable, const gchar *text, gint length, gint *position,
    BeditViewFrame *frame) {
    gunichar c;
    const gchar *p;
    const gchar *end;
    const gchar *next;

    p = text;
    end = text + length;

    if (p == end) {
        return;
    }

    c = g_utf8_get_char(p);

    if (((c == '-' || c == '+') && *position == 0) ||
        (c == ':' && *position != 0)) {
        gchar *s = NULL;

        if (c == ':') {
            s = gtk_editable_get_chars(editable, 0, -1);
            s = g_utf8_strchr(s, -1, ':');
        }

        if (s == NULL || s == p) {
            next = g_utf8_next_char(p);
            p = next;
        }

        g_free(s);
    }

    while (p != end) {
        next = g_utf8_next_char(p);

        c = g_utf8_get_char(p);

        if (!g_unichar_isdigit(c)) {
            g_signal_stop_emission_by_name(editable, "insert_text");
            gtk_widget_error_bell(GTK_WIDGET(frame->goto_line_entry));
            break;
        }

        p = next;
    }
}

static void update_goto_line(BeditViewFrame *frame) {
    const gchar *entry_text;
    gboolean moved;
    gboolean moved_offset;
    gint line;
    gint offset_line = 0;
    gint line_offset = 0;
    gchar **split_text = NULL;
    const gchar *text;
    GtkTextIter iter;
    BeditDocument *doc;

    entry_text = gtk_entry_get_text(GTK_ENTRY(frame->goto_line_entry));

    if (entry_text[0] == '\0') {
        return;
    }

    get_iter_at_start_mark(frame, &iter);

    split_text = g_strsplit(entry_text, ":", -1);

    if (g_strv_length(split_text) > 1) {
        text = split_text[0];
    } else {
        text = entry_text;
    }

    if (text[0] == '-') {
        gint cur_line = gtk_text_iter_get_line(&iter);

        if (text[1] != '\0') {
            offset_line = MAX(atoi(text + 1), 0);
        }

        line = MAX(cur_line - offset_line, 0);
    } else if (entry_text[0] == '+') {
        gint cur_line = gtk_text_iter_get_line(&iter);

        if (text[1] != '\0') {
            offset_line = MAX(atoi(text + 1), 0);
        }

        line = cur_line + offset_line;
    } else {
        line = MAX(atoi(text) - 1, 0);
    }

    if (split_text[1] != NULL) {
        line_offset = atoi(split_text[1]);
    }

    g_strfreev(split_text);

    doc = get_document(frame);
    moved = bedit_document_goto_line(doc, line);
    moved_offset = bedit_document_goto_line_offset(doc, line, line_offset);

    bedit_view_scroll_to_cursor(frame->view);

    if (!moved || !moved_offset) {
        set_goto_line_state(frame, SEARCH_STATE_NOT_FOUND);
    } else {
        set_goto_line_state(frame, SEARCH_STATE_NORMAL);
    }
}

static void goto_line_entry_changed_cb(GtkEntry *entry, BeditViewFrame *frame) {
    renew_flush_timeout(frame);

    update_goto_line(frame);
}

static gboolean goto_line_entry_focus_out_event(
    GtkWidget *widget, GdkEventFocus *event, BeditViewFrame *frame) {
    hide_goto_line_widget(frame, FALSE);
    return GDK_EVENT_PROPAGATE;
}

static void start_interactive_goto_line_real(BeditViewFrame *frame) {
    GtkTextBuffer *buffer;
    GtkTextIter iter;
    GtkTextMark *mark;
    gint line;
    gchar *line_str;

    if (gtk_revealer_get_reveal_child(frame->revealer)) {
        gtk_editable_select_region(
            GTK_EDITABLE(frame->goto_line_entry), 0, -1);
        return;
    }

    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(frame->view));

    mark = gtk_text_buffer_get_insert(buffer);
    gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);
    
    if (frame->start_mark != NULL) {
        gtk_text_buffer_delete_mark(buffer, frame->start_mark);
    }

    frame->start_mark = gtk_text_buffer_create_mark(buffer, NULL, &iter, FALSE);

    line = gtk_text_iter_get_line(&iter);
    line_str = g_strdup_printf("%d", line + 1);
    gtk_entry_set_text(GTK_ENTRY(frame->goto_line_entry), line_str);

    gtk_editable_select_region(GTK_EDITABLE(frame->goto_line_entry), 0, -1);

    gtk_revealer_set_reveal_child(frame->revealer, TRUE);
    gtk_widget_grab_focus(GTK_WIDGET(frame->goto_line_entry));

    renew_flush_timeout(frame);
    
    g_free(line_str);
}

static void bedit_view_frame_class_init(BeditViewFrameClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    object_class->dispose = bedit_view_frame_dispose;

    /* Bind class to template */
    gtk_widget_class_set_template_from_resource(
        widget_class, "/com/bwhmather/bedit/ui/bedit-view-frame.ui");
    gtk_widget_class_bind_template_child(widget_class, BeditViewFrame, view);
    gtk_widget_class_bind_template_child(
        widget_class, BeditViewFrame, revealer);
    gtk_widget_class_bind_template_child(
        widget_class, BeditViewFrame, goto_line_entry);
}

static GMountOperation *view_frame_mount_operation_factory(
    GtkSourceFile *file, gpointer user_data) {
    GtkWidget *view_frame = user_data;
    GtkWidget *window = gtk_widget_get_toplevel(view_frame);

    return gtk_mount_operation_new(GTK_WINDOW(window));
}

static void bedit_view_frame_init(BeditViewFrame *frame) {
    BeditDocument *doc;
    GtkSourceFile *file;

    bedit_debug(DEBUG_WINDOW);

    gtk_widget_init_template(GTK_WIDGET(frame));

    doc = get_document(frame);
    file = bedit_document_get_file(doc);

    gtk_source_file_set_mount_operation_factory(
        file, view_frame_mount_operation_factory, frame, NULL);


    gtk_widget_set_margin_end(GTK_WIDGET(frame->revealer), SEARCH_POPUP_MARGIN);

    g_signal_connect(
        frame->revealer, "key-press-event",
        G_CALLBACK(goto_line_widget_key_press_event),
        frame);

    g_signal_connect(
        frame->goto_line_entry, "activate",
        G_CALLBACK(goto_line_entry_activate),
        frame);

    g_signal_connect(
        frame->goto_line_entry, "insert-text",
        G_CALLBACK(goto_line_entry_insert_text),
        frame);

    g_signal_connect(
        frame->goto_line_entry, "stop-search",
        G_CALLBACK(goto_line_entry_escaped),
        frame);

    frame->goto_line_entry_changed_id = g_signal_connect(
        frame->goto_line_entry, "changed",
        G_CALLBACK(goto_line_entry_changed_cb),
        frame);

    frame->goto_line_entry_focus_out_id = g_signal_connect(
        frame->goto_line_entry, "focus-out-event",
        G_CALLBACK(goto_line_entry_focus_out_event),
        frame);
}

BeditViewFrame *bedit_view_frame_new(void) {
    return g_object_new(BEDIT_TYPE_VIEW_FRAME, NULL);
}

BeditView *bedit_view_frame_get_view(BeditViewFrame *frame) {
    g_return_val_if_fail(BEDIT_IS_VIEW_FRAME(frame), NULL);

    return frame->view;
}

void bedit_view_frame_popup_goto_line(BeditViewFrame *frame) {
    g_return_if_fail(BEDIT_IS_VIEW_FRAME(frame));

    start_interactive_goto_line_real(frame);
}

void bedit_view_frame_clear_search(BeditViewFrame *frame) {
    g_return_if_fail(BEDIT_IS_VIEW_FRAME(frame));

    g_signal_handler_block(
        frame->goto_line_entry, frame->goto_line_entry_changed_id);

    gtk_entry_set_text(GTK_ENTRY(frame->goto_line_entry), "");

    g_signal_handler_unblock(
        frame->goto_line_entry, frame->goto_line_entry_changed_id);

    gtk_widget_grab_focus(GTK_WIDGET(frame->view));
}

