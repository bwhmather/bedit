/*
 * bedit-progress-info-bar.h
 * This file is part of bedit
 *
 * Copyright (C) 2005 - Paolo Maggi
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

#ifndef GEDIT_PROGRESS_INFO_BAR_H
#define GEDIT_PROGRESS_INFO_BAR_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GEDIT_TYPE_PROGRESS_INFO_BAR (bedit_progress_info_bar_get_type ())
G_DECLARE_FINAL_TYPE (BeditProgressInfoBar, bedit_progress_info_bar, GEDIT, PROGRESS_INFO_BAR, GtkInfoBar)

GtkWidget	*bedit_progress_info_bar_new			(const gchar          *icon_name,
								 const gchar          *markup,
								 gboolean              has_cancel);

void		 bedit_progress_info_bar_set_icon_name		(BeditProgressInfoBar *bar,
								 const gchar          *icon_name);

void		 bedit_progress_info_bar_set_markup		(BeditProgressInfoBar *bar,
								 const gchar          *markup);

void		 bedit_progress_info_bar_set_text		(BeditProgressInfoBar *bar,
								 const gchar          *text);

void		 bedit_progress_info_bar_set_fraction		(BeditProgressInfoBar *bar,
								 gdouble               fraction);

void		 bedit_progress_info_bar_pulse			(BeditProgressInfoBar *bar);

G_END_DECLS

#endif  /* GEDIT_PROGRESS_INFO_BAR_H  */

/* ex:set ts=8 noet: */
