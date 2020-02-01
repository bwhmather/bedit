/*
 * bedit-replace-dialog.h
 * This file is part of bedit
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

#ifndef BEDIT_REPLACE_DIALOG_H
#define BEDIT_REPLACE_DIALOG_H

#include <gtk/gtk.h>
#include <gtksourceview/gtksource.h>
#include "bedit-window.h"

G_BEGIN_DECLS

#define BEDIT_TYPE_REPLACE_DIALOG (bedit_replace_dialog_get_type())
G_DECLARE_FINAL_TYPE(
    BeditReplaceDialog, bedit_replace_dialog, BEDIT, REPLACE_DIALOG, GtkDialog)

enum {
    BEDIT_REPLACE_DIALOG_FIND_RESPONSE = 100,
    BEDIT_REPLACE_DIALOG_REPLACE_RESPONSE,
    BEDIT_REPLACE_DIALOG_REPLACE_ALL_RESPONSE
};

GtkWidget *bedit_replace_dialog_new(BeditWindow *window);

void bedit_replace_dialog_present_with_time(
    BeditReplaceDialog *dialog, guint32 timestamp);

const gchar *bedit_replace_dialog_get_search_text(BeditReplaceDialog *dialog);

const gchar *bedit_replace_dialog_get_replace_text(BeditReplaceDialog *dialog);

gboolean bedit_replace_dialog_get_backwards(BeditReplaceDialog *dialog);

void bedit_replace_dialog_set_replace_error(
    BeditReplaceDialog *dialog, const gchar *error_msg);

G_END_DECLS

#endif /* BEDIT_REPLACE_DIALOG_H  */

