/*
 * gedit-window.c
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

#include <gtk/gtk.h>
#include <gedit/gedit-document.h>
#include "gedit-recent.h"

void
gedit_recent_add_document (GeditDocument *document)
{
	GtkRecentManager *recent_manager;
	GtkRecentData *recent_data;
	GFile *location;
	gchar *uri;

	g_return_if_fail (GEDIT_IS_DOCUMENT (document));

	static gchar *groups[2] = {
		"gedit",
		NULL
	};

	location = gedit_document_get_location (document);
	if (location !=  NULL)
	{
		recent_manager = gtk_recent_manager_get_default ();

		recent_data = g_slice_new (GtkRecentData);

		recent_data->display_name = NULL;
		recent_data->description = NULL;
		recent_data->mime_type = gedit_document_get_mime_type (document);
		recent_data->app_name = (gchar *) g_get_application_name ();
		recent_data->app_exec = g_strjoin (" ", g_get_prgname (), "%u", NULL);
		recent_data->groups = groups;
		recent_data->is_private = FALSE;

		uri = g_file_get_uri (location);
		gtk_recent_manager_add_full (recent_manager,
					     uri,
					     recent_data);

		g_free (uri);
		g_free (recent_data->app_exec);
		g_free (recent_data->mime_type);
		g_slice_free (GtkRecentData, recent_data);
		g_object_unref (location);
	}
}

void
gedit_recent_remove_if_local (GFile *location)
{
	g_return_if_fail (G_IS_FILE (location));

	/* If a file is local chances are that if load/save fails the file has
	 * beed removed and the failure is permanent so we remove it from the
	 * list of recent files. For remote files the failure may be just
	 * transitory and we keep the file in the list.
	 */
	if (g_file_has_uri_scheme (location, "file"))
	{
		GtkRecentManager *recent_manager;
		gchar *uri;

		recent_manager = gtk_recent_manager_get_default ();

		uri = g_file_get_uri (location);
		gtk_recent_manager_remove_item (recent_manager, uri, NULL);
		g_free (uri);
	}
}

/* ex:set ts=8 noet: */
