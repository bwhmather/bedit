/*
 * bedit-notebook-popup-menu.c
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * Based on gedit-notebook-popup-menu.c from Gedit.
 *
 * Copyright (C) 2011-2014 - Ignacio Casal Quinteiro
 * Copyright (C) 2013 - Garrett Regier
 * Copyright (C) 2013-2015 - Sébastien Wilmet
 * Copyright (C) 2014 - Robert Roth, Sebastien Lafargue
 * Copyright (C) 2014-2015 - Paolo Borelli
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

#include "bedit-notebook-popup-menu.h"

#include <glib/gi18n.h>

#include "bedit-app-private.h"
#include "bedit-app.h"
#include "bedit-commands-private.h"
#include "bedit-notebook.h"

struct _BeditNotebookPopupMenu {
    GtkMenu parent_instance;

    BeditWindow *window;
    BeditTab *tab;

    GSimpleActionGroup *action_group;
};

enum { PROP_0, PROP_WINDOW, PROP_TAB, LAST_PROP };

static GParamSpec *properties[LAST_PROP];

G_DEFINE_TYPE(BeditNotebookPopupMenu, bedit_notebook_popup_menu, GTK_TYPE_MENU)

static void bedit_notebook_popup_menu_set_property(
    GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec
) {
    BeditNotebookPopupMenu *menu = BEDIT_NOTEBOOK_POPUP_MENU(object);

    switch (prop_id) {
    case PROP_WINDOW:
        menu->window = BEDIT_WINDOW(g_value_get_object(value));
        break;

    case PROP_TAB:
        menu->tab = BEDIT_TAB(g_value_get_object(value));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void bedit_notebook_popup_menu_get_property(
    GObject *object, guint prop_id, GValue *value, GParamSpec *pspec
) {
    BeditNotebookPopupMenu *menu = BEDIT_NOTEBOOK_POPUP_MENU(object);

    switch (prop_id) {
    case PROP_WINDOW:
        g_value_set_object(value, menu->window);
        break;

    case PROP_TAB:
        g_value_set_object(value, menu->tab);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void update_sensitivity(BeditNotebookPopupMenu *menu) {
    BeditTabState state;
    GtkNotebook *notebook;
    gint page_num;
    gint n_pages;
    GAction *action;

    state = bedit_tab_get_state(menu->tab);

    notebook = GTK_NOTEBOOK(_bedit_window_get_notebook(menu->window));
    n_pages = gtk_notebook_get_n_pages(notebook);
    page_num = gtk_notebook_page_num(notebook, GTK_WIDGET(menu->tab));

    action = g_action_map_lookup_action(
        G_ACTION_MAP(menu->action_group), "close"
    );
    g_simple_action_set_enabled(
        G_SIMPLE_ACTION(action),
        (state != BEDIT_TAB_STATE_CLOSING) &&
        (state != BEDIT_TAB_STATE_SAVING) &&
        (state != BEDIT_TAB_STATE_SHOWING_PRINT_PREVIEW) &&
        (state != BEDIT_TAB_STATE_PRINTING) &&
        (state != BEDIT_TAB_STATE_SAVING_ERROR)
    );

    action = g_action_map_lookup_action(
        G_ACTION_MAP(menu->action_group), "move-to-new-window"
    );
    g_simple_action_set_enabled(G_SIMPLE_ACTION(action), n_pages > 1);

    action = g_action_map_lookup_action(
        G_ACTION_MAP(menu->action_group), "move-left"
    );
    g_simple_action_set_enabled(G_SIMPLE_ACTION(action), page_num > 0);

    action = g_action_map_lookup_action(
        G_ACTION_MAP(menu->action_group), "move-right"
    );
    g_simple_action_set_enabled(
        G_SIMPLE_ACTION(action), page_num < n_pages - 1
    );
}

static void bedit_notebook_popup_menu_constructed(GObject *object) {
    BeditNotebookPopupMenu *menu = BEDIT_NOTEBOOK_POPUP_MENU(object);

    update_sensitivity(menu);

    G_OBJECT_CLASS(bedit_notebook_popup_menu_parent_class)
        ->constructed(object);
}

static void bedit_notebook_popup_menu_class_init(
    BeditNotebookPopupMenuClass *klass
) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->get_property = bedit_notebook_popup_menu_get_property;
    object_class->set_property = bedit_notebook_popup_menu_set_property;
    object_class->constructed = bedit_notebook_popup_menu_constructed;

    properties[PROP_WINDOW] = g_param_spec_object(
        "window", "Window", "The BeditWindow", BEDIT_TYPE_WINDOW,
        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS
    );

    properties[PROP_TAB] = g_param_spec_object(
        "tab", "Tab", "The BeditTab", BEDIT_TYPE_TAB,
        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS
    );

    g_object_class_install_properties(object_class, LAST_PROP, properties);
}

static void on_move_left_activate(
    GSimpleAction *action, GVariant *parameter, gpointer user_data
) {
    BeditNotebookPopupMenu *menu = BEDIT_NOTEBOOK_POPUP_MENU(user_data);
    GtkNotebook *notebook;
    gint page_num;

    notebook = GTK_NOTEBOOK(_bedit_window_get_notebook(menu->window));
    page_num = gtk_notebook_page_num(notebook, GTK_WIDGET(menu->tab));

    if (page_num > 0) {
        gtk_notebook_reorder_child(
            notebook, GTK_WIDGET(menu->tab), page_num - 1
        );
    }
}

static void on_move_right_activate(
    GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    BeditNotebookPopupMenu *menu = BEDIT_NOTEBOOK_POPUP_MENU(user_data);
    GtkNotebook *notebook;
    gint page_num;
    gint n_pages;

    notebook = GTK_NOTEBOOK(_bedit_window_get_notebook(menu->window));
    n_pages = gtk_notebook_get_n_pages(notebook);
    page_num = gtk_notebook_page_num(notebook, GTK_WIDGET(menu->tab));

    if (page_num < (n_pages - 1)) {
        gtk_notebook_reorder_child(
            notebook, GTK_WIDGET(menu->tab), page_num + 1
        );
    }
}

static void on_move_to_new_window_activate(
    GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    BeditNotebookPopupMenu *menu = BEDIT_NOTEBOOK_POPUP_MENU(user_data);

    _bedit_window_move_tab_to_new_window(menu->window, menu->tab);
}

static void on_close_activate(
    GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    BeditNotebookPopupMenu *menu = BEDIT_NOTEBOOK_POPUP_MENU(user_data);

    (void) action;
    (void) parameter;

    _bedit_cmd_file_close_tab(menu->tab, menu->window);
}

static GActionEntry action_entries[] = {
    {"move-left", on_move_left_activate, NULL, NULL, NULL, {0, 0, 0}},
    {"move-right", on_move_right_activate, NULL, NULL, NULL, {0, 0, 0}},
    {"move-to-new-window", on_move_to_new_window_activate, NULL, NULL, NULL, {0, 0, 0}},
    {"close", on_close_activate, NULL, NULL, NULL, {0, 0, 0}}
};

static void bedit_notebook_popup_menu_init(BeditNotebookPopupMenu *menu) {
    gtk_menu_shell_bind_model(
        GTK_MENU_SHELL(menu),
        _bedit_app_get_notebook_menu(BEDIT_APP(g_application_get_default())),
        "popup", TRUE
    );

    menu->action_group = g_simple_action_group_new();
    g_action_map_add_action_entries(
        G_ACTION_MAP(menu->action_group), action_entries,
        G_N_ELEMENTS(action_entries), menu
    );

    gtk_widget_insert_action_group(
        GTK_WIDGET(menu), "popup", G_ACTION_GROUP(menu->action_group)
    );
}

GtkWidget *bedit_notebook_popup_menu_new(BeditWindow *window, BeditTab *tab) {
    return g_object_new(
        BEDIT_TYPE_NOTEBOOK_POPUP_MENU,
        "window", window,
        "tab", tab, NULL
    );
}

