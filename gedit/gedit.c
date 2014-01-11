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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the gedit Team, 2005. See the AUTHORS file for a
 * list of people on the gedit Team.
 * See the ChangeLog files for a list of changes.
 *
 * $Id$
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
	type = GEDIT_TYPE_APP_WIN32;
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

	return status;
}

/* ex:set ts=8 noet: */
