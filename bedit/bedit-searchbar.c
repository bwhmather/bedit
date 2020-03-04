/*
 * bedit-searchbar.c
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
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

#include "bedit-debug.h"
#include "bedit-searchbar.h"
#include "bedit-view.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gtksourceview/gtksource.h>
#include <string.h>

typedef enum {
    BEDIT_SEARCHBAR_MODE_HIDDEN,
    BEDIT_SEARCHBAR_MODE_FIND,
    BEDIT_SEARCHBAR_MODE_REPLACE,
} _BeditSearchbarMode;

struct _BeditSearchbar {
    GtkGrid parent_instance;
    _BeditSearchbarMode mode;

    GtkWidget *revealer;

    GtkWidget *search_entry;
    GtkWidget *replace_entry;

    GtkWidget *prev_button;
    GtkWidget *next_button;

    GtkWidget *match_case_toggle;
    GtkWidget *regex_toggle;

    GtkWidget *replace_button;
    GtkWidget *replace_all_button;

    BeditView *view;
    GtkSourceSearchContext *context;
    GCancellable *cancellable;
    GtkTextMark *start_mark;

    gulong cursor_moved_cb_id;

    gboolean search_active : 1;
    gboolean replace_active : 1;
};

enum {
    PROP_0,
    PROP_SEARCH_ACTIVE,
    PROP_REPLACE_ACTIVE,
    LAST_PROP
};

static GParamSpec *properties[LAST_PROP];

G_DEFINE_TYPE(BeditSearchbar, bedit_searchbar, GTK_TYPE_BIN)

static void bedit_searchbar_get_property(
    GObject *object, guint prop_id, GValue *value, GParamSpec *pspec
) {
    BeditSearchbar *searchbar = BEDIT_SEARCHBAR(object);

    switch (prop_id) {
    case PROP_SEARCH_ACTIVE:
        g_value_set_boolean(
            value, bedit_searchbar_get_search_active(searchbar)
        );
        break;

    case PROP_REPLACE_ACTIVE:
        g_value_set_boolean(
            value, bedit_searchbar_get_search_active(searchbar)
        );
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void bedit_searchbar_class_init(BeditSearchbarClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    object_class->get_property = bedit_searchbar_get_property;

    properties[PROP_SEARCH_ACTIVE] = g_param_spec_boolean(
        "search-active", "Search active",
        "Whether a search is in progress", FALSE,
        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS
    );

    properties[PROP_REPLACE_ACTIVE] = g_param_spec_boolean(
        "replace-active", "Replace active",
        "Whether a search is in progress and in replace mode", FALSE,
        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS
    );

    g_object_class_install_properties(object_class, LAST_PROP, properties);

    gtk_widget_class_set_template_from_resource(
        widget_class, "/com/bwhmather/bedit/ui/bedit-searchbar.ui"
    );

    gtk_widget_class_bind_template_child(
        widget_class, BeditSearchbar, revealer
    );

    gtk_widget_class_bind_template_child(
        widget_class, BeditSearchbar, search_entry
    );

    gtk_widget_class_bind_template_child(
        widget_class, BeditSearchbar, replace_entry
    );

    gtk_widget_class_bind_template_child(
        widget_class, BeditSearchbar, prev_button
    );
    gtk_widget_class_bind_template_child(
        widget_class, BeditSearchbar, next_button
    );

    gtk_widget_class_bind_template_child(
        widget_class, BeditSearchbar, match_case_toggle
    );
    gtk_widget_class_bind_template_child(
        widget_class, BeditSearchbar, regex_toggle
    );

    gtk_widget_class_bind_template_child(
        widget_class, BeditSearchbar, replace_button
    );
    gtk_widget_class_bind_template_child(
        widget_class, BeditSearchbar, replace_all_button
    );
}

static void bedit_searchbar_clear_start_mark(BeditSearchbar *searchbar) {
    GtkTextBuffer *buffer;

    bedit_debug(DEBUG_WINDOW);

    g_return_if_fail(BEDIT_IS_SEARCHBAR(searchbar));

    if (searchbar->start_mark == NULL) {
        return;
    }

    buffer = gtk_text_mark_get_buffer(searchbar->start_mark);
    gtk_text_buffer_delete_mark(buffer, searchbar->start_mark);

    searchbar->start_mark = NULL;
}

static void bedit_searchbar_set_start_mark(BeditSearchbar *searchbar) {
    GtkTextBuffer *buffer;
    GtkTextIter start_iter;

    bedit_debug(DEBUG_WINDOW);

    g_return_if_fail(BEDIT_IS_SEARCHBAR(searchbar));
    g_return_if_fail(GTK_IS_TEXT_VIEW(searchbar->view));
    g_return_if_fail(searchbar->start_mark == NULL);

    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(searchbar->view));

    gtk_text_buffer_get_selection_bounds(buffer, &start_iter, NULL);

    searchbar->start_mark = gtk_text_buffer_create_mark(
        buffer, NULL, &start_iter, FALSE
    );
}

static void bedit_searchbar_reset_start_mark(BeditSearchbar *searchbar) {
    bedit_debug(DEBUG_WINDOW);

    g_return_if_fail(BEDIT_IS_SEARCHBAR(searchbar));

    if (searchbar->view == NULL) {
        return;
    }
    bedit_searchbar_clear_start_mark(searchbar);
    bedit_searchbar_set_start_mark(searchbar);
}

static void bedit_searchbar_update_search_finished_cb(
    GtkSourceSearchContext *search_context,
    GAsyncResult *result,
    BeditSearchbar *searchbar
) {
    GtkTextIter match_start;
    GtkTextIter match_end;
    gboolean found;
    GtkSourceBuffer *buffer;

    bedit_debug(DEBUG_WINDOW);

    g_return_if_fail(BEDIT_IS_SEARCHBAR(searchbar));

    if (searchbar->view == NULL) {
        // Tab has been closed.  Search result is no longer relevant.
        return;
    }

    buffer = GTK_SOURCE_BUFFER(gtk_text_view_get_buffer(
        GTK_TEXT_VIEW(searchbar->view)
    ));
    if (buffer != gtk_source_search_context_get_buffer(search_context)) {
        // Tab has changed.  Search result is no longer relevant.
        return;
    }

    found = gtk_source_search_context_forward_finish(
        search_context, result,
        &match_start, &match_end,
        NULL, NULL
    );

    if (found) {
        g_signal_handler_block(
            buffer, searchbar->cursor_moved_cb_id
        );

        gtk_text_buffer_select_range(
            GTK_TEXT_BUFFER(buffer), &match_start, &match_end
        );

        g_signal_handler_unblock(
            buffer, searchbar->cursor_moved_cb_id
        );

        bedit_view_scroll_to_cursor(searchbar->view);
    } else {
        GtkTextIter start_at;

        gtk_text_buffer_get_iter_at_mark(
            GTK_TEXT_BUFFER(buffer), &start_at, searchbar->start_mark
        );

        gtk_text_buffer_select_range(
            GTK_TEXT_BUFFER(buffer), &start_at, &start_at
        );

        bedit_view_scroll_to_cursor(searchbar->view);
    }
}

static void bedit_searchbar_update_search_active(BeditSearchbar *searchbar) {
    gboolean search_active;
    gchar const *search_text;

    bedit_debug(DEBUG_WINDOW);

    g_return_if_fail(BEDIT_IS_SEARCHBAR(searchbar));

    search_active = TRUE;

    if (searchbar->view == NULL) {
        search_active = FALSE;
    }

    if (searchbar->mode == BEDIT_SEARCHBAR_MODE_HIDDEN) {
        search_active = FALSE;
    }

    search_text = gtk_entry_get_text(GTK_ENTRY(searchbar->search_entry));
    if (search_text == NULL || search_text[0] == '\0') {
        search_active = FALSE;
    }

    if (searchbar->search_active != search_active) {
        searchbar->search_active = search_active;
        g_object_notify_by_pspec(
            G_OBJECT(searchbar), properties[PROP_SEARCH_ACTIVE]
        );
    }
}

static void bedit_searchbar_update_replace_active(BeditSearchbar *searchbar) {
    gboolean replace_active;
    gchar const *search_text;

    bedit_debug(DEBUG_WINDOW);

    g_return_if_fail(BEDIT_IS_SEARCHBAR(searchbar));

    replace_active = TRUE;

    if (searchbar->view == NULL) {
        replace_active = FALSE;
    }

    if (searchbar->mode != BEDIT_SEARCHBAR_MODE_REPLACE) {
        replace_active = FALSE;
    }

    search_text = gtk_entry_get_text(GTK_ENTRY(searchbar->search_entry));
    if (search_text == NULL || search_text[0] == '\0') {
        replace_active = FALSE;
    }

    if (searchbar->replace_active != replace_active) {
        searchbar->replace_active = replace_active;
        g_object_notify_by_pspec(
            G_OBJECT(searchbar), properties[PROP_REPLACE_ACTIVE]
        );
    }
}

static void bedit_searchbar_update_search(BeditSearchbar *searchbar) {
    GtkSourceBuffer *buffer;
    GtkSourceSearchSettings *settings;
    GtkTextIter start_at;
    gchar const *search_text;
    gboolean case_sensitive;
    gboolean regex_enabled;

    bedit_debug(DEBUG_WINDOW);

    g_return_if_fail(BEDIT_IS_SEARCHBAR(searchbar));

    bedit_searchbar_update_search_active(searchbar);
    bedit_searchbar_update_replace_active(searchbar);

    if (searchbar->mode == BEDIT_SEARCHBAR_MODE_HIDDEN) {
        g_clear_object(&searchbar->context);
        return;
    }

    if (searchbar->view == NULL) {
        return;
    }

    buffer = GTK_SOURCE_BUFFER(gtk_text_view_get_buffer(
        GTK_TEXT_VIEW(searchbar->view)
    ));

    if (searchbar->context == NULL) {
        searchbar->context = gtk_source_search_context_new(buffer, NULL);
        searchbar->cancellable = g_cancellable_new();
    }

    /* Copy searchbar state to settings object. */
    settings = gtk_source_search_context_get_settings(searchbar->context);

    search_text = gtk_entry_get_text(GTK_ENTRY(searchbar->search_entry));
    gtk_source_search_settings_set_search_text(settings, search_text);

    case_sensitive = gtk_toggle_button_get_active(
        GTK_TOGGLE_BUTTON(searchbar->match_case_toggle)
    );
    gtk_source_search_settings_set_case_sensitive(settings, case_sensitive);

    regex_enabled = gtk_toggle_button_get_active(
        GTK_TOGGLE_BUTTON(searchbar->regex_toggle)
    );
    gtk_source_search_settings_set_regex_enabled(settings, regex_enabled);

    gtk_source_search_settings_set_wrap_around(settings, TRUE);

    /* Move cursor to first occurrence after start mark. */
    gtk_text_buffer_get_iter_at_mark(
        GTK_TEXT_BUFFER(buffer), &start_at, searchbar->start_mark
    );

    gtk_source_search_context_forward_async(
        searchbar->context, &start_at, searchbar->cancellable,
        (GAsyncReadyCallback)bedit_searchbar_update_search_finished_cb,
        searchbar
    );
}

