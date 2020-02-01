/*
 * bedit-app-osx.h
 * This file is part of bedit
 *
 * Copyright (C) 2010 - Jesse van den Kieboom
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

#ifndef BEDIT_APP_OSX_H
#define BEDIT_APP_OSX_H

#include "bedit-app.h"

G_BEGIN_DECLS

#define BEDIT_TYPE_APP_OSX (bedit_app_osx_get_type())

G_DECLARE_FINAL_TYPE(BeditAppOSX, bedit_app_osx, BEDIT, APP_OSX, BeditApp)

void bedit_app_osx_set_window_title(
    BeditAppOSX *app, BeditWindow *window, const gchar *title,
    BeditDocument *document);

gboolean bedit_app_osx_show_url(BeditAppOSX *app, const gchar *url);

G_END_DECLS

#endif /* BEDIT_APP_OSX_H */

