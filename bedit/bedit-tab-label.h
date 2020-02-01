/*
 * bedit-tab-label.h
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * Based on gedit-tab-label.h from Gedit.
 *
 * Copyright (C) 2010 - Jesse van den Kieboom, Steve Frécinaux
 * Copyright (C) 2010-2011 - Garrett Regier
 * Copyright (C) 2010-2015 - Paolo Borelli
 * Copyright (C) 2013-2016 - Sébastien Wilmet
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

#ifndef BEDIT_TAB_LABEL_H
#define BEDIT_TAB_LABEL_H

#include <bedit/bedit-tab.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define BEDIT_TYPE_TAB_LABEL (bedit_tab_label_get_type())

G_DECLARE_FINAL_TYPE(BeditTabLabel, bedit_tab_label, BEDIT, TAB_LABEL, GtkBox)

GtkWidget *bedit_tab_label_new(BeditTab *tab);

BeditTab *bedit_tab_label_get_tab(BeditTabLabel *tab_label);

G_END_DECLS

#endif /* BEDIT_TAB_LABEL_H */

