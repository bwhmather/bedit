/*
 * bedit-file-chooser-dialog-gtk.h
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * Based on gedit-file-chooser-dialog-gtk.h from Gedit.
 *
 * Copyright (C) 2005 - Paolo Maggi
 * Copyright (C) 2014 - Jesse van den Kieboom
 * Copyright (C) 2015 - Paolo Borelli
 * Copyright (C) 2016-2019 - Sébastien Wilmet
 * Copyright (C) 2018 - James Henstridge
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

#ifndef BEDIT_FILE_CHOOSER_DIALOG_GTK_H
#define BEDIT_FILE_CHOOSER_DIALOG_GTK_H

#include <gtk/gtk.h>
#include "bedit-file-chooser-dialog.h"

G_BEGIN_DECLS

#define BEDIT_TYPE_FILE_CHOOSER_DIALOG_GTK                                  \
    (bedit_file_chooser_dialog_gtk_get_type())

G_DECLARE_FINAL_TYPE(
    BeditFileChooserDialogGtk, bedit_file_chooser_dialog_gtk, BEDIT,
    FILE_CHOOSER_DIALOG_GTK, GtkFileChooserDialog
)

BeditFileChooserDialog *bedit_file_chooser_dialog_gtk_create(
    const gchar *title, GtkWindow *parent,
    BeditFileChooserFlags flags, const GtkSourceEncoding *encoding,
    const gchar *cancel_label, GtkResponseType cancel_response,
    const gchar *accept_label, GtkResponseType accept_response
);

G_END_DECLS

#endif /* BEDIT_FILE_CHOOSER_DIALOG_GTK_H */

