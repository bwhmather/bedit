/*
 * gedit-app.h
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

#ifndef GEDIT_APP_H
#define GEDIT_APP_H

#include <gtk/gtk.h>
#include <gedit/gedit-window.h>

G_BEGIN_DECLS

#define GEDIT_TYPE_APP (gedit_app_get_type())

G_DECLARE_DERIVABLE_TYPE (BeditApp, gedit_app, GEDIT, APP, GtkApplication)

struct _BeditAppClass
{
	GtkApplicationClass parent_class;

	gboolean (*show_help)                   (BeditApp    *app,
	                                         GtkWindow   *parent,
	                                         const gchar *name,
	                                         const gchar *link_id);

	gchar *(*help_link_id)                  (BeditApp    *app,
	                                         const gchar *name,
	                                         const gchar *link_id);

	void (*set_window_title)                (BeditApp    *app,
	                                         BeditWindow *window,
	                                         const gchar *title);

	BeditWindow *(*create_window)           (BeditApp    *app);

	gboolean (*process_window_event)        (BeditApp    *app,
	                                         BeditWindow *window,
	                                         GdkEvent    *event);
};

typedef enum
{
	GEDIT_LOCKDOWN_COMMAND_LINE	= 1 << 0,
	GEDIT_LOCKDOWN_PRINTING		= 1 << 1,
	GEDIT_LOCKDOWN_PRINT_SETUP	= 1 << 2,
	GEDIT_LOCKDOWN_SAVE_TO_DISK	= 1 << 3
} BeditLockdownMask;

/* We need to define this here to avoid problems with bindings and gsettings */
#define GEDIT_LOCKDOWN_ALL 0xF

BeditWindow	*gedit_app_create_window		(BeditApp    *app,
							 GdkScreen   *screen);

GList		*gedit_app_get_main_windows		(BeditApp    *app);

GList		*gedit_app_get_documents		(BeditApp    *app);

GList		*gedit_app_get_views			(BeditApp    *app);

/* Lockdown state */
BeditLockdownMask gedit_app_get_lockdown		(BeditApp    *app);

gboolean	 gedit_app_show_help			(BeditApp    *app,
                                                         GtkWindow   *parent,
                                                         const gchar *name,
                                                         const gchar *link_id);

void		 gedit_app_set_window_title		(BeditApp    *app,
                                                         BeditWindow *window,
                                                         const gchar *title);
gboolean	gedit_app_process_window_event		(BeditApp    *app,
							 BeditWindow *window,
							 GdkEvent    *event);

G_END_DECLS

#endif /* GEDIT_APP_H */

/* ex:set ts=8 noet: */
