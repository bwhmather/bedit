/*
 * bedit-file-chooser-dialog.c
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * Based on gedit-file-chooser-dialog.c from Gedit.
 *
 * Copyright (C) 2005-2015 - Paolo Borelli
 * Copyright (C) 2007 - Paolo Maggi
 * Copyright (C) 2008-2014 - Jesse van den Kieboom
 * Copyright (C) 2009-2013 - Ignacio Casal Quinteiro
 * Copyright (C) 2010 - Philip Withnall, Steve Frécinaux
 * Copyright (C) 2010-2015 - Garrett Regier
 * Copyright (C) 2012 - Seif Lotfy
 * Copyright (C) 2013-2019 - Sébastien Wilmet
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

#include "bedit-file-chooser-dialog.h"

#ifdef OS_OSX
#include "bedit-file-chooser-dialog-osx.h"
#else
#include "bedit-file-chooser-dialog-gtk.h"
#endif

G_DEFINE_INTERFACE(
    BeditFileChooserDialog, bedit_file_chooser_dialog, G_TYPE_OBJECT
)

static gboolean confirm_overwrite_accumulator(
    GSignalInvocationHint *ihint, GValue *return_accu,
    const GValue *handler_return, gpointer dummy
) {
    gboolean continue_emission;
    GtkFileChooserConfirmation conf;

    conf = g_value_get_enum(handler_return);
    g_value_set_enum(return_accu, conf);
    continue_emission = (conf == GTK_FILE_CHOOSER_CONFIRMATION_CONFIRM);

    return continue_emission;
}

static void bedit_file_chooser_dialog_default_init(
    BeditFileChooserDialogInterface *iface
) {
    g_signal_new(
        "response", G_TYPE_FROM_INTERFACE(iface),
        G_SIGNAL_RUN_LAST, 0, NULL,
        NULL, NULL, G_TYPE_NONE, 1, G_TYPE_INT
    );

    g_signal_new(
        "confirm-overwrite", G_TYPE_FROM_INTERFACE(iface),
        G_SIGNAL_RUN_LAST, 0, confirm_overwrite_accumulator,
        NULL, NULL, GTK_TYPE_FILE_CHOOSER_CONFIRMATION, 0
    );
}

BeditFileChooserDialog *bedit_file_chooser_dialog_create(
    const gchar *title, GtkWindow *parent, BeditFileChooserFlags flags,
    const GtkSourceEncoding *encoding, const gchar *cancel_label,
    GtkResponseType cancel_response, const gchar *accept_label,
    GtkResponseType accept_response
) {
#ifdef OS_OSX
    return bedit_file_chooser_dialog_osx_create(
        title, parent, flags, encoding,
        cancel_label, cancel_response,
        accept_label, accept_response
    );
#else
    return bedit_file_chooser_dialog_gtk_create(
        title, parent, flags, encoding,
        cancel_label, cancel_response,
        accept_label, accept_response
    );
#endif
}

void bedit_file_chooser_dialog_set_encoding(
    BeditFileChooserDialog *dialog, const GtkSourceEncoding *encoding
) {
    BeditFileChooserDialogInterface *iface;

    g_return_if_fail(BEDIT_IS_FILE_CHOOSER_DIALOG(dialog));

    iface = BEDIT_FILE_CHOOSER_DIALOG_GET_IFACE(dialog);
    g_return_if_fail(iface->set_encoding != NULL);

    iface->set_encoding(dialog, encoding);
}

const GtkSourceEncoding *bedit_file_chooser_dialog_get_encoding(
    BeditFileChooserDialog *dialog
) {
    BeditFileChooserDialogInterface *iface;

    g_return_val_if_fail(BEDIT_IS_FILE_CHOOSER_DIALOG(dialog), NULL);

    iface = BEDIT_FILE_CHOOSER_DIALOG_GET_IFACE(dialog);
    g_return_val_if_fail(iface->get_encoding != NULL, NULL);

    return iface->get_encoding(dialog);
}

void bedit_file_chooser_dialog_set_newline_type(
    BeditFileChooserDialog *dialog, GtkSourceNewlineType newline_type
) {
    BeditFileChooserDialogInterface *iface;

    g_return_if_fail(BEDIT_IS_FILE_CHOOSER_DIALOG(dialog));

    iface = BEDIT_FILE_CHOOSER_DIALOG_GET_IFACE(dialog);
    g_return_if_fail(iface->set_newline_type != NULL);

    iface->set_newline_type(dialog, newline_type);
}

GtkSourceNewlineType bedit_file_chooser_dialog_get_newline_type(
    BeditFileChooserDialog *dialog
) {
    BeditFileChooserDialogInterface *iface;

    g_return_val_if_fail(
        BEDIT_IS_FILE_CHOOSER_DIALOG(dialog), GTK_SOURCE_NEWLINE_TYPE_DEFAULT
    );

    iface = BEDIT_FILE_CHOOSER_DIALOG_GET_IFACE(dialog);
    g_return_val_if_fail(
        iface->get_newline_type != NULL, GTK_SOURCE_NEWLINE_TYPE_DEFAULT
    );

    return iface->get_newline_type(dialog);
}

void bedit_file_chooser_dialog_set_current_folder(
    BeditFileChooserDialog *dialog, GFile *folder
) {
    BeditFileChooserDialogInterface *iface;

    g_return_if_fail(BEDIT_IS_FILE_CHOOSER_DIALOG(dialog));

    iface = BEDIT_FILE_CHOOSER_DIALOG_GET_IFACE(dialog);
    g_return_if_fail(iface->set_current_folder != NULL);

    iface->set_current_folder(dialog, folder);
}

void bedit_file_chooser_dialog_set_current_name(
    BeditFileChooserDialog *dialog, const gchar *name
) {
    BeditFileChooserDialogInterface *iface;

    g_return_if_fail(BEDIT_IS_FILE_CHOOSER_DIALOG(dialog));

    iface = BEDIT_FILE_CHOOSER_DIALOG_GET_IFACE(dialog);
    g_return_if_fail(iface->set_current_name != NULL);

    iface->set_current_name(dialog, name);
}

void bedit_file_chooser_dialog_set_file(
    BeditFileChooserDialog *dialog, GFile *file
) {
    BeditFileChooserDialogInterface *iface;

    g_return_if_fail(BEDIT_IS_FILE_CHOOSER_DIALOG(dialog));
    g_return_if_fail(file == NULL || G_IS_FILE(file));

    iface = BEDIT_FILE_CHOOSER_DIALOG_GET_IFACE(dialog);
    g_return_if_fail(iface->set_file != NULL);

    iface->set_file(dialog, file);
}

GSList *bedit_file_chooser_dialog_get_files(BeditFileChooserDialog *dialog) {
    BeditFileChooserDialogInterface *iface;

    g_return_val_if_fail(BEDIT_IS_FILE_CHOOSER_DIALOG(dialog), NULL);

    iface = BEDIT_FILE_CHOOSER_DIALOG_GET_IFACE(dialog);
    g_return_val_if_fail(iface->get_files != NULL, NULL);

    return iface->get_files(dialog);
}

GFile *bedit_file_chooser_dialog_get_file(BeditFileChooserDialog *dialog) {
    BeditFileChooserDialogInterface *iface;

    g_return_val_if_fail(BEDIT_IS_FILE_CHOOSER_DIALOG(dialog), NULL);

    iface = BEDIT_FILE_CHOOSER_DIALOG_GET_IFACE(dialog);
    g_return_val_if_fail(iface->get_file != NULL, NULL);

    return iface->get_file(dialog);
}

void bedit_file_chooser_dialog_set_do_overwrite_confirmation(
    BeditFileChooserDialog *dialog, gboolean overwrite_confirmation
) {
    BeditFileChooserDialogInterface *iface;

    g_return_if_fail(BEDIT_IS_FILE_CHOOSER_DIALOG(dialog));

    iface = BEDIT_FILE_CHOOSER_DIALOG_GET_IFACE(dialog);
    g_return_if_fail(iface->set_do_overwrite_confirmation != NULL);

    iface->set_do_overwrite_confirmation(dialog, overwrite_confirmation);
}

void bedit_file_chooser_dialog_show(BeditFileChooserDialog *dialog) {
    BeditFileChooserDialogInterface *iface;

    g_return_if_fail(BEDIT_IS_FILE_CHOOSER_DIALOG(dialog));

    iface = BEDIT_FILE_CHOOSER_DIALOG_GET_IFACE(dialog);
    g_return_if_fail(iface->show != NULL);

    iface->show(dialog);
}

void bedit_file_chooser_dialog_hide(BeditFileChooserDialog *dialog) {
    BeditFileChooserDialogInterface *iface;

    g_return_if_fail(BEDIT_IS_FILE_CHOOSER_DIALOG(dialog));

    iface = BEDIT_FILE_CHOOSER_DIALOG_GET_IFACE(dialog);
    g_return_if_fail(iface->hide != NULL);

    iface->hide(dialog);
}

void bedit_file_chooser_dialog_destroy(BeditFileChooserDialog *dialog) {
    BeditFileChooserDialogInterface *iface;

    g_return_if_fail(BEDIT_IS_FILE_CHOOSER_DIALOG(dialog));

    iface = BEDIT_FILE_CHOOSER_DIALOG_GET_IFACE(dialog);
    g_return_if_fail(iface->destroy != NULL);

    iface->destroy(dialog);
}

void bedit_file_chooser_dialog_set_modal(
    BeditFileChooserDialog *dialog, gboolean is_modal
) {
    BeditFileChooserDialogInterface *iface;

    g_return_if_fail(BEDIT_IS_FILE_CHOOSER_DIALOG(dialog));

    iface = BEDIT_FILE_CHOOSER_DIALOG_GET_IFACE(dialog);
    g_return_if_fail(iface->set_modal != NULL);

    iface->set_modal(dialog, is_modal);
}

GtkWindow *bedit_file_chooser_dialog_get_window(
    BeditFileChooserDialog *dialog
) {
    BeditFileChooserDialogInterface *iface;

    g_return_val_if_fail(BEDIT_IS_FILE_CHOOSER_DIALOG(dialog), NULL);

    iface = BEDIT_FILE_CHOOSER_DIALOG_GET_IFACE(dialog);

    if (iface->get_window) {
        return iface->get_window(dialog);
    }

    return NULL;
}

void bedit_file_chooser_dialog_add_pattern_filter(
    BeditFileChooserDialog *dialog, const gchar *name, const gchar *pattern
) {
    BeditFileChooserDialogInterface *iface;

    g_return_if_fail(BEDIT_IS_FILE_CHOOSER_DIALOG(dialog));

    iface = BEDIT_FILE_CHOOSER_DIALOG_GET_IFACE(dialog);

    if (iface->add_pattern_filter) {
        iface->add_pattern_filter(dialog, name, pattern);
    }
}

