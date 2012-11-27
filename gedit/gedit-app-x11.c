/*
 * gedit-app-x11.h
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

#include "gedit-app-x11.h"

#define GEDIT_APP_X11_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE((object), GEDIT_TYPE_APP_X11, GeditAppX11Private))

G_DEFINE_TYPE (GeditAppX11, gedit_app_x11, GEDIT_TYPE_APP)

static void
gedit_app_x11_finalize (GObject *object)
{
	G_OBJECT_CLASS (gedit_app_x11_parent_class)->finalize (object);
}

/* This should go in GtkApplication at some point... */

#include <X11/Xatom.h>
#include <X11/Xlib.h>

static void
gedit_app_add_platform_data (GApplication    *app,
                             GVariantBuilder *builder)
{
  G_APPLICATION_CLASS (gedit_app_x11_parent_class)->add_platform_data (app, builder);

  /* In the event that we are launched from a terminal we will probably
   * see that we have DISPLAY set, but not DESKTOP_STARTUP_ID.
   *
   * If we are the second instance, we want to bring the window of the
   * existing instance forward.  Unfortunately, without
   * DESKTOP_STARTUP_ID being set, we have no timestamp on which to base
   * the claim that we are the representative of the user's last
   * interaction.
   *
   * We need to fake it by finding out the current timestamp of the
   * server and sending that along as if we had DESKTOP_STARTUP_ID given
   * in the first place.
   *
   * We want to do this without initialising all of Gtk+, so we can just
   * talk directly to the X server to find out...
   *
   * In order to get the current timestamp, we need to see a
   * timestamp-tagged event sent by the server.  Property change
   * notifications are probably the easiest to generate.  We create our
   * own window so that we don't spam other people who may be watching
   * for property notifies on the root window...
   *
   * We could use any property at all, but we may as well use the
   * "_NET_WM_USER_TIME_WINDOW" one since that's what we're doing
   * here...
   */
  if (!g_getenv ("DESKTOP_STARTUP_ID") && g_getenv ("DISPLAY"))
    {
      gchar *startup_id;
      Display *display;
      Window window;
      XEvent event;
      Atom atom;

      display = XOpenDisplay (0);
      window = XCreateWindow (display, DefaultRootWindow (display), 0, 0, 1, 1, 0, 0, InputOnly, 0, 0, NULL);
      XSelectInput (display, window, PropertyChangeMask);
      atom = XInternAtom (display, "_NET_WM_USER_TIME_WINDOW", False);
      XChangeProperty (display, window, atom, XA_WINDOW, 32, PropModeReplace, (void *) &window, 1);
      XNextEvent (display, &event);
      g_assert (event.type == PropertyNotify);
      XCloseDisplay (display);

      startup_id = g_strdup_printf ("_TIME%u", (guint) ((XPropertyEvent *) &event)->time);
      g_variant_builder_add (builder, "{sv}", "desktop-startup-id", g_variant_new_string (startup_id));
      g_free (startup_id);
    }
}

static void
gedit_app_x11_class_init (GeditAppX11Class *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GApplicationClass *app_class = G_APPLICATION_CLASS (klass);

	object_class->finalize = gedit_app_x11_finalize;
	app_class->add_platform_data = gedit_app_add_platform_data;
}

static void
gedit_app_x11_init (GeditAppX11 *self)
{
}

/* ex:set ts=8 noet: */
