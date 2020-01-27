/*
 * bedit-encodings-dialog.h
 * This file is part of bedit
 *
 * Copyright (C) 2003-2005 Paolo Maggi
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

#ifndef BEDIT_ENCODINGS_DIALOG_H
#define BEDIT_ENCODINGS_DIALOG_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define BEDIT_TYPE_ENCODINGS_DIALOG (bedit_encodings_dialog_get_type())

G_DECLARE_FINAL_TYPE(
    BeditEncodingsDialog, bedit_encodings_dialog, BEDIT, ENCODINGS_DIALOG,
    GtkDialog)

GtkWidget *bedit_encodings_dialog_new(void);

G_END_DECLS

#endif /* BEDIT_ENCODINGS_DIALOG_H */

