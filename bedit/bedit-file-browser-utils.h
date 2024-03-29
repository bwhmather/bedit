/*
 * bedit-file-browser-utils.h
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * Based on gedit-file-browser-utils.h from Gedit.
 *
 * Copyright (C) 2006 - Jesse van den Kieboom <jesse@icecrew.nl>
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

#ifndef BEDIT_FILE_BROWSER_UTILS_H
#define BEDIT_FILE_BROWSER_UTILS_H

#include <gio/gio.h>

#include "bedit-window.h"

gchar *bedit_file_browser_utils_name_from_themed_icon(GIcon *icon);
gchar *bedit_file_browser_utils_icon_name_from_file(GFile *file);
gchar *bedit_file_browser_utils_file_basename(GFile *file);

gboolean bedit_file_browser_utils_confirmation_dialog(
    BeditWindow *window, GtkMessageType type, gchar const *message,
    gchar const *secondary, gchar const *button_label
);

#endif /* BEDIT_FILE_BROWSER_UTILS_H */

