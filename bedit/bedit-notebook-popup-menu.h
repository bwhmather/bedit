/*
 * bedit-notebook-popup-menu.h
 * This file is part of bedit
 *
 * Copyright (C) 2011 - Ignacio Casal Quinteiro
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

#ifndef BEDIT_NOTEBOOK_POPUP_MENU_H
#define BEDIT_NOTEBOOK_POPUP_MENU_H

#include <gtk/gtk.h>
#include "bedit-tab.h"
#include "bedit-window.h"

G_BEGIN_DECLS

#define BEDIT_TYPE_NOTEBOOK_POPUP_MENU (bedit_notebook_popup_menu_get_type())
G_DECLARE_FINAL_TYPE(
    BeditNotebookPopupMenu, bedit_notebook_popup_menu, BEDIT,
    NOTEBOOK_POPUP_MENU, GtkMenu)

GtkWidget *bedit_notebook_popup_menu_new(BeditWindow *window, BeditTab *tab);

G_END_DECLS

#endif /* BEDIT_NOTEBOOK_POPUP_MENU_H */
