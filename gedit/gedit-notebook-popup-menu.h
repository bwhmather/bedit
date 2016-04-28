/*
 * gedit-notebook-popup-menu.h
 * This file is part of gedit
 *
 * Copyright (C) 2011 - Ignacio Casal Quinteiro
 *
 * gedit is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * gedit is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with gedit. If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef GEDIT_NOTEBOOK_POPUP_MENU_H
#define GEDIT_NOTEBOOK_POPUP_MENU_H

#include <gtk/gtk.h>
#include "gedit-window.h"
#include "gedit-tab.h"

G_BEGIN_DECLS

#define GEDIT_TYPE_NOTEBOOK_POPUP_MENU			(gedit_notebook_popup_menu_get_type ())
G_DECLARE_FINAL_TYPE (GeditNotebookPopupMenu, gedit_notebook_popup_menu, GEDIT, NOTEBOOK_POPUP_MENU, GtkMenu)

GtkWidget           *gedit_notebook_popup_menu_new          (GeditWindow *window,
                                                             GeditTab    *tab);

G_END_DECLS

#endif /* GEDIT_NOTEBOOK_POPUP_MENU_H */
