/*
 * bedit-commands-search.c
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * Based on gedit-commands-search.c from Gedit.
 *
 * Copyright (C) 1998-1999 - Alex Roberts, Evan Lawrence
 * Copyright (C) 2000-2001 - Chema Celorio
 * Copyright (C) 2000-2007 - Paolo Maggi
 * Copyright (C) 2005-2015 - Paolo Borelli
 * Copyright (C) 2006-2007 - Paolo Maggi
 * Copyright (C) 2009-2013 - Ignacio Casal Quinteiro
 * Copyright (C) 2010 - Garrett Regier, Javier Jardón, Jesse van den Kieboom, Steve Frécinaux
 * Copyright (C) 2013-2019 - Sébastien Wilmet
 * Copyright (C) 2014 - Oliver Gerlich, Robert Roth
 * Copyright (C) 2016 - Piotr Drąg
 * Copyright (C) 2018 - Christian Hergert
 * Copyright (C) 2019 - Michael Catanzaro
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

#include "bedit-commands-private.h"
#include "bedit-commands.h"

#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>
#include <string.h>

#include "bedit-debug.h"
#include "bedit-replace-dialog.h"
#include "bedit-searchbar.h"
#include "bedit-statusbar.h"
#include "bedit-tab-private.h"
#include "bedit-tab.h"
#include "bedit-utils.h"
#include "bedit-view-frame.h"
#include "bedit-window-private.h"
#include "bedit-window.h"

#define BEDIT_REPLACE_DIALOG_KEY "bedit-replace-dialog-key"
#define BEDIT_LAST_SEARCH_DATA_KEY "bedit-last-search-data-key"

#define MAX_MSG_LENGTH 40

void _bedit_cmd_search_find(
    GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    BeditWindow *window = BEDIT_WINDOW(user_data);

    bedit_debug(DEBUG_COMMANDS);

    bedit_searchbar_show_find(BEDIT_SEARCHBAR(window->priv->searchbar));
}

void _bedit_cmd_search_replace(
    GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    BeditWindow *window = BEDIT_WINDOW(user_data);

    bedit_debug(DEBUG_COMMANDS);

    bedit_searchbar_show_replace(BEDIT_SEARCHBAR(window->priv->searchbar));
}

void _bedit_cmd_search_find_next(
    GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    BeditWindow *window = BEDIT_WINDOW(user_data);

    bedit_debug(DEBUG_COMMANDS);

    bedit_searchbar_next(BEDIT_SEARCHBAR(window->priv->searchbar));
}

void _bedit_cmd_search_find_prev(
    GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    BeditWindow *window = BEDIT_WINDOW(user_data);

    bedit_debug(DEBUG_COMMANDS);

    bedit_searchbar_prev(BEDIT_SEARCHBAR(window->priv->searchbar));
}

void _bedit_cmd_search_clear_highlight(
    GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    BeditWindow *window = BEDIT_WINDOW(user_data);
    BeditTab *active_tab;
    BeditViewFrame *frame;
    BeditDocument *doc;

    bedit_debug(DEBUG_COMMANDS);

    active_tab = bedit_window_get_active_tab(window);

    if (active_tab == NULL) {
        return;
    }

    frame = _bedit_tab_get_view_frame(active_tab);
    bedit_view_frame_clear_search(frame);

    doc = bedit_tab_get_document(active_tab);
    bedit_document_set_search_context(doc, NULL);
}

void _bedit_cmd_search_goto_line(
    GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    BeditWindow *window = BEDIT_WINDOW(user_data);
    BeditTab *active_tab;
    BeditViewFrame *frame;

    bedit_debug(DEBUG_COMMANDS);

    active_tab = bedit_window_get_active_tab(window);

    if (active_tab == NULL) {
        return;
    }

    frame = _bedit_tab_get_view_frame(active_tab);
    bedit_view_frame_popup_goto_line(frame);
}

