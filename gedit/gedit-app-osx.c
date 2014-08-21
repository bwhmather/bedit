/*
 * gedit-app-osx.c
 * This file is part of gedit
 *
 * Copyright (C) 2010 - Jesse van den Kieboom
 *
 * gedit is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * gedit is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with gedit; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#include "gedit-app-osx.h"
#include <gedit/gedit-dirs.h>
#include <gedit/gedit-debug.h>
#include <gdk/gdkquartz.h>
#include <string.h>
#include <glib/gi18n.h>

#include "gedit-commands.h"
#include <AvailabilityMacros.h>

G_DEFINE_TYPE (GeditAppOSX, gedit_app_osx, GEDIT_TYPE_APP)

static void
gedit_app_osx_finalize (GObject *object)
{
	G_OBJECT_CLASS (gedit_app_osx_parent_class)->finalize (object);
}

gboolean
gedit_app_osx_show_url (GeditAppOSX *app,
                        const gchar *url)
{
	return [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:[NSString stringWithUTF8String:url]]];
}

static gboolean
gedit_app_osx_show_help_impl (GeditApp    *app,
                              GtkWindow   *parent,
                              const gchar *name,
                              const gchar *link_id)
{
	gboolean ret = FALSE;

	if (name == NULL || g_strcmp0 (name, "gedit.xml") == 0 || g_strcmp0 (name, "gedit") == 0)
	{
		gchar *link;

		if (link_id)
		{
			link = g_strdup_printf ("http://library.gnome.org/users/gedit/stable/%s",
						link_id);
		}
		else
		{
			link = g_strdup ("http://library.gnome.org/users/gedit/stable/");
		}

		ret = gedit_app_osx_show_url (GEDIT_APP_OSX (app), link);
		g_free (link);
	}

	return ret;
}

static void
gedit_app_osx_set_window_title_impl (GeditApp    *app,
                                     GeditWindow *window,
                                     const gchar *title)
{
	NSWindow *native;
	GeditDocument *document;
	GdkWindow *wnd;

	g_return_if_fail (GEDIT_IS_WINDOW (window));

	wnd = gtk_widget_get_window (GTK_WIDGET (window));

	if (wnd == NULL)
	{
		return;
	}

	native = gdk_quartz_window_get_nswindow (wnd);
	document = gedit_window_get_active_document (window);

	if (document)
	{
		bool ismodified;

		if (gedit_document_is_untitled (document))
		{
			[native setRepresentedURL:nil];
		}
		else
		{
			GtkSourceFile *file;
			GFile *location;
			gchar *uri;

			file = gedit_document_get_file (document);
			location = gtk_source_file_get_location (file);

			uri = g_file_get_uri (location);

			NSURL *nsurl = [NSURL URLWithString:[NSString stringWithUTF8String:uri]];

			[native setRepresentedURL:nsurl];
			g_free (uri);
		}

		ismodified = !gedit_document_is_untouched (document);
		[native setDocumentEdited:ismodified];
	}
	else
	{
		[native setRepresentedURL:nil];
		[native setDocumentEdited:false];
	}

	GEDIT_APP_CLASS (gedit_app_osx_parent_class)->set_window_title (app, window, title);
}

static void
load_accels (void)
{
	gchar *filename;

	filename = g_build_filename (gedit_dirs_get_gedit_data_dir (),
	                             "osx.accels",
	                             NULL);

	if (filename != NULL)
	{
		gedit_debug_message (DEBUG_APP, "Loading accels from %s\n", filename);

		gtk_accel_map_load (filename);
		g_free (filename);
	}
}

static void
load_keybindings (void)
{
	gchar *filename;

	filename = g_build_filename (gedit_dirs_get_gedit_data_dir (),
	                             "osx.css",
	                             NULL);

	if (filename != NULL)
	{
		GtkCssProvider *provider;
		GError *error = NULL;

		gedit_debug_message (DEBUG_APP, "Loading keybindings from %s\n", filename);

		provider = gtk_css_provider_new ();

		if (!gtk_css_provider_load_from_path (provider,
		                                      filename,
		                                      &error))
		{
			g_warning ("Failed to load osx keybindings from `%s':\n%s",
			           filename,
			           error->message);

			g_error_free (error);
		}
		else
		{
			gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
			                                           GTK_STYLE_PROVIDER (provider),
			                                           GTK_STYLE_PROVIDER_PRIORITY_SETTINGS);
		}

		g_object_unref (provider);
		g_free (filename);
	}
}

static void
gedit_app_osx_startup (GApplication *application)
{
	G_APPLICATION_CLASS (gedit_app_osx_parent_class)->startup (application);

	/* Load the osx specific accel overrides */
	load_accels ();
	load_keybindings ();
}

static gboolean
gedit_app_osx_process_window_event_impl (GeditApp    *app,
                                         GeditWindow *window,
                                         GdkEvent    *event)
{
	NSEvent *nsevent;

	/* For OS X we will propagate the event to NSApp, which handles some OS X
	 * specific keybindings and the accelerators for the menu
	 */
	nsevent = gdk_quartz_event_get_nsevent (event);
	[NSApp sendEvent:nsevent];

	/* It does not really matter what we return here since it's the last thing
	 * in the chain. Also we can't get from sendEvent whether the event was
	 * actually handled by NSApp anyway
	 */
	return TRUE;
}

static void
gedit_app_osx_class_init (GeditAppOSXClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GeditAppClass *app_class = GEDIT_APP_CLASS (klass);
	GApplicationClass *application_class = G_APPLICATION_CLASS (klass);

	object_class->finalize = gedit_app_osx_finalize;
	application_class->startup = gedit_app_osx_startup;

	app_class->show_help = gedit_app_osx_show_help_impl;
	app_class->set_window_title = gedit_app_osx_set_window_title_impl;
	app_class->process_window_event = gedit_app_osx_process_window_event_impl;
}

static void
gedit_app_osx_init (GeditAppOSX *app)
{
	/* This is required so that Cocoa is not going to parse the
	   command line arguments by itself and generate OpenFile events.
	   We already parse the command line ourselves, so this is needed
	   to prevent opening files twice, etc. */
	[[NSUserDefaults standardUserDefaults] setObject:@"NO"
	                                       forKey:@"NSTreatUnknownArgumentsAsOpen"];
}

/* ex:set ts=8 noet: */
