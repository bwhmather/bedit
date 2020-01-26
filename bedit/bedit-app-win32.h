/*
 * bedit-app-win32.h
 * This file is part of bedit
 *
 * Copyright (C) 2010 - Jesse van den Kieboom
 *
 * bedit is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * bedit is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with bedit; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#ifndef GEDIT_APP_WIN32_H
#define GEDIT_APP_WIN32_H

#include "bedit-app.h"

G_BEGIN_DECLS

#define GEDIT_TYPE_APP_WIN32 (bedit_app_win32_get_type())

G_DECLARE_FINAL_TYPE(BeditAppWin32, bedit_app_win32, GEDIT, APP_WIN32, BeditApp)

G_END_DECLS

#endif /* GEDIT_APP_WIN32_H */

