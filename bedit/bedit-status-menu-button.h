/*
 * bedit-status-menu-button.h
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * Based on gedit-status-menu-button.h from Gedit.
 *
 * Copyright (C) 2008 - Jesse van den Kieboom
 * Copyright (C) 2013-2015 - Paolo Borelli
 * Copyright (C) 2016 - Sébastien Wilmet
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

#ifndef BEDIT_STATUS_MENU_BUTTON_H
#define BEDIT_STATUS_MENU_BUTTON_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define BEDIT_TYPE_STATUS_MENU_BUTTON (bedit_status_menu_button_get_type())

G_DECLARE_FINAL_TYPE(
    BeditStatusMenuButton, bedit_status_menu_button,
    BEDIT, STATUS_MENU_BUTTON,
    GtkMenuButton
)

GtkWidget *bedit_status_menu_button_new(void);

void bedit_status_menu_button_set_label(
    BeditStatusMenuButton *button, const gchar *label
);

const gchar *bedit_status_menu_button_get_label(BeditStatusMenuButton *button);

G_END_DECLS

#endif /* BEDIT_STATUS_MENU_BUTTON_H */

