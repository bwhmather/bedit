/*
 * bedit-window-private.h
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * Based on gedit-window-private.h from Gedit.
 *
 * Copyright (C) 2005-2014 - Paolo Maggi
 * Copyright (C) 2005-2014 - Paolo Borelli
 * Copyright (C) 2008-2014 - Jesse van den Kieboom
 * Copyright (C) 2009-2014 - Ignacio Casal Quinteiro
 * Copyright (C) 2010 - Garrett Regier
 * Copyright (C) 2010-2014 - Steve Frécinaux
 * Copyright (C) 2013 - Nelson Benítez León
 * Copyright (C) 2013-2019 - Sébastien Wilmet
 * Copyright (C) 2014 - Daniel Mustieles, Sagar Ghuge, Sebastien Lafargue
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

#ifndef BEDIT_WINDOW_PRIVATE_H
#define BEDIT_WINDOW_PRIVATE_H

#include <libpeas/peas-extension-set.h>

#include "bedit-message-bus.h"
#include "bedit-settings.h"
#include "bedit/bedit-window.h"
#include "bedit-file-browser.h"


G_BEGIN_DECLS

/* WindowPrivate is in a separate .h so that we can access it from
 * bedit-commands */

struct _BeditWindowPrivate {
    GSettings *editor_settings;
    GSettings *ui_settings;
    GSettings *window_settings;

    BeditNotebook *notebook;
    BeditTab *active_tab;

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

    /* searchbar */
    GtkWidget *searchbar;

    /* Tab bar widgets */
    GtkWidget *action_area;
    GtkWidget *gear_button;
    GtkWidget *leave_fullscreen_button;
    BeditFileBrowser *file_browser;

    gint num_tabs_with_error;

    gint width;
    gint height;
    GdkWindowState window_state;

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

