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
};

G_DEFINE_TYPE(BeditSearchbar, bedit_searchbar, GTK_TYPE_BIN)

static void bedit_searchbar_dispose(GObject *object) {
    BeditSearchbar *searchbar = BEDIT_SEARCHBAR(object);

    // TODO remove document search.

    G_OBJECT_CLASS(bedit_searchbar_parent_class)->dispose(object);
}

static void bedit_searchbar_class_init(BeditSearchbarClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    object_class->dispose = bedit_searchbar_dispose;

    gtk_widget_class_set_template_from_resource(
        widget_class, "/com/bwhmather/bedit/ui/bedit-searchbar.ui");

    gtk_widget_class_bind_template_child(
        widget_class, BeditSearchbar, revealer);

    gtk_widget_class_bind_template_child(
        widget_class, BeditSearchbar, search_entry);

    gtk_widget_class_bind_template_child(
        widget_class, BeditSearchbar, replace_entry);

    gtk_widget_class_bind_template_child(
        widget_class, BeditSearchbar, prev_button);
    gtk_widget_class_bind_template_child(
        widget_class, BeditSearchbar, next_button);

    gtk_widget_class_bind_template_child(
        widget_class, BeditSearchbar, match_case_toggle);
    gtk_widget_class_bind_template_child(
        widget_class, BeditSearchbar, regex_toggle);

    gtk_widget_class_bind_template_child(
        widget_class, BeditSearchbar, replace_button);
    gtk_widget_class_bind_template_child(
        widget_class, BeditSearchbar, replace_all_button);
}

static void bedit_searchbar_update_search(BeditSearchbar *searchbar) {
    GtkSourceSearchSettings *settings;
    gchar *search_text;

    bedit_debug(DEBUG_WINDOW);

    g_return_if_fail(BEDIT_IS_SEARCHBAR(searchbar));

    if (searchbar->mode == BEDIT_SEARCHBAR_MODE_HIDDEN) {
        g_clear_object(&searchbar->context);
        return;
    }

    if (searchbar->view == NULL) {
        return;
    }

    if (searchbar->context == NULL) {
        GtkSourceBuffer *buffer = GTK_SOURCE_BUFFER(gtk_text_view_get_buffer(
            GTK_TEXT_VIEW(searchbar->view)));
        searchbar->context = gtk_source_search_context_new(buffer, NULL);
    }

    settings = gtk_source_search_context_get_settings(searchbar->context);

    search_text = gtk_entry_get_text(GTK_ENTRY(searchbar->search_entry));
    gtk_source_search_settings_set_search_text(settings, search_text);
}

static gboolean search_widget_key_press_cb(
    GtkWidget *widget, GdkEventKey *event, BeditSearchbar *searchbar) {
    bedit_debug(DEBUG_WINDOW);

    g_return_val_if_fail(BEDIT_IS_SEARCHBAR(searchbar), GDK_EVENT_PROPAGATE);

	return GDK_EVENT_PROPAGATE;
}

static void search_entry_changed_cb(
    GtkEntry *entry, BeditSearchbar *searchbar) {
	bedit_debug(DEBUG_WINDOW);

    g_return_if_fail(BEDIT_IS_SEARCHBAR(searchbar));

    bedit_searchbar_update_search(searchbar);
}

static void search_entry_escaped_cb(
    GtkSearchEntry *entry, BeditSearchbar *searchbar) {
	bedit_debug(DEBUG_WINDOW);
	
    g_return_if_fail(BEDIT_IS_SEARCHBAR(searchbar));

    bedit_searchbar_hide(searchbar);
    if (searchbar->view != NULL) {
        gtk_widget_grab_focus(GTK_WIDGET(searchbar->view));
    }
}

static void replace_entry_escaped_cb(
    GtkSearchEntry *entry, BeditSearchbar *searchbar) {
	bedit_debug(DEBUG_WINDOW);
	
    g_return_if_fail(BEDIT_IS_SEARCHBAR(searchbar));

    bedit_searchbar_hide(searchbar);
}

static void bedit_searchbar_init(BeditSearchbar *searchbar) {
    gtk_widget_init_template(GTK_WIDGET(searchbar));

	g_signal_connect(
	    searchbar, "key-press-event",
        G_CALLBACK(search_widget_key_press_cb), searchbar);

	g_signal_connect(
	    searchbar->search_entry, "changed",
        G_CALLBACK(search_entry_changed_cb), searchbar);

	g_signal_connect(
	    searchbar->search_entry, "stop-search",
        G_CALLBACK(search_entry_escaped_cb), searchbar);

    g_signal_connect(
        searchbar->replace_entry, "stop-search",
        G_CALLBACK(replace_entry_escaped_cb), searchbar);
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
    BeditSearchbar *searchbar, BeditView *view) {
    bedit_debug(DEBUG_WINDOW);

    g_return_if_fail(BEDIT_IS_SEARCHBAR(searchbar));
    g_return_if_fail(BEDIT_IS_VIEW(view) || view == NULL);

    g_clear_object(&searchbar->context);
    searchbar->view = view;

    bedit_searchbar_update_search(searchbar);
}

static void bedit_searchbar_set_replace_visible(
    BeditSearchbar *searchbar, gboolean visible) {
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

