/*
 * bedit-searchbar.h
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

#ifndef BEDIT_SEARCHBAR_H
#define BEDIT_SEARCHBAR_H

#include <bedit/bedit-view.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define BEDIT_TYPE_SEARCHBAR (bedit_searchbar_get_type())

G_DECLARE_FINAL_TYPE(
    BeditSearchbar, bedit_searchbar,
    BEDIT, SEARCHBAR,
    GtkBin
)

GtkWidget *bedit_searchbar_new(void);

void bedit_searchbar_set_view(BeditSearchbar *searchbar, BeditView *view);

void bedit_searchbar_show_find(BeditSearchbar *searchbar);
void bedit_searchbar_show_replace(BeditSearchbar *searchbar);
void bedit_searchbar_hide(BeditSearchbar *searchbar);

void bedit_searchbar_next(BeditSearchbar *searchbar);
void bedit_searchbar_prev(BeditSearchbar *searchbar);

gboolean bedit_searchbar_get_search_active(BeditSearchbar *searchbar);
gboolean bedit_searchbar_get_replace_active(BeditSearchbar *searchbar);

G_END_DECLS

#endif

