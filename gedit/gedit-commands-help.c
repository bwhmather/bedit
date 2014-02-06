/*
 * gedit-help-commands.c
 * This file is part of gedit
 *
 * Copyright (C) 1998, 1999 Alex Roberts, Evan Lawrence
 * Copyright (C) 2000, 2001 Chema Celorio, Paolo Maggi
 * Copyright (C) 2002-2005 Paolo Maggi
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

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "gedit-commands.h"
#include "gedit-debug.h"
#include "gedit-app.h"
#include "gedit-dirs.h"

void
_gedit_cmd_help_contents (GtkAction   *action,
			  GeditWindow *window)
{
	gedit_debug (DEBUG_COMMANDS);

	gedit_app_show_help (GEDIT_APP (g_application_get_default ()),
	                     GTK_WINDOW (window),
	                     NULL,
	                     NULL);
}

void
_gedit_cmd_help_about (GtkAction   *action,
		       GeditWindow *window)
{
	static const gchar * const authors[] = {
		"Alex Roberts",
		"Chema Celorio",
		"Evan Lawrence",
		"Federico Mena Quintero <federico@novell.com>",
		"Garrett Regier <garrettregier@gmail.com>",
		"Ignacio Casal Quinteiro <icq@gnome.org>",
		"James Willcox <jwillcox@gnome.org>",
		"Jesse van den Kieboom <jessevdk@gnome.org>",
		"Paolo Borelli <pborelli@katamail.com>",
		"Paolo Maggi <paolo@gnome.org>",
		"S\303\251bastien Wilmet <swilmet@gnome.org>",
		"Steve Fr\303\251cinaux <steve@istique.net>",
		NULL
	};

	static const gchar * const documenters[] = {
		"Jim Campbell <jwcampbell@gmail.com>",
		"Daniel Neel <dneelyep@gmail.com>",
		"Sun GNOME Documentation Team <gdocteam@sun.com>",
		"Eric Baudais <baudais@okstate.edu>",
		NULL
	};

	static const gchar copyright[] = "Copyright \xc2\xa9 1998-2014 - the gedit team";

	static const gchar comments[] = \
		N_("gedit is a small and lightweight text editor for the GNOME Desktop");

	GdkPixbuf *logo;
	const gchar *data_dir;
	gchar *logo_file;

	gedit_debug (DEBUG_COMMANDS);

	data_dir = gedit_dirs_get_gedit_data_dir ();
	logo_file = g_build_filename (data_dir,
				      "logo",
				      "gedit-logo.png",
				      NULL);
	logo = gdk_pixbuf_new_from_file (logo_file, NULL);
	g_free (logo_file);

	gtk_show_about_dialog (GTK_WINDOW (window),
			       "program-name", "gedit",
			       "authors", authors,
			       "comments", _(comments),
			       "copyright", copyright,
			       "license-type", GTK_LICENSE_GPL_2_0,
			       "documenters", documenters,
			       "logo", logo,
			       "translator-credits", _("translator-credits"),
			       "version", VERSION,
			       "website", "http://www.gedit.org",
			       "website-label", "www.gedit.org",
			       NULL);

	if (logo)
		g_object_unref (logo);
}

/* ex:set ts=8 noet: */