static gboolean search_widget_key_press_cb(
    GtkWidget *widget, GdkEventKey *event, BeditSearchbar *searchbar
) {
    bedit_debug(DEBUG_WINDOW);

    g_return_val_if_fail(BEDIT_IS_SEARCHBAR(searchbar), GDK_EVENT_PROPAGATE);

    return GDK_EVENT_PROPAGATE;
}

static void search_entry_changed_cb(
    GtkEntry *entry, BeditSearchbar *searchbar
) {
    bedit_debug(DEBUG_WINDOW);

    g_return_if_fail(BEDIT_IS_SEARCHBAR(searchbar));

    bedit_searchbar_update_search(searchbar);
}

static void search_entry_escaped_cb(
    GtkSearchEntry *entry, BeditSearchbar *searchbar
) {
    bedit_debug(DEBUG_WINDOW);

    g_return_if_fail(BEDIT_IS_SEARCHBAR(searchbar));

    bedit_searchbar_hide(searchbar);
    if (searchbar->view != NULL) {
        gtk_widget_grab_focus(GTK_WIDGET(searchbar->view));
    }
}

static void replace_entry_escaped_cb(
    GtkSearchEntry *entry, BeditSearchbar *searchbar
) {
    bedit_debug(DEBUG_WINDOW);

    g_return_if_fail(BEDIT_IS_SEARCHBAR(searchbar));

    bedit_searchbar_hide(searchbar);
}

