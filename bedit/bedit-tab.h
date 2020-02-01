/*
 * bedit-tab.h
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * Based on gedit-tab.h from Gedit.
 *
 * Copyright (C) 2005-2015 - Paolo Borelli
 * Copyright (C) 2006 - Paolo Maggi
 * Copyright (C) 2007-2010 - Steve Frécinaux
 * Copyright (C) 2010 - Ignacio Casal Quinteiro, Jesse van den Kieboom
 * Copyright (C) 2010-2011 - Garrett Regier
 * Copyright (C) 2013-2019 - Sébastien Wilmet
 * Copyright (C) 2014 - Sagar Ghuge
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

#ifndef BEDIT_TAB_H
#define BEDIT_TAB_H

#include <bedit/bedit-document.h>
#include <bedit/bedit-view.h>
#include <gtksourceview/gtksource.h>

G_BEGIN_DECLS

typedef enum {
    BEDIT_TAB_STATE_NORMAL = 0,
    BEDIT_TAB_STATE_LOADING,
    BEDIT_TAB_STATE_REVERTING,
    BEDIT_TAB_STATE_SAVING,
    BEDIT_TAB_STATE_PRINTING,
    BEDIT_TAB_STATE_SHOWING_PRINT_PREVIEW,
    BEDIT_TAB_STATE_LOADING_ERROR,
    BEDIT_TAB_STATE_REVERTING_ERROR,
    BEDIT_TAB_STATE_SAVING_ERROR,
    BEDIT_TAB_STATE_GENERIC_ERROR,
    BEDIT_TAB_STATE_CLOSING,
    BEDIT_TAB_STATE_EXTERNALLY_MODIFIED_NOTIFICATION,
    BEDIT_TAB_NUM_OF_STATES /* This is not a valid state */
} BeditTabState;

#define BEDIT_TYPE_TAB (bedit_tab_get_type())

G_DECLARE_FINAL_TYPE(BeditTab, bedit_tab, BEDIT, TAB, GtkBox)

BeditView *bedit_tab_get_view(BeditTab *tab);

/* This is only an helper function */
BeditDocument *bedit_tab_get_document(BeditTab *tab);

BeditTab *bedit_tab_get_from_document(BeditDocument *doc);

BeditTabState bedit_tab_get_state(BeditTab *tab);

gboolean bedit_tab_get_auto_save_enabled(BeditTab *tab);

void bedit_tab_set_auto_save_enabled(BeditTab *tab, gboolean enable);

gint bedit_tab_get_auto_save_interval(BeditTab *tab);

void bedit_tab_set_auto_save_interval(BeditTab *tab, gint interval);

void bedit_tab_set_info_bar(BeditTab *tab, GtkWidget *info_bar);

G_END_DECLS

#endif /* BEDIT_TAB_H  */

