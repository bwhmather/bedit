/*
 * bedit-commands-view.c
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * Based on gedit-commands-view.c from Gedit.
 *
 * Copyright (C) 1998-1999 - Alex Roberts, Evan Lawrence
 * Copyright (C) 2000-2001 - Chema Celorio
 * Copyright (C) 2000-2006 - Paolo Maggi
 * Copyright (C) 2005-2015 - Paolo Borelli
 * Copyright (C) 2006-2014 - Steve Frécinaux
 * Copyright (C) 2009-2014 - Ignacio Casal Quinteiro
 * Copyright (C) 2010 - Garrett Regier
 * Copyright (C) 2010-2014 - Jesse van den Kieboom
 * Copyright (C) 2013-2019 - Sébastien Wilmet
 * Copyright (C) 2014 - Robert Roth, Sebastien Lafargue
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

#include <gtk/gtk.h>

#include "bedit-debug.h"
#include "bedit-highlight-mode-dialog.h"
#include "bedit-highlight-mode-selector.h"
#include "bedit-window.h"

void _bedit_cmd_view_focus_active(
    GSimpleAction *action, GVariant *state, gpointer user_data
) {
    BeditView *active_view;
    BeditWindow *window = BEDIT_WINDOW(user_data);

    bedit_debug(DEBUG_COMMANDS);

    active_view = bedit_window_get_active_view(window);

    if (active_view) {
        gtk_widget_grab_focus(GTK_WIDGET(active_view));
    }
}

void _bedit_cmd_view_toggle_fullscreen_mode(
    GSimpleAction *action, GVariant *state, gpointer user_data
) {
    BeditWindow *window = BEDIT_WINDOW(user_data);

    bedit_debug(DEBUG_COMMANDS);

    if (g_variant_get_boolean(state)) {
        _bedit_window_fullscreen(window);
    } else {
        _bedit_window_unfullscreen(window);
    }
}

void _bedit_cmd_view_leave_fullscreen_mode(
    GSimpleAction *action, GVariant *parameter, gpointer user_data
) {
    _bedit_window_unfullscreen(BEDIT_WINDOW(user_data));
}

static void on_language_selected(
    BeditHighlightModeSelector *sel, GtkSourceLanguage *language,
    BeditWindow *window
) {
    BeditDocument *doc;

    doc = bedit_window_get_active_document(window);
    if (doc) {
        bedit_document_set_language(doc, language);
    }
}

void _bedit_cmd_view_highlight_mode(
    GSimpleAction *action, GVariant *parameter, gpointer user_data
) {
    GtkWindow *window = GTK_WINDOW(user_data);
    GtkWidget *dlg;
    BeditHighlightModeSelector *sel;
    BeditDocument *doc;

    dlg = bedit_highlight_mode_dialog_new(window);
    sel = bedit_highlight_mode_dialog_get_selector(
        BEDIT_HIGHLIGHT_MODE_DIALOG(dlg)
    );

    doc = bedit_window_get_active_document(BEDIT_WINDOW(window));
    if (doc) {
        bedit_highlight_mode_selector_select_language(
            sel, bedit_document_get_language(doc)
        );
    }

    g_signal_connect(
        sel, "language-selected",
        G_CALLBACK(on_language_selected), window
    );

    gtk_widget_show(GTK_WIDGET(dlg));
}

