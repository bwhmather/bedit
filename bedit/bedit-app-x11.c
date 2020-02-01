/*
 * bedit-app-x11.c
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * Based on gedit-app-x11.c from Gedit.
 *
 * Copyright (C) 2010 - Garrett Regier, Jesse van den Kieboom, Steve Frécinaux
 * Copyright (C) 2012 - Ignacio Casal Quinteiro, Ryan Lortie
 * Copyright (C) 2013 - Alexander Larsson, Sébastien Wilmet
 * Copyright (C) 2015 - Paolo Borelli
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

#include "bedit-app-x11.h"

#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif

struct _BeditAppX11 {
    BeditApp parent_instance;
};

G_DEFINE_TYPE(BeditAppX11, bedit_app_x11, BEDIT_TYPE_APP)

/* This should go in GtkApplication at some point... */

#include <X11/Xatom.h>
#include <X11/Xlib.h>

static void bedit_app_add_platform_data(
    GApplication *app, GVariantBuilder *builder) {
    G_APPLICATION_CLASS(bedit_app_x11_parent_class)
        ->add_platform_data(app, builder);

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
    if (!g_getenv("DESKTOP_STARTUP_ID") && g_getenv("DISPLAY") &&
        GDK_IS_X11_DISPLAY(gdk_display_get_default())) {
        gchar *startup_id;
        Display *display;
        Window window;
        XEvent event;
        Atom atom;

        display = XOpenDisplay(0);
        window = XCreateWindow(
            display, DefaultRootWindow(display), 0, 0, 1, 1, 0, 0, InputOnly, 0,
            0, NULL);
        XSelectInput(display, window, PropertyChangeMask);
        atom = XInternAtom(display, "_NET_WM_USER_TIME_WINDOW", False);
        XChangeProperty(
            display, window, atom, XA_WINDOW, 32, PropModeReplace,
            (void *)&window, 1);
        XNextEvent(display, &event);
        g_assert(event.type == PropertyNotify);
        XCloseDisplay(display);

        startup_id =
            g_strdup_printf("_TIME%u", (guint)((XPropertyEvent *)&event)->time);
        g_variant_builder_add(
            builder, "{sv}", "desktop-startup-id",
            g_variant_new_string(startup_id));
        g_free(startup_id);
    }
}

static void bedit_app_x11_class_init(BeditAppX11Class *klass) {
    GApplicationClass *app_class = G_APPLICATION_CLASS(klass);

    app_class->add_platform_data = bedit_app_add_platform_data;
}

static void bedit_app_x11_init(BeditAppX11 *self) {}

