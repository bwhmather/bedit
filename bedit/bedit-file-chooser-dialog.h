/*
 * bedit-file-chooser-dialog.h
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * Based on gedit-file-chooser-dialog.h from Gedit.
 *
 * Copyright (C) 2005-2015 - Paolo Borelli
 * Copyright (C) 2008 - Joe M Smith
 * Copyright (C) 2010 - Garrett Regier, Steve Frécinaux
 * Copyright (C) 2010-2014 - Jesse van den Kieboom
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

#ifndef BEDIT_FILE_CHOOSER_DIALOG_H
#define BEDIT_FILE_CHOOSER_DIALOG_H

#include <gtk/gtk.h>
#include <gtksourceview/gtksource.h>

G_BEGIN_DECLS

#define BEDIT_TYPE_FILE_CHOOSER_DIALOG (bedit_file_chooser_dialog_get_type())

G_DECLARE_INTERFACE(
    BeditFileChooserDialog, bedit_file_chooser_dialog, BEDIT,
    FILE_CHOOSER_DIALOG, GObject)

struct _BeditFileChooserDialogInterface {
    GTypeInterface g_iface;

    /* Virtual public methods */
    void (*set_encoding)(
        BeditFileChooserDialog *dialog, const GtkSourceEncoding *encoding
    );

    const GtkSourceEncoding *(*get_encoding)(BeditFileChooserDialog *dialog);

    void (*set_newline_type)(
        BeditFileChooserDialog *dialog, GtkSourceNewlineType newline_type
    );

    GtkSourceNewlineType (*get_newline_type)(BeditFileChooserDialog *dialog);

    void (*set_current_folder)(BeditFileChooserDialog *dialog, GFile *folder);

    void (*set_current_name)(
        BeditFileChooserDialog *dialog, const gchar *name
    );

    void (*set_file)(BeditFileChooserDialog *dialog, GFile *file);

    GFile *(*get_file)(BeditFileChooserDialog *dialog);

    GSList *(*get_files)(BeditFileChooserDialog *dialog);

    void (*set_do_overwrite_confirmation)(
        BeditFileChooserDialog *dialog, gboolean overwrite_confirmation
    );

    void (*show)(BeditFileChooserDialog *dialog);
    void (*hide)(BeditFileChooserDialog *dialog);

    void (*destroy)(BeditFileChooserDialog *dialog);

    void (*set_modal)(BeditFileChooserDialog *dialog, gboolean is_modal);

    GtkWindow *(*get_window)(BeditFileChooserDialog *dialog);

    void (*add_pattern_filter)(
        BeditFileChooserDialog *dialog, const gchar *name,
        const gchar *pattern
    );
};

typedef enum {
    BEDIT_FILE_CHOOSER_SAVE = 1 << 0,
    BEDIT_FILE_CHOOSER_OPEN = 1 << 1,
    BEDIT_FILE_CHOOSER_ENABLE_ENCODING = 1 << 2,
    BEDIT_FILE_CHOOSER_ENABLE_LINE_ENDING = 1 << 3,
    BEDIT_FILE_CHOOSER_ENABLE_DEFAULT_FILTERS = 1 << 4
} BeditFileChooserFlags;

BeditFileChooserDialog *bedit_file_chooser_dialog_create(
    const gchar *title, GtkWindow *parent,
    BeditFileChooserFlags flags, const GtkSourceEncoding *encoding,
    const gchar *cancel_label, GtkResponseType cancel_response,
    const gchar *accept_label, GtkResponseType accept_response
);

void bedit_file_chooser_dialog_destroy(BeditFileChooserDialog *dialog);

void bedit_file_chooser_dialog_set_encoding(
    BeditFileChooserDialog *dialog, const GtkSourceEncoding *encoding
);

const GtkSourceEncoding *bedit_file_chooser_dialog_get_encoding(
    BeditFileChooserDialog *dialog
);

void bedit_file_chooser_dialog_set_newline_type(
    BeditFileChooserDialog *dialog, GtkSourceNewlineType newline_type
);

GtkSourceNewlineType bedit_file_chooser_dialog_get_newline_type(
    BeditFileChooserDialog *dialog
);

void bedit_file_chooser_dialog_set_current_folder(
    BeditFileChooserDialog *dialog, GFile *folder
);

void bedit_file_chooser_dialog_set_current_name(
    BeditFileChooserDialog *dialog, const gchar *name
);

void bedit_file_chooser_dialog_set_file(
    BeditFileChooserDialog *dialog, GFile *file
);

GFile *bedit_file_chooser_dialog_get_file(BeditFileChooserDialog *dialog);

GSList *bedit_file_chooser_dialog_get_files(BeditFileChooserDialog *dialog);

void bedit_file_chooser_dialog_set_do_overwrite_confirmation(
    BeditFileChooserDialog *dialog, gboolean overwrite_confirmation
);

void bedit_file_chooser_dialog_show(BeditFileChooserDialog *dialog);
void bedit_file_chooser_dialog_hide(BeditFileChooserDialog *dialog);

void bedit_file_chooser_dialog_set_modal(
    BeditFileChooserDialog *dialog, gboolean is_modal
);

GtkWindow *bedit_file_chooser_dialog_get_window(
    BeditFileChooserDialog *dialog
);

void bedit_file_chooser_dialog_add_pattern_filter(
    BeditFileChooserDialog *dialog, const gchar *name, const gchar *pattern
);

G_END_DECLS

#endif /* BEDIT_FILE_CHOOSER_DIALOG_H */

