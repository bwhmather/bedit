/*
 * bedit-file-browser-utils.h - Bedit plugin providing easy file access
 * from the sidepanel
 *
 * Copyright (C) 2006 - Jesse van den Kieboom <jesse@icecrew.nl>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GEDIT_FILE_BROWSER_UTILS_H
#define GEDIT_FILE_BROWSER_UTILS_H

#include <bedit/bedit-window.h>
#include <gio/gio.h>

gchar           *bedit_file_browser_utils_name_from_themed_icon         (GIcon          *icon);
GdkPixbuf	*bedit_file_browser_utils_pixbuf_from_theme	        (gchar const    *name,
									 GtkIconSize     size);

GdkPixbuf	*bedit_file_browser_utils_pixbuf_from_icon	        (GIcon          *icon,
									 GtkIconSize     size);
GdkPixbuf	*bedit_file_browser_utils_pixbuf_from_file        	(GFile          *file,
									 GtkIconSize     size,
									 gboolean        use_symbolic);
gchar           *bedit_file_browser_utils_symbolic_icon_name_from_file  (GFile *file);
gchar		*bedit_file_browser_utils_file_basename		        (GFile          *file);

gboolean	 bedit_file_browser_utils_confirmation_dialog	        (BeditWindow    *window,
									 GtkMessageType  type,
									 gchar const    *message,
									 gchar const    *secondary,
									 gchar const    *button_label);

#endif /* GEDIT_FILE_BROWSER_UTILS_H */
/* ex:set ts=8 noet: */
