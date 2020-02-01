/*
 * bedit-dirs.h
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * Based on gedit-dirs.h from Gedit.
 *
 * Copyright (C) 2008-2012 - Paolo Borelli, Ignacio Casal Quinteiro
 * Copyright (C) 2010 - Garrett Regier, Jesse van den Kieboom, Steve Frécinaux
 * Copyright (C) 2013-2019 - Sébastien Wilmet
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

#ifndef BEDIT_DIRS_H
#define BEDIT_DIRS_H

#include <glib.h>

G_BEGIN_DECLS

/* This function must be called before starting bedit */
void bedit_dirs_init(void);
/* This function must be called before exiting bedit */
void bedit_dirs_shutdown(void);

const gchar *bedit_dirs_get_user_config_dir(void);

const gchar *bedit_dirs_get_user_data_dir(void);

const gchar *bedit_dirs_get_user_styles_dir(void);

const gchar *bedit_dirs_get_user_plugins_dir(void);

const gchar *bedit_dirs_get_bedit_locale_dir(void);

const gchar *bedit_dirs_get_bedit_lib_dir(void);

const gchar *bedit_dirs_get_bedit_plugins_dir(void);

const gchar *bedit_dirs_get_bedit_plugins_data_dir(void);

G_END_DECLS

#endif /* BEDIT_DIRS_H */

