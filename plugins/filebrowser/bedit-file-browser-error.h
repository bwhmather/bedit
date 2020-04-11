/*
 * bedit-file-browser-error.c
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * Based on gedit-file-browser-error.c from Gedit.
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

#ifndef BEDIT_FILE_BROWSER_ERROR_H
#define BEDIT_FILE_BROWSER_ERROR_H

G_BEGIN_DECLS

typedef enum {
    BEDIT_FILE_BROWSER_ERROR_NONE,
    BEDIT_FILE_BROWSER_ERROR_RENAME,
    BEDIT_FILE_BROWSER_ERROR_DELETE,
    BEDIT_FILE_BROWSER_ERROR_NEW_FILE,
    BEDIT_FILE_BROWSER_ERROR_NEW_DIRECTORY,
    BEDIT_FILE_BROWSER_ERROR_OPEN_DIRECTORY,
    BEDIT_FILE_BROWSER_ERROR_SET_ROOT,
    BEDIT_FILE_BROWSER_ERROR_LOAD_DIRECTORY,
    BEDIT_FILE_BROWSER_ERROR_NUM
} BeditFileBrowserError;

G_END_DECLS

#endif /* BEDIT_FILE_BROWSER_ERROR_H */