static void next_button_clicked_cb(
    GtkButton *button, BeditSearchbar *searchbar
) {
    bedit_debug(DEBUG_WINDOW);

    g_return_if_fail(BEDIT_IS_SEARCHBAR(searchbar));

    bedit_searchbar_next(searchbar);
}

static void prev_button_clicked_cb(
    GtkButton *button, BeditSearchbar *searchbar
) {
    bedit_debug(DEBUG_WINDOW);

    g_return_if_fail(BEDIT_IS_SEARCHBAR(searchbar));

    bedit_searchbar_prev(searchbar);
}

// TODO sould be handled by the window.
static void next_match_cb(
    GtkSearchEntry *entry, BeditSearchbar *searchbar
) {
    bedit_debug(DEBUG_WINDOW);

    g_return_if_fail(BEDIT_IS_SEARCHBAR(searchbar));

    bedit_searchbar_next(searchbar);
}

// TODO sould be handled by the window.
static void prev_match_cb(
    GtkSearchEntry *entry, BeditSearchbar *searchbar
) {
    bedit_debug(DEBUG_WINDOW);

    g_return_if_fail(BEDIT_IS_SEARCHBAR(searchbar));

    bedit_searchbar_prev(searchbar);
}

static void replace_button_clicked_cb(
    GtkButton *button, BeditSearchbar *searchbar
) {
    bedit_debug(DEBUG_WINDOW);

    g_return_if_fail(BEDIT_IS_SEARCHBAR(searchbar));

    bedit_searchbar_replace(searchbar);
}

