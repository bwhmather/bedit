/*
 * bedit-commands-file-print.c
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * Based on gedit-commands-file-print.c from Gedit.
 *
 * Copyright (C) 1998-1999 - Alex Roberts, Evan Lawrence
 * Copyright (C) 2000-2001 - Chema Celorio
 * Copyright (C) 2000-2006 - Paolo Maggi
 * Copyright (C) 2005-2015 - Paolo Borelli
 * Copyright (C) 2006 - Paolo Maggi
 * Copyright (C) 2010 - Garrett Regier, Jesse van den Kieboom, Steve Frécinaux
 * Copyright (C) 2013 - Ignacio Casal Quinteiro
 * Copyright (C) 2014 - Robert Roth
 * Copyright (C) 2015 - Sébastien Wilmet
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

#include "bedit-commands-private.h"
#include "bedit-commands.h"

#include "bedit-debug.h"
#include "bedit-tab-private.h"
#include "bedit-tab.h"
#include "bedit-window.h"

void _bedit_cmd_file_print(
    GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    BeditWindow *window = BEDIT_WINDOW(user_data);
    BeditTab *tab;

    bedit_debug(DEBUG_COMMANDS);

    tab = bedit_window_get_active_tab(window);

    if (tab != NULL) {
        _bedit_tab_print(tab);
    }
}

