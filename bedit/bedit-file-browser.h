/*
 * bedit-file-browser.h
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


#ifndef BEDIT_FILE_BROWSER_H
#define BEDIT_FILE_BROWSER_H

#include <glib-object.h>

#include "bedit-window.h"

G_BEGIN_DECLS

#define BEDIT_TYPE_FILE_BROWSER (bedit_file_browser_get_type())

G_DECLARE_FINAL_TYPE(
    BeditFileBrowser,
    bedit_file_browser,
    BEDIT, FILE_BROWSER,
    GObject
)

BeditFileBrowser *bedit_file_browser_new(BeditWindow *window);

// TODO left over from migration from plugin.
void bedit_file_browser_activate(BeditFileBrowser *plugin);
void bedit_file_browser_deactivate(BeditFileBrowser *plugin);
void bedit_file_browser_update_state(BeditFileBrowser *plugin);

G_END_DECLS

#endif /* BEDIT_FILE_BROWSER_WINDOW_ACTIVATABLE_H */
