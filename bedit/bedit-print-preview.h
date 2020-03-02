/*
 * bedit-print-preview.h
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * Based on gedit-print-preview.h from Gedit.
 *
 * Copyright (C) 2008 - Joe M Smith
 * Copyright (C) 2008-2015 - Paolo Borelli
 * Copyright (C) 2010 - Jesse van den Kieboom, Steve Frécinaux
 * Copyright (C) 2010-2011 - Garrett Regier
 * Copyright (C) 2015-2016 - Sébastien Wilmet
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

#ifndef BEDIT_PRINT_PREVIEW_H
#define BEDIT_PRINT_PREVIEW_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define BEDIT_TYPE_PRINT_PREVIEW (bedit_print_preview_get_type())

G_DECLARE_FINAL_TYPE(
    BeditPrintPreview, bedit_print_preview, BEDIT, PRINT_PREVIEW, GtkGrid
)

GtkWidget *bedit_print_preview_new(
    GtkPrintOperation *operation,
    GtkPrintOperationPreview *gtk_preview,
    GtkPrintContext *context
);

G_END_DECLS

#endif /* BEDIT_PRINT_PREVIEW_H */

