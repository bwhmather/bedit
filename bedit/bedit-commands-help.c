/*
 * bedit-commands-help.c
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * Based on gedit-commands-help.c from Gedit.
 *
 * Copyright (C) 1998-1999 - Alex Roberts, Evan Lawrence
 * Copyright (C) 2000-2001 - Chema Celorio
 * Copyright (C) 2000-2006 - Paolo Maggi
 * Copyright (C) 2005-2015 - Paolo Borelli
 * Copyright (C) 2006-2010 - Steve Frécinaux
 * Copyright (C) 2010 - Jesse van den Kieboom
 * Copyright (C) 2010-2011 - Ignacio Casal Quinteiro
 * Copyright (C) 2010-2013 - Garrett Regier
 * Copyright (C) 2011 - Jim Campbell
 * Copyright (C) 2014 - Asheesh Laroia, Robert Roth, Sebastien Lafargue
 * Copyright (C) 2014-2019 - Sébastien Wilmet
 * Copyright (C) 2019 - Jordi Mas
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

#include "bedit-commands-private.h"
#include "bedit-commands.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "bedit-app.h"
#include "bedit-debug.h"
#include "bedit-dirs.h"

void _bedit_cmd_help_keyboard_shortcuts(BeditWindow *window) {
    static GtkWidget *shortcuts_window;

    bedit_debug(DEBUG_COMMANDS);

    if (shortcuts_window == NULL) {
        GtkBuilder *builder;

        builder = gtk_builder_new_from_resource(
            "/com/bwhmather/bedit/ui/bedit-shortcuts.ui");
        shortcuts_window =
            GTK_WIDGET(gtk_builder_get_object(builder, "shortcuts-bedit"));

        g_signal_connect(
            shortcuts_window, "destroy", G_CALLBACK(gtk_widget_destroyed),
            &shortcuts_window);

        g_object_unref(builder);
    }

    if (GTK_WINDOW(window) !=
        gtk_window_get_transient_for(GTK_WINDOW(shortcuts_window))) {
        gtk_window_set_transient_for(
            GTK_WINDOW(shortcuts_window), GTK_WINDOW(window));
    }

    gtk_widget_show_all(shortcuts_window);
    gtk_window_present(GTK_WINDOW(shortcuts_window));
}

void _bedit_cmd_help_contents(BeditWindow *window) {
    bedit_debug(DEBUG_COMMANDS);

    bedit_app_show_help(
        BEDIT_APP(g_application_get_default()), GTK_WINDOW(window), NULL, NULL);
}

void _bedit_cmd_help_about(BeditWindow *window) {
    static const gchar *const authors[] = {
        "Alex Roberts",
        "Chema Celorio",
        "Evan Lawrence",
        "Federico Mena Quintero <federico@novell.com>",
        "Garrett Regier <garrettregier@gmail.com>",
        "Ignacio Casal Quinteiro <icq@gnome.org>",
        "James Willcox <jwillcox@gnome.org>",
        "Jesse van den Kieboom <jessevdk@gnome.org>",
        "Paolo Borelli <pborelli@gnome.org>",
        "Paolo Maggi <paolo@gnome.org>",
        "S\303\251bastien Lafargue <slafargue@gnome.org>",
        "S\303\251bastien Wilmet <swilmet@gnome.org>",
        "Steve Fr\303\251cinaux <steve@istique.net>",
        NULL};

    static const gchar *const documenters[] = {
        "Jim Campbell <jwcampbell@gmail.com>",
        "Daniel Neel <dneelyep@gmail.com>",
        "Sun GNOME Documentation Team <gdocteam@sun.com>",
        "Eric Baudais <baudais@okstate.edu>", NULL};

    static const gchar copyright[] =
        "Copyright \xc2\xa9 1998-2019 - the bedit team";

    static const gchar comments[] = N_(
        "bedit is a small and lightweight text editor for the GNOME Desktop");

    GdkPixbuf *logo;
    GError *error = NULL;

    bedit_debug(DEBUG_COMMANDS);

    logo = gdk_pixbuf_new_from_resource(
        "/com/bwhmather/bedit/pixmaps/bedit-logo.png", &error);
    if (error != NULL) {
        g_warning("Error when loading the bedit logo: %s", error->message);
        g_clear_error(&error);
    }

    gtk_show_about_dialog(
        GTK_WINDOW(window), "program-name", "bedit", "authors", authors,
        "comments", _(comments), "copyright", copyright, "license-type",
        GTK_LICENSE_GPL_2_0, "documenters", documenters, "logo", logo,
        "translator-credits", _("translator-credits"), "version", VERSION,
        "website", "http://www.bedit.org", "website-label", "www.bedit.org",
        NULL);

    g_clear_object(&logo);
}

