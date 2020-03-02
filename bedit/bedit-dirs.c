/*
 * bedit-dirs.c
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * Based on gedit-dirs.c from Gedit.
 *
 * Copyright (C) 2008-2012 - Paolo Borelli
 * Copyright (C) 2008-2010 - Ignacio Casal Quinteiro
 * Copyright (C) 2009-2014 - Jesse van den Kieboom
 * Copyright (C) 2010 - Garrett Regier, Steve Frécinaux
 * Copyright (C) 2012 - Daniel Trebbien
 * Copyright (C) 2015-2019 - Sébastien Wilmet
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

#include "config.h"

#include "bedit-dirs.h"

#ifdef OS_OSX
#include <gtkosxapplication.h>
#endif

static gchar *user_config_dir = NULL;
static gchar *user_data_dir = NULL;
static gchar *user_styles_dir = NULL;
static gchar *user_plugins_dir = NULL;
static gchar *bedit_locale_dir = NULL;
static gchar *bedit_lib_dir = NULL;
static gchar *bedit_plugins_dir = NULL;
static gchar *bedit_plugins_data_dir = NULL;

void bedit_dirs_init(void) {
#ifdef G_OS_WIN32
    gchar *win32_dir;

    win32_dir = g_win32_get_package_installation_directory_of_module(NULL);

    bedit_locale_dir = g_build_filename(
        win32_dir, "share", "locale", NULL
    );
    bedit_lib_dir = g_build_filename(
        win32_dir, "lib", "bedit", NULL
    );
    bedit_plugins_data_dir = g_build_filename(
        win32_dir, "share", "bedit", "plugins", NULL
    );

    g_free(win32_dir);
#endif /* G_OS_WIN32 */

#ifdef OS_OSX
    if (gtkosx_application_get_bundle_id() != NULL) {
        const gchar *bundle_resource_dir =
            gtkosx_application_get_resource_path();

        bedit_locale_dir = g_build_filename(
            bundle_resource_dir, "share", "locale", NULL
        );
        bedit_lib_dir = g_build_filename(
            bundle_resource_dir, "lib", "bedit", NULL
        );
        bedit_plugins_data_dir = g_build_filename(
            bundle_resource_dir, "share", "bedit", "plugins", NULL
        );
    }
#endif /* OS_OSX */

    if (bedit_locale_dir == NULL) {
        bedit_locale_dir = g_build_filename(
            DATADIR, "locale", NULL
        );
        bedit_lib_dir = g_build_filename(
            LIBDIR, "bedit", NULL
        );
        bedit_plugins_data_dir = g_build_filename(
            DATADIR, "bedit", "plugins", NULL
        );
    }

    user_config_dir = g_build_filename(g_get_user_config_dir(), "bedit", NULL);
    user_data_dir = g_build_filename(g_get_user_data_dir(), "bedit", NULL);
    user_styles_dir = g_build_filename(user_data_dir, "styles", NULL);
    user_plugins_dir = g_build_filename(user_data_dir, "plugins", NULL);
    bedit_plugins_dir = g_build_filename(bedit_lib_dir, "plugins", NULL);
}

void bedit_dirs_shutdown(void) {
    g_free(user_config_dir);
    g_free(user_data_dir);
    g_free(user_styles_dir);
    g_free(user_plugins_dir);
    g_free(bedit_locale_dir);
    g_free(bedit_lib_dir);
    g_free(bedit_plugins_dir);
    g_free(bedit_plugins_data_dir);
}

const gchar *bedit_dirs_get_user_config_dir(void) {
    return user_config_dir;
}

const gchar *bedit_dirs_get_user_data_dir(void) {
    return user_data_dir;
}

const gchar *bedit_dirs_get_user_styles_dir(void) {
    return user_styles_dir;
}

const gchar *bedit_dirs_get_user_plugins_dir(void) {
    return user_plugins_dir;
}

const gchar *bedit_dirs_get_bedit_locale_dir(void) {
    return bedit_locale_dir;
}

const gchar *bedit_dirs_get_bedit_lib_dir(void) {
    return bedit_lib_dir;
}

const gchar *bedit_dirs_get_bedit_plugins_dir(void) {
    return bedit_plugins_dir;
}

const gchar *bedit_dirs_get_bedit_plugins_data_dir(void) {
    return bedit_plugins_data_dir;
}

