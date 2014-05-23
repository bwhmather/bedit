/*
 * gedit.c
 * This file is part of gedit
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <glib.h>
#include "gedit-app.h"
#ifdef OS_OSX
#include "gedit-app-osx.h"
#else
#ifdef G_OS_WIN32
#include "gedit-app-win32.h"
#else
#include "gedit-app-x11.h"
#endif
#endif

#ifdef G_OS_WIN32
#include <gmodule.h>
static GModule *libgedit_dll = NULL;

/* This code must live in gedit.exe, not in libgedit.dll, since the whole
 * point is to find and load libgedit.dll.
 */
static void
gedit_w32_load_private_dll (GType *type)
{
	gchar *dllpath;
	gchar *prefix;

	prefix = g_win32_get_package_installation_directory_of_module (NULL);

	if (prefix != NULL)
	{
		/* Instead of g_module_open () it may be possible to do any of the
		 * following:
		 * A) Change PATH to "${dllpath}/lib/gedit;$PATH"
		 * B) Call SetDllDirectory ("${dllpath}/lib/gedit")
		 * C) Call AddDllDirectory ("${dllpath}/lib/gedit")
		 * But since we only have one library, and its name is known, may as well
		 * use gmodule.
		 */
		dllpath = g_build_filename (prefix, "lib", "gedit", "libgedit.dll", NULL);
		g_free (prefix);
		libgedit_dll = g_module_open (dllpath, 0);
		g_free (dllpath);
	}

	if (libgedit_dll == NULL)
	{
		libgedit_dll = g_module_open ("libgedit.dll", 0);
	}

	if (libgedit_dll != NULL)
	{
		/* Now that we did everything we could to make the DLL available, we can
		 * implicitly call the *_get_type() function from it.
		 */
		*type = GEDIT_TYPE_APP_WIN32;
	}
}

static void
gedit_w32_unload_private_dll (void)
{
	if (libgedit_dll)
	{
		g_module_close (libgedit_dll);
		libgedit_dll = NULL;
	}
}
#endif

int
main (int argc, char *argv[])
{
	GType type;
	GeditApp *app;
	gint status;

#ifdef OS_OSX
	type = GEDIT_TYPE_APP_OSX;
#else
#ifdef G_OS_WIN32
	gedit_w32_load_private_dll (&type);
#else
	type = GEDIT_TYPE_APP_X11;
#endif
#endif

	app = g_object_new (type,
	                    "application-id", "org.gnome.gedit",
	                    "flags", G_APPLICATION_HANDLES_COMMAND_LINE,
	                    NULL);

	status = g_application_run (G_APPLICATION (app), argc, argv);

	g_object_unref (app);

#ifdef G_OS_WIN32
	gedit_w32_unload_private_dll ();
#endif

	return status;
}

/* ex:set ts=8 noet: */
