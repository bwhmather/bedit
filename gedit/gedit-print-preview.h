/*
 * gedit-print-preview.h
 *
 * Copyright (C) 2008 Paolo Borelli
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

#ifndef GEDIT_PRINT_PREVIEW_H
#define GEDIT_PRINT_PREVIEW_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GEDIT_TYPE_PRINT_PREVIEW (gedit_print_preview_get_type ())

G_DECLARE_FINAL_TYPE (GeditPrintPreview, gedit_print_preview, GEDIT, PRINT_PREVIEW, GtkGrid)

GtkWidget	*gedit_print_preview_new	(GtkPrintOperation        *operation,
						 GtkPrintOperationPreview *gtk_preview,
						 GtkPrintContext          *context);

G_END_DECLS

#endif /* GEDIT_PRINT_PREVIEW_H */

/* ex:set ts=8 noet: */
