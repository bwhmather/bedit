/*
 * bedit-window-private.h
 * This file is part of bedit
 *
 * Copyright (C) 2005 - Paolo Maggi
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANWINDOWILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef BEDIT_WINDOW_PRIVATE_H
#define BEDIT_WINDOW_PRIVATE_H

#include <libpeas/peas-extension-set.h>

#include "bedit-message-bus.h"
#include "bedit-settings.h"
#include "bedit/bedit-window.h"

G_BEGIN_DECLS

/* WindowPrivate is in a separate .h so that we can access it from
 * bedit-commands */

struct _BeditWindowPrivate {
    GSettings *editor_settings;
    GSettings *ui_settings;
    GSettings *window_settings;

    BeditNotebook *notebook;
    BeditTab *active_tab;

    GtkWidget *side_panel_box;
    GtkWidget *side_panel;
    GtkWidget *side_stack_switcher;
    GtkWidget *side_panel_inline_stack_switcher;

    GtkWidget *hpaned;

    BeditMessageBus *message_bus;
    PeasExtensionSet *extensions;

    /* statusbar and context ids for statusbar messages */
    GtkWidget *statusbar;
    GtkWidget *line_col_button;
    GtkWidget *tab_width_button;
    GtkWidget *language_button;
    GtkWidget *language_button_label;
    GtkWidget *language_popover;
    guint generic_message_cid;
    guint tip_message_cid;
    guint bracket_match_message_cid;
    guint tab_width_id;
    guint language_changed_id;
    guint wrap_mode_changed_id;

    /* Headerbars */
    GtkWidget *titlebar_paned;
    GtkWidget *side_headerbar;
    GtkWidget *headerbar;

    GtkMenuButton *gear_button;
    GtkMenuButton *leave_fullscreen_button;

    gint num_tabs_with_error;

    gint width;
    gint height;
    GdkWindowState window_state;

    gint side_panel_size;

    BeditWindowState state;

    guint inhibition_cookie;

    GtkWindowGroup *window_group;

    GFile *default_location;

    gchar *direct_save_uri;

    GSList *closed_docs_stack;

    guint removing_tabs : 1;
    guint dispose_has_run : 1;

    guint in_fullscreen_eventbox : 1;
};

G_END_DECLS

#endif /* BEDIT_WINDOW_PRIVATE_H  */

