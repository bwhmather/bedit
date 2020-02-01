/*
 * bedit-preferences-dialog.h
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * Based on gedit-preferences-dialog.h from Gedit.
 *
 * Copyright (C) 2001-2005 - Paolo Maggi
 * Copyright (C) 2012-2014 - Ignacio Casal Quinteiro
 * Copyright (C) 2013-2016 - Sébastien Wilmet
 * Copyright (C) 2014-2015 - Paolo Borelli
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

#ifndef BEDIT_PREFERENCES_DIALOG_H
#define BEDIT_PREFERENCES_DIALOG_H

#include "bedit-window.h"

G_BEGIN_DECLS

void bedit_show_preferences_dialog(BeditWindow *parent);

G_END_DECLS

#endif /* BEDIT_PREFERENCES_DIALOG_H */

