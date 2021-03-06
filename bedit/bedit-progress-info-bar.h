/*
 * bedit-progress-info-bar.h
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * Based on gedit-progress-info-bar.h from Gedit.
 *
 * Copyright (C) 2005 - Paolo Maggi
 * Copyright (C) 2010 - Garrett Regier, Steve Frécinaux
 * Copyright (C) 2013-2016 - Sébastien Wilmet
 * Copyright (C) 2014-2015 - Paolo Borelli
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

#ifndef BEDIT_PROGRESS_INFO_BAR_H
#define BEDIT_PROGRESS_INFO_BAR_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define BEDIT_TYPE_PROGRESS_INFO_BAR (bedit_progress_info_bar_get_type())
G_DECLARE_FINAL_TYPE(
    BeditProgressInfoBar, bedit_progress_info_bar,
    BEDIT, PROGRESS_INFO_BAR,
    GtkInfoBar
)

GtkWidget *bedit_progress_info_bar_new(
    const gchar *icon_name, const gchar *markup, gboolean has_cancel
);

void bedit_progress_info_bar_set_icon_name(
    BeditProgressInfoBar *bar, const gchar *icon_name
);

void bedit_progress_info_bar_set_markup(
    BeditProgressInfoBar *bar, const gchar *markup
);

void bedit_progress_info_bar_set_text(
    BeditProgressInfoBar *bar, const gchar *text
);

void bedit_progress_info_bar_set_fraction(
    BeditProgressInfoBar *bar, gdouble fraction
);

void bedit_progress_info_bar_pulse(BeditProgressInfoBar *bar);

G_END_DECLS

#endif /* BEDIT_PROGRESS_INFO_BAR_H  */

