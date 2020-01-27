/*
 * bedit-file-browser-messages.h - Bedit plugin providing easy file access
 * from the sidepanel
 *
 * Copyright (C) 2008 - Jesse van den Kieboom <jesse@icecrew.nl>
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

#ifndef BEDIT_FILE_BROWSER_MESSAGES_H
#define BEDIT_FILE_BROWSER_MESSAGES_H

#include <bedit/bedit-message-bus.h>
#include <bedit/bedit-window.h>
#include "bedit-file-browser-widget.h"

void bedit_file_browser_messages_register(
    BeditWindow *window, BeditFileBrowserWidget *widget);
void bedit_file_browser_messages_unregister(BeditWindow *window);

#endif /* BEDIT_FILE_BROWSER_MESSAGES_H */

