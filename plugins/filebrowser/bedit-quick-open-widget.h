/*
 * bedit-quick-open.h
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
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

#ifndef BEDIT_QUICK_OPEN_WIDGET_H
#define BEDIT_QUICK_OPEN_WIDGET_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define BEDIT_TYPE_QUICK_OPEN_WIDGET (bedit_quick_open_widget_get_type())

G_DECLARE_FINAL_TYPE(
    BeditQuickOpenWidget, bedit_quick_open_widget,
    BEDIT, QUICK_OPEN_WIDGET,
    GtkGrid
)

GType bedit_quick_open_widget_get_type(void) G_GNUC_CONST;

GtkWidget *bedit_quick_open_widget_new(void);

void bedit_quick_open_widget_set_virtual_root(
    BeditQuickOpenWidget *obj, GFile *virtual_root
);
GFile *bedit_quick_open_widget_get_virtual_root(BeditQuickOpenWidget *obj);

void _bedit_quick_open_widget_register_type(GTypeModule *type_module);

G_END_DECLS

#endif /* BEDIT_QUICK_OPEN_WIDGET_H */