static void replace_all_button_clicked_cb(
    GtkButton *button, BeditSearchbar *searchbar
) {
    bedit_debug(DEBUG_WINDOW);

    g_return_if_fail(BEDIT_IS_SEARCHBAR(searchbar));

    bedit_searchbar_replace_all(searchbar);
}

static void bedit_searchbar_init(BeditSearchbar *searchbar) {
    gtk_widget_init_template(GTK_WIDGET(searchbar));

    g_signal_connect(
        searchbar, "key-press-event",
        G_CALLBACK(search_widget_key_press_cb), searchbar
    );

    g_signal_connect(
        searchbar->search_entry, "changed",
        G_CALLBACK(search_entry_changed_cb), searchbar
    );

    g_signal_connect(
        searchbar->search_entry, "stop-search",
        G_CALLBACK(search_entry_escaped_cb), searchbar
    );

    // TODO this should handled by the window.
    g_signal_connect(
        searchbar->search_entry, "next-match",
        G_CALLBACK(next_match_cb), searchbar
    );

    // TODO this should handled by the window.
    g_signal_connect(
        searchbar->search_entry, "previous-match",
        G_CALLBACK(prev_match_cb), searchbar
    );

    g_signal_connect(
        searchbar->replace_entry, "stop-search",
        G_CALLBACK(replace_entry_escaped_cb), searchbar
    );

    // TODO this should handled by the window.
    g_signal_connect(
        searchbar->replace_entry, "next-match",
        G_CALLBACK(next_match_cb), searchbar
    );

    // TODO this should handled by the window.
    g_signal_connect(
        searchbar->replace_entry, "previous-match",
        G_CALLBACK(prev_match_cb), searchbar
    );

    g_signal_connect(
        searchbar->next_button, "clicked",
        G_CALLBACK(next_button_clicked_cb), searchbar
    );

    g_signal_connect(
        searchbar->prev_button, "clicked",
        G_CALLBACK(prev_button_clicked_cb), searchbar
    );

    g_signal_connect(
        searchbar->replace_button, "clicked",
        G_CALLBACK(replace_button_clicked_cb), searchbar
    );

    g_signal_connect(
        searchbar->replace_all_button, "clicked",
        G_CALLBACK(replace_all_button_clicked_cb), searchbar
    );
}

static void bedit_searchbar_cursor_moved_cb(
    GtkTextBuffer *buffer, BeditSearchbar *searchbar
) {
    bedit_debug(DEBUG_WINDOW);

    g_return_if_fail(BEDIT_IS_SEARCHBAR(searchbar));

    bedit_searchbar_reset_start_mark(searchbar);
}

/**
 * bedit_searchbar_new:
 *
 * Creates a new #BeditSearchbar.
 *
 * Return value: the new #BeditSearchbar object
 **/
GtkWidget *bedit_searchbar_new(void) {
    return GTK_WIDGET(g_object_new(BEDIT_TYPE_SEARCHBAR, NULL));
}

