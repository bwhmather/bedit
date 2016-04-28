/*
 * gedit-replace-dialog.h
 * This file is part of gedit
 *
 * Copyright (C) 2005 Paolo Maggi
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

#ifndef GEDIT_REPLACE_DIALOG_H
#define GEDIT_REPLACE_DIALOG_H

#include <gtk/gtk.h>
#include <gtksourceview/gtksource.h>
#include "gedit-window.h"

G_BEGIN_DECLS

#define GEDIT_TYPE_REPLACE_DIALOG (gedit_replace_dialog_get_type ())
G_DECLARE_FINAL_TYPE (GeditReplaceDialog, gedit_replace_dialog, GEDIT, REPLACE_DIALOG, GtkDialog)

enum
{
	GEDIT_REPLACE_DIALOG_FIND_RESPONSE = 100,
	GEDIT_REPLACE_DIALOG_REPLACE_RESPONSE,
	GEDIT_REPLACE_DIALOG_REPLACE_ALL_RESPONSE
};

GtkWidget		*gedit_replace_dialog_new			(GeditWindow        *window);

void			 gedit_replace_dialog_present_with_time		(GeditReplaceDialog *dialog,
									 guint32             timestamp);

const gchar		*gedit_replace_dialog_get_search_text		(GeditReplaceDialog *dialog);

const gchar		*gedit_replace_dialog_get_replace_text		(GeditReplaceDialog *dialog);

gboolean		 gedit_replace_dialog_get_backwards		(GeditReplaceDialog *dialog);

void			 gedit_replace_dialog_set_replace_error		(GeditReplaceDialog *dialog,
									 const gchar        *error_msg);

G_END_DECLS

#endif  /* GEDIT_REPLACE_DIALOG_H  */

/* ex:set ts=8 noet: */
