/*
 * bedit-file-browser-utils.c
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * Based on gedit-file-browser-utils.c from Gedit.
 *
 * Copyright (C) 2006 - Jesse van den Kieboom <jesse@icecrew.nl>
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

#include "bedit-file-browser-utils.h"

#include <glib/gi18n-lib.h>

#include "bedit-utils.h"

gchar *bedit_file_browser_utils_icon_name_from_file(GFile *file) {
    GFileInfo *info;
    GIcon *icon;

    info = g_file_query_info(
        file, G_FILE_ATTRIBUTE_STANDARD_ICON, G_FILE_QUERY_INFO_NONE,
        NULL, NULL
    );

    if (!info) {
        return NULL;
    }

    if (
        (icon = g_file_info_get_icon(info)) &&
        G_IS_THEMED_ICON(icon)
    ) {
        const gchar *const *names = g_themed_icon_get_names(
            G_THEMED_ICON(icon)
        );
        return g_strdup(names[0]);
    }

    g_object_unref(info);
    return NULL;
}

gchar *bedit_file_browser_utils_name_from_themed_icon(GIcon *icon) {
    GtkIconTheme *theme;
    const gchar *const *names;

    if (!G_IS_THEMED_ICON(icon)) {
        return NULL;
    }

    theme = gtk_icon_theme_get_default();
    names = g_themed_icon_get_names(G_THEMED_ICON(icon));

    if (gtk_icon_theme_has_icon(theme, names[0])) {
        return g_strdup(names[0]);
    }

    return NULL;
}

gchar *bedit_file_browser_utils_file_basename(GFile *file) {
    return bedit_utils_basename_for_display(file);
}

gboolean bedit_file_browser_utils_confirmation_dialog(
    BeditWindow *window, GtkMessageType type, gchar const *message,
    gchar const *secondary, gchar const *button_label
) {
    GtkWidget *dlg;
    gint ret;

    dlg = gtk_message_dialog_new(
        GTK_WINDOW(window), GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        type, GTK_BUTTONS_NONE, "%s", message
    );

    if (secondary) {
        gtk_message_dialog_format_secondary_text(
            GTK_MESSAGE_DIALOG(dlg), "%s", secondary
        );
    }

    gtk_dialog_add_buttons(
        GTK_DIALOG(dlg),
        _("_Cancel"), GTK_RESPONSE_CANCEL,
        button_label, GTK_RESPONSE_OK,
        NULL
    );

    gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_CANCEL);

    ret = gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);

    return (ret == GTK_RESPONSE_OK);
}