/**
 * bedit_searchbar_set_overwrite:
 * @searchbar: a #BeditSearchbar
 * @view: the new view to bind, or NULL to unbind the current document.
 *
 * Binds the search-bar to a new view, safely unbinding any previously bound
 * view.
 **/
void bedit_searchbar_set_view(
    BeditSearchbar *searchbar, BeditView *view
) {
    bedit_debug(DEBUG_WINDOW);

    g_return_if_fail(BEDIT_IS_SEARCHBAR(searchbar));
    g_return_if_fail(BEDIT_IS_VIEW(view) || view == NULL);

    if (view == searchbar->view) {
        return;
    }

    if (searchbar->view != NULL) {
        BeditDocument *document;

        if (searchbar->cancellable != NULL) {
            g_cancellable_cancel(searchbar->cancellable);
            g_clear_object(&searchbar->cancellable);
        }

        if (searchbar->context != NULL) {
            g_clear_object(&searchbar->context);
        }

        document = BEDIT_DOCUMENT(gtk_text_view_get_buffer(
            GTK_TEXT_VIEW(searchbar->view)
        ));

        g_signal_handler_disconnect(
            document, searchbar->cursor_moved_cb_id
        );

        bedit_searchbar_clear_start_mark(searchbar);
    }

    searchbar->view = view;
    if (searchbar->view != NULL) {
        BeditDocument *document = BEDIT_DOCUMENT(gtk_text_view_get_buffer(
            GTK_TEXT_VIEW(searchbar->view)
        ));

        bedit_searchbar_set_start_mark(searchbar);

        searchbar->cursor_moved_cb_id = g_signal_connect(
            document, "cursor-moved",
            G_CALLBACK(bedit_searchbar_cursor_moved_cb),
            searchbar
        );
    }

    bedit_searchbar_update_search(searchbar);
}

static void bedit_searchbar_set_replace_visible(
    BeditSearchbar *searchbar, gboolean visible
) {
    bedit_debug(DEBUG_WINDOW);

    g_return_if_fail(BEDIT_IS_SEARCHBAR(searchbar));

    visible = !!visible;

    gtk_widget_set_visible(searchbar->replace_entry, visible);
    gtk_widget_set_visible(searchbar->replace_button, visible);
    gtk_widget_set_visible(searchbar->replace_all_button, visible);
}

/**
 * bedit_searchbar_show_find:
 * @searchbar: a #BeditSearchbar
 *
 * Unhides the find part of the searchbar.  If the searchbar is hidden it will
 * pop up.  If the replace part of the searchbar is visible it will be hidden.
 */
void bedit_searchbar_show_find(BeditSearchbar *searchbar) {
    bedit_debug(DEBUG_WINDOW);

    g_return_if_fail(BEDIT_IS_SEARCHBAR(searchbar));

    if (searchbar->mode == BEDIT_SEARCHBAR_MODE_HIDDEN) {
        bedit_searchbar_reset_start_mark(searchbar);
    }

    searchbar->mode = BEDIT_SEARCHBAR_MODE_FIND;

    bedit_searchbar_set_replace_visible(searchbar, FALSE);
    gtk_revealer_set_reveal_child(GTK_REVEALER(searchbar->revealer), TRUE);

    gtk_widget_grab_focus(GTK_WIDGET(searchbar->search_entry));

    bedit_searchbar_update_search(searchbar);
}

/**
 * bedit_searchbar_show_replace:
 * @searchbar: a #BeditSearchbar
 *
 * Unhides both the find and replace parts of the searchbar.
 */
void bedit_searchbar_show_replace(BeditSearchbar *searchbar) {
    bedit_debug(DEBUG_WINDOW);

    g_return_if_fail(BEDIT_IS_SEARCHBAR(searchbar));

    if (searchbar->mode == BEDIT_SEARCHBAR_MODE_HIDDEN) {
        bedit_searchbar_reset_start_mark(searchbar);
    }

    searchbar->mode = BEDIT_SEARCHBAR_MODE_REPLACE;

    bedit_searchbar_set_replace_visible(searchbar, TRUE);
    gtk_revealer_set_reveal_child(GTK_REVEALER(searchbar->revealer), TRUE);

    gtk_widget_grab_focus(GTK_WIDGET(searchbar->search_entry));

    bedit_searchbar_update_search(searchbar);
}

/**
 * bedit_searchbar_hide:
 * @searchbar: the #BeditSearchbar to hide
 *
 * Safe to call if the searchbar is already hidden.
 */
