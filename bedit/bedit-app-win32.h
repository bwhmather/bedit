/*
 * bedit-app-win32.h
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * Based on gedit-app-win32.h from Gedit.
 *
 * Copyright (C) 2010 - Garrett Regier, Jesse van den Kieboom, Steve Frécinaux
 * Copyright (C) 2015 - Paolo Borelli
 * Copyright (C) 2016 - Sébastien Wilmet
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

#ifndef BEDIT_APP_WIN32_H
#define BEDIT_APP_WIN32_H

#include "bedit-app.h"

G_BEGIN_DECLS

#define BEDIT_TYPE_APP_WIN32 (bedit_app_win32_get_type())

G_DECLARE_FINAL_TYPE(BeditAppWin32, bedit_app_win32, BEDIT, APP_WIN32, BeditApp)

G_END_DECLS

#endif /* BEDIT_APP_WIN32_H */

