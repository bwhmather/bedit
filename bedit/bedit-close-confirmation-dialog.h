/*
 * bedit-close-confirmation-dialog.h
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * Based on gedit-close-confirmation-dialog.h from Gedit.
 *
 * Copyright (C) 2004-2005 - GNOME Foundation
 * Copyright (C) 2012 - Ignacio Casal Quinteiro
 * Copyright (C) 2012-2015 - Paolo Borelli
 * Copyright (C) 2013-2016 - Sébastien Wilmet
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

#ifndef BEDIT_CLOSE_CONFIRMATION_DIALOG_H
#define BEDIT_CLOSE_CONFIRMATION_DIALOG_H

#include <bedit/bedit-document.h>
#include <glib.h>
#include <gtk/gtk.h>

#define BEDIT_TYPE_CLOSE_CONFIRMATION_DIALOG                                \
    (bedit_close_confirmation_dialog_get_type())

G_DECLARE_FINAL_TYPE(
    BeditCloseConfirmationDialog, bedit_close_confirmation_dialog, BEDIT,
    CLOSE_CONFIRMATION_DIALOG, GtkMessageDialog
)

GtkWidget *bedit_close_confirmation_dialog_new(
    GtkWindow *parent, GList *unsaved_documents
);

GtkWidget *bedit_close_confirmation_dialog_new_single(
    GtkWindow *parent, BeditDocument *doc
);

const GList *bedit_close_confirmation_dialog_get_unsaved_documents(
    BeditCloseConfirmationDialog *dlg
);

GList *bedit_close_confirmation_dialog_get_selected_documents(
    BeditCloseConfirmationDialog *dlg
);

#endif /* BEDIT_CLOSE_CONFIRMATION_DIALOG_H */