void bedit_searchbar_hide(BeditSearchbar *searchbar) {
    bedit_debug(DEBUG_WINDOW);

    g_return_if_fail(BEDIT_IS_SEARCHBAR(searchbar));

    searchbar->mode = BEDIT_SEARCHBAR_MODE_HIDDEN;

    gtk_revealer_set_reveal_child(GTK_REVEALER(searchbar->revealer), FALSE);

    gtk_widget_grab_focus(GTK_WIDGET(searchbar->search_entry));

    bedit_searchbar_update_search(searchbar);
}

static void bedit_searchbar_next_finished_cb(
    GtkSourceSearchContext *search_context,
    GAsyncResult *result, BeditSearchbar *searchbar
) {
    GtkTextIter match_start;
    GtkTextIter match_end;
    gboolean found;
    GtkSourceBuffer *buffer;

    bedit_debug(DEBUG_WINDOW);

    g_return_if_fail(BEDIT_IS_SEARCHBAR(searchbar));

    if (searchbar->view == NULL) {
        // Tab has been closed.  Search result is no longer relevant.
        return;
    }

    buffer = GTK_SOURCE_BUFFER(gtk_text_view_get_buffer(
        GTK_TEXT_VIEW(searchbar->view)
    ));
    if (buffer != gtk_source_search_context_get_buffer(search_context)) {
        // Tab has changed.  Search result is no longer relevant.
        return;
    }

    found = gtk_source_search_context_forward_finish(
        search_context, result,
        &match_start, &match_end,
        NULL, NULL
    );

    if (found) {
        gtk_text_buffer_select_range(
            GTK_TEXT_BUFFER(buffer), &match_start, &match_end
        );

        bedit_view_scroll_to_cursor(searchbar->view);
    }
}

void bedit_searchbar_next(BeditSearchbar *searchbar) {
    GtkTextBuffer *buffer;
    GtkSourceSearchContext *search_context;
    GtkTextIter start_at;
    GCancellable *cancellable;

    bedit_debug(DEBUG_WINDOW);

    g_return_if_fail(BEDIT_IS_SEARCHBAR(searchbar));

    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(searchbar->view));
    if (buffer == NULL) {
        return;
    }

    search_context = searchbar->context;
    if (search_context == NULL) {
        return;
    }

    cancellable = searchbar->cancellable;
    g_return_if_fail(G_IS_CANCELLABLE(cancellable));

    gtk_text_buffer_get_selection_bounds(buffer, NULL, &start_at);

    gtk_source_search_context_forward_async(
        search_context, &start_at, cancellable,
        (GAsyncReadyCallback)bedit_searchbar_next_finished_cb, searchbar
    );
}

static void bedit_searchbar_prev_finished_cb(
    GtkSourceSearchContext *search_context,
    GAsyncResult *result, BeditSearchbar *searchbar
) {
    GtkTextIter match_start;
    GtkTextIter match_end;
    gboolean found;
    GtkSourceBuffer *buffer;

    bedit_debug(DEBUG_WINDOW);

    g_return_if_fail(BEDIT_IS_SEARCHBAR(searchbar));

    if (searchbar->view == NULL) {
        // Tab has been closed.  Search result is no longer relevant.
        return;
    }

    buffer = GTK_SOURCE_BUFFER(gtk_text_view_get_buffer(
        GTK_TEXT_VIEW(searchbar->view)
    ));
    if (buffer != gtk_source_search_context_get_buffer(search_context)) {
        // Tab has changed.  Search result is no longer relevant.
        return;
    }

    found = gtk_source_search_context_backward_finish(
        search_context, result,
        &match_start, &match_end,
        NULL, NULL
    );

    if (found) {
        gtk_text_buffer_select_range(
            GTK_TEXT_BUFFER(buffer), &match_start, &match_end
        );

        bedit_view_scroll_to_cursor(searchbar->view);
    }
}

