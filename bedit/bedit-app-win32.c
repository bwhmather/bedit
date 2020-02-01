/*
 * bedit-app-win32.c
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

#include "bedit-app-win32.h"

#define SAVE_DATADIR DATADIR
#undef DATADIR
#include <conio.h>
#include <io.h>
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#include <windows.h>
#define DATADIR SAVE_DATADIR
#undef SAVE_DATADIR

struct _BeditAppWin32 {
    BeditApp parent_instance;
};

G_DEFINE_TYPE(BeditAppWin32, bedit_app_win32, BEDIT_TYPE_APP)

static void bedit_app_win32_finalize(GObject *object) {
    G_OBJECT_CLASS(bedit_app_win32_parent_class)->finalize(object);
}

static gchar *bedit_app_win32_help_link_id_impl(
    BeditApp *app, const gchar *name, const gchar *link_id) {
    if (link_id) {
        return g_strdup_printf(
            "http://library.gnome.org/users/bedit/stable/%s", link_id);
    } else {
        return g_strdup("http://library.gnome.org/users/bedit/stable/");
    }
}

static void setup_path(void) {
    gchar *path;
    gchar *installdir;
    gchar *bin;

    installdir = g_win32_get_package_installation_directory_of_module(NULL);

    bin = g_build_filename(installdir, "bin", NULL);
    g_free(installdir);

    /* Set PATH to include the bedit executable's folder */
    path = g_build_path(";", bin, g_getenv("PATH"), NULL);
    g_free(bin);

    if (!g_setenv("PATH", path, TRUE)) {
        g_warning("Could not set PATH for bedit");
    }

    g_free(path);
}

static void prep_console(void) {
    /* If we open bedit from a console get the stdout printing */
    if (fileno(stdout) != -1 && _get_osfhandle(fileno(stdout)) != -1) {
        /* stdout is fine, presumably redirected to a file or pipe */
    } else {
        typedef BOOL (*WINAPI AttachConsole_t)(DWORD);

        AttachConsole_t p_AttachConsole = (AttachConsole_t)GetProcAddress(
            GetModuleHandle("kernel32.dll"), "AttachConsole");

        if (p_AttachConsole != NULL && p_AttachConsole(ATTACH_PARENT_PROCESS)) {
            freopen("CONOUT$", "w", stdout);
            dup2(fileno(stdout), 1);
            freopen("CONOUT$", "w", stderr);
            dup2(fileno(stderr), 2);
        }
    }
}

static void bedit_app_win32_startup(GApplication *application) {
    G_APPLICATION_CLASS(bedit_app_win32_parent_class)->startup(application);

    setup_path();
    prep_console();
}

static void bedit_app_win32_class_init(BeditAppWin32Class *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    GApplicationClass *gapp_class = G_APPLICATION_CLASS(klass);
    BeditAppClass *app_class = BEDIT_APP_CLASS(klass);

    object_class->finalize = bedit_app_win32_finalize;

    gapp_class->startup = bedit_app_win32_startup;

    app_class->help_link_id = bedit_app_win32_help_link_id_impl;
}

static void bedit_app_win32_init(BeditAppWin32 *self) {}

