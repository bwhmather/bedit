/*
 * bedit.c
 * This file is part of bedit
 *
 * Copyright (C) 2005 - Paolo Maggi
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

#include "bedit-app.h"

#if defined OS_OSX
#include "bedit-app-osx.h"
#elif defined G_OS_WIN32
#include "bedit-app-win32.h"
#else
#include "bedit-app-x11.h"
#endif

#include <libintl.h>
#include <locale.h>
#include <tepl/tepl.h>

#include "bedit-debug.h"
#include "bedit-dirs.h"

#ifdef G_OS_WIN32
#include <gmodule.h>
static GModule *libbedit_dll = NULL;

/* This code must live in bedit.exe, not in libbedit.dll, since the whole
 * point is to find and load libbedit.dll.
 */
static gboolean bedit_w32_load_private_dll(void) {
    gchar *dllpath;
    gchar *prefix;

    prefix = g_win32_get_package_installation_directory_of_module(NULL);

    if (prefix != NULL) {
        /* Instead of g_module_open () it may be possible to do any of the
         * following:
         * A) Change PATH to "${dllpath}/lib/bedit;$PATH"
         * B) Call SetDllDirectory ("${dllpath}/lib/bedit")
         * C) Call AddDllDirectory ("${dllpath}/lib/bedit")
         * But since we only have one library, and its name is known, may as
         * well use gmodule.
         */
        dllpath = g_build_filename(
            prefix, "lib", "bedit", "lib" PACKAGE_STRING ".dll", NULL);
        g_free(prefix);

        libbedit_dll = g_module_open(dllpath, 0);
        if (libbedit_dll == NULL) {
            g_printerr("Failed to load '%s': %s\n", dllpath, g_module_error());
        }

        g_free(dllpath);
    }

    if (libbedit_dll == NULL) {
        libbedit_dll = g_module_open("lib" PACKAGE_STRING ".dll", 0);
        if (libbedit_dll == NULL) {
            g_printerr(
                "Failed to load 'lib" PACKAGE_STRING ".dll': %s\n",
                g_module_error());
        }
    }

    return (libbedit_dll != NULL);
}

static void bedit_w32_unload_private_dll(void) {
    if (libbedit_dll) {
        g_module_close(libbedit_dll);
        libbedit_dll = NULL;
    }
}
#endif /* G_OS_WIN32 */

static void setup_i18n(void) {
    const gchar *dir;

    setlocale(LC_ALL, "");

    dir = bedit_dirs_get_bedit_locale_dir();
    bindtextdomain(GETTEXT_PACKAGE, dir);

    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
    textdomain(GETTEXT_PACKAGE);
}

int main(int argc, char *argv[]) {
    GType type;
    BeditApp *app;
    gint status;

#if defined OS_OSX
    type = GEDIT_TYPE_APP_OSX;
#elif defined G_OS_WIN32
    if (!bedit_w32_load_private_dll()) {
        return 1;
    }

    type = GEDIT_TYPE_APP_WIN32;
#else
    type = GEDIT_TYPE_APP_X11;
#endif

    /* NOTE: we should not make any calls to the bedit api before the
     * private library is loaded */
    bedit_dirs_init();

    setup_i18n();
    tepl_init();

    app = g_object_new(
        type, "application-id", "com.bwhmather.bedit", "flags",
        G_APPLICATION_HANDLES_COMMAND_LINE | G_APPLICATION_HANDLES_OPEN, NULL);

    status = g_application_run(G_APPLICATION(app), argc, argv);

    /* Break reference cycles caused by the PeasExtensionSet
     * for BeditAppActivatable which holds a ref on the BeditApp
     */
    g_object_run_dispose(G_OBJECT(app));

    g_object_add_weak_pointer(G_OBJECT(app), (gpointer *)&app);
    g_object_unref(app);

    if (app != NULL) {
        bedit_debug_message(
            DEBUG_APP, "Leaking with %i refs", G_OBJECT(app)->ref_count);
    }

    tepl_finalize();

#ifdef G_OS_WIN32
    bedit_w32_unload_private_dll();
#endif

    return status;
}