void bedit_searchbar_prev(BeditSearchbar *searchbar) {
    GtkTextBuffer *buffer;
    GtkSourceSearchContext *search_context;
    GtkTextIter start_at;
    GCancellable *cancellable;

    bedit_debug(DEBUG_WINDOW);

    g_return_if_fail(BEDIT_IS_SEARCHBAR(searchbar));

    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(searchbar->view));
    if (buffer == NULL) {
        return;
    }

    search_context = searchbar->context;
    if (search_context == NULL) {
        return;
    }

    cancellable = searchbar->cancellable;
    g_return_if_fail(G_IS_CANCELLABLE(cancellable));

    gtk_text_buffer_get_selection_bounds(buffer, &start_at, NULL);

    gtk_source_search_context_backward_async(
        search_context, &start_at, cancellable,
        (GAsyncReadyCallback)bedit_searchbar_prev_finished_cb, searchbar
    );
}

void bedit_searchbar_replace(BeditSearchbar *searchbar) {
    GtkSourceSearchContext *search_context;
    GtkTextBuffer *buffer;
    gchar const* replace_text;
    gchar* replace_text_unescaped;
    GtkTextIter selection_start;
    GtkTextIter selection_end;
    GError *error = NULL;

    bedit_debug(DEBUG_WINDOW);

    g_return_if_fail(BEDIT_IS_SEARCHBAR(searchbar));

    if (!bedit_searchbar_get_replace_active(searchbar)) {
        return;
    }

    search_context = searchbar->context;
    g_return_if_fail(GTK_SOURCE_IS_SEARCH_CONTEXT(search_context));


    replace_text = gtk_entry_get_text(GTK_ENTRY(searchbar->replace_entry));
    g_return_if_fail(replace_text != NULL);

	replace_text_unescaped =
	    gtk_source_utils_unescape_search_text(replace_text);

    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(searchbar->view));
	gtk_text_buffer_get_selection_bounds(
	    buffer, &selection_start, &selection_end
	);

	gtk_source_search_context_replace(
	    search_context,
	    &selection_start, &selection_end,
	    replace_text_unescaped, -1, &error
    );

	g_free(replace_text_unescaped);

	if (error != NULL) {
		// gedit_replace_dialog_set_replace_error (dialog, error->message);
		g_error_free(error);
	}

	bedit_searchbar_next(searchbar);
}

void bedit_searchbar_replace_all(BeditSearchbar *searchbar) {
    BeditView *view;
    GtkSourceSearchContext *search_context;
    GtkSourceCompletion *completion;
    gchar const* replace_text;
    gchar* replace_text_unescaped;
    gint count;
    GError *error = NULL;

    bedit_debug(DEBUG_WINDOW);

    g_return_if_fail(BEDIT_IS_SEARCHBAR(searchbar));

    if (!bedit_searchbar_get_replace_active(searchbar)) {
        return;
    }

    view = searchbar->view;
    g_return_if_fail(BEDIT_IS_VIEW(view));

    search_context = searchbar->context;
    g_return_if_fail(GTK_SOURCE_IS_SEARCH_CONTEXT(search_context));

	completion = gtk_source_view_get_completion(GTK_SOURCE_VIEW(view));
	gtk_source_completion_block_interactive(completion);

    replace_text = gtk_entry_get_text(GTK_ENTRY(searchbar->replace_entry));
    g_return_if_fail(replace_text != NULL);

	replace_text_unescaped =
	    gtk_source_utils_unescape_search_text(replace_text);

	count = gtk_source_search_context_replace_all(
	    search_context, replace_text_unescaped, -1, &error
	);

	g_free(replace_text_unescaped);

	gtk_source_completion_unblock_interactive (completion);
}


/**
 * bedit_searchbar_get_search_active:
 *
 * Returns true if the searchbar is visible and has an active search in
 * progress, i.e. has a valid search query.  Returns false otherwise.
 */
gboolean bedit_searchbar_get_search_active(BeditSearchbar *searchbar) {
    bedit_debug(DEBUG_WINDOW);

    g_return_val_if_fail(BEDIT_IS_SEARCHBAR(searchbar), FALSE);

    return searchbar->search_active;
}

/**
 * bedit_searchbar_get_replace_active:
 *
 * Returns true if the replace part of the searchbar is visible and there is a
 * valid search in progress.
 * Calls to #bedit_searchbar_replace and #bedit_searchbar_replace_all will be
 * ignored if this is not true.
 */
gboolean bedit_searchbar_get_replace_active(BeditSearchbar *searchbar) {
    bedit_debug(DEBUG_WINDOW);

    g_return_val_if_fail(BEDIT_IS_SEARCHBAR(searchbar), FALSE);

    return searchbar->replace_active;
}
