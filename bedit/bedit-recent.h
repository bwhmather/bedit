/*
 * bedit-recent.h
 * This file is part of bedit
 *
 * Copyright (C) 2005 - Paolo Maggi
 * Copyright (C) 2014 - Paolo Borelli
 * Copyright (C) 2014 - Jesse van den Kieboom
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANWINDOWILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GEDIT_RECENT_H
#define GEDIT_RECENT_H

#include <gtk/gtk.h>
#include <bedit/bedit-document.h>

G_BEGIN_DECLS

typedef struct
{
	GtkRecentManager *manager;
	GtkRecentFilter *filter;

	gint limit;
	gchar *substring_filter;

	guint show_private : 1;
	guint show_not_found : 1;
	guint local_only : 1;
} BeditRecentConfiguration;

void		 bedit_recent_add_document		 (BeditDocument            *document);

void		 bedit_recent_remove_if_local		 (GFile                    *location);

void		 bedit_recent_configuration_init_default (BeditRecentConfiguration *config);
void		 bedit_recent_configuration_destroy	 (BeditRecentConfiguration *config);
GList		*bedit_recent_get_items			 (BeditRecentConfiguration *config);

G_END_DECLS

#endif  /* GEDIT_RECENT_H  */

/* ex:set ts=8 noet: */
