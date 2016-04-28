/*
 * gedit-file-browser-messages.h - Gedit plugin providing easy file access
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

#ifndef GEDIT_FILE_BROWSER_MESSAGES_H
#define GEDIT_FILE_BROWSER_MESSAGES_H

#include <gedit/gedit-window.h>
#include <gedit/gedit-message-bus.h>
#include "gedit-file-browser-widget.h"

void gedit_file_browser_messages_register   (GeditWindow            *window,
					     GeditFileBrowserWidget *widget);
void gedit_file_browser_messages_unregister (GeditWindow            *window);

#endif /* GEDIT_FILE_BROWSER_MESSAGES_H */
/* ex:set ts=8 noet: */
