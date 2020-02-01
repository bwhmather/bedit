/*
 * bedit-menu-stack-switcher.h
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * Based on gedit-menu-stack-switcher.h from Gedit.
 *
 * Copyright (C) 2014 - Steve Frécinaux
 * Copyright (C) 2015 - Paolo Borelli
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

#ifndef BEDIT_MENU_STACK_SWITCHER_H
#define BEDIT_MENU_STACK_SWITCHER_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define BEDIT_TYPE_MENU_STACK_SWITCHER (bedit_menu_stack_switcher_get_type())

G_DECLARE_FINAL_TYPE(
    BeditMenuStackSwitcher, bedit_menu_stack_switcher, BEDIT,
    MENU_STACK_SWITCHER, GtkMenuButton)

GtkWidget *bedit_menu_stack_switcher_new(void);

void bedit_menu_stack_switcher_set_stack(
    BeditMenuStackSwitcher *switcher, GtkStack *stack);

GtkStack *bedit_menu_stack_switcher_get_stack(BeditMenuStackSwitcher *switcher);

G_END_DECLS

#endif /* BEDIT_MENU_STACK_SWITCHER_H  */

