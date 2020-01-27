/*
 * bedit-commands-file.c
 * This file is part of bedit
 *
 * Copyright (C) 1998, 1999 Alex Roberts, Evan Lawrence
 * Copyright (C) 2000, 2001 Chema Celorio, Paolo Maggi
 * Copyright (C) 2002-2005 Paolo Maggi
 * Copyright (C) 2014 Sébastien Wilmet
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

#include "bedit-commands-private.h"
#include "bedit-commands.h"

#include <gio/gio.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "bedit-close-confirmation-dialog.h"
#include "bedit-debug.h"
#include "bedit-document-private.h"
#include "bedit-document.h"
#include "bedit-file-chooser-dialog.h"
#include "bedit-notebook.h"
#include "bedit-statusbar.h"
#include "bedit-tab-private.h"
#include "bedit-tab.h"
#include "bedit-utils.h"
#include "bedit-window-private.h"
#include "bedit-window.h"

#define BEDIT_OPEN_DIALOG_KEY "bedit-open-dialog-key"
#define BEDIT_IS_CLOSING_ALL "bedit-is-closing-all"
#define BEDIT_NOTEBOOK_TO_CLOSE "bedit-notebook-to-close"
#define BEDIT_IS_QUITTING "bedit-is-quitting"
#define BEDIT_IS_QUITTING_ALL "bedit-is-quitting-all"

static void tab_state_changed_while_saving(
    BeditTab *tab, GParamSpec *pspec, BeditWindow *window);

void _bedit_cmd_file_new(
    GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    BeditWindow *window = BEDIT_WINDOW(user_data);

    bedit_debug(DEBUG_COMMANDS);

    bedit_window_create_tab(window, TRUE);
}

static BeditTab *get_tab_from_file(GList *docs, GFile *file) {
    GList *l;

    for (l = docs; l != NULL; l = l->next) {
        BeditDocument *doc;
        GtkSourceFile *source_file;
        GFile *location;

        doc = l->data;
        source_file = bedit_document_get_file(doc);
        location = gtk_source_file_get_location(source_file);

        if (location != NULL && g_file_equal(location, file)) {
            return bedit_tab_get_from_document(doc);
        }
    }

    return NULL;
}

static gboolean is_duplicated_file(GSList *files, GFile *file) {
    GSList *l;

    for (l = files; l != NULL; l = l->next) {
        if (g_file_equal(l->data, file)) {
            return TRUE;
        }
    }

    return FALSE;
}

/* File loading */
static GSList *load_file_list(
    BeditWindow *window, const GSList *files, const GtkSourceEncoding *encoding,
    gint line_pos, gint column_pos, gboolean create) {
    GList *win_docs;
    GSList *files_to_load = NULL;
    GSList *loaded_files = NULL;
    BeditTab *tab;
    gboolean jump_to = TRUE; /* Whether to jump to the new tab */
    const GSList *l;
    gint num_loaded_files = 0;

    bedit_debug(DEBUG_COMMANDS);

    win_docs = bedit_window_get_documents(window);

    /* Remove the files corresponding to documents already opened in
     * "window" and remove duplicates from the "files" list.
     */
    for (l = files; l != NULL; l = l->next) {
        GFile *file = l->data;

        if (is_duplicated_file(files_to_load, file)) {
            continue;
        }

        tab = get_tab_from_file(win_docs, file);

        if (tab == NULL) {
            files_to_load = g_slist_prepend(files_to_load, file);
        } else {
            if (l == files) {
                BeditDocument *doc;

                bedit_window_set_active_tab(window, tab);
                jump_to = FALSE;
                doc = bedit_tab_get_document(tab);

                if (line_pos > 0) {
                    if (column_pos > 0) {
                        bedit_document_goto_line_offset(
                            doc, line_pos - 1, column_pos - 1);
                    } else {
                        bedit_document_goto_line(doc, line_pos - 1);
                    }

                    bedit_view_scroll_to_cursor(bedit_tab_get_view(tab));
                }
            }

            ++num_loaded_files;
            loaded_files =
                g_slist_prepend(loaded_files, bedit_tab_get_document(tab));
        }
    }

    g_list_free(win_docs);

    if (files_to_load == NULL) {
        return g_slist_reverse(loaded_files);
    }

    files_to_load = g_slist_reverse(files_to_load);
    l = files_to_load;

    tab = bedit_window_get_active_tab(window);
    if (tab != NULL) {
        BeditDocument *doc;

        doc = bedit_tab_get_document(tab);

        if (bedit_document_is_untouched(doc) &&
            bedit_tab_get_state(tab) == BEDIT_TAB_STATE_NORMAL) {
            _bedit_tab_load(
                tab, l->data, encoding, line_pos, column_pos, create);

            /* make sure the view has focus */
            gtk_widget_grab_focus(GTK_WIDGET(bedit_tab_get_view(tab)));

            l = g_slist_next(l);
            jump_to = FALSE;

            ++num_loaded_files;
            loaded_files =
                g_slist_prepend(loaded_files, bedit_tab_get_document(tab));
        }
    }

    while (l != NULL) {
        g_return_val_if_fail(l->data != NULL, NULL);

        tab = bedit_window_create_tab_from_location(
            window, l->data, encoding, line_pos, column_pos, create, jump_to);

        if (tab != NULL) {
            jump_to = FALSE;

            ++num_loaded_files;
            loaded_files =
                g_slist_prepend(loaded_files, bedit_tab_get_document(tab));
        }

        l = g_slist_next(l);
    }

    loaded_files = g_slist_reverse(loaded_files);

    if (num_loaded_files == 1) {
        BeditDocument *doc;
        gchar *uri_for_display;

        g_return_val_if_fail(tab != NULL, loaded_files);

        doc = bedit_tab_get_document(tab);
        uri_for_display = bedit_document_get_uri_for_display(doc);

        bedit_statusbar_flash_message(
            BEDIT_STATUSBAR(window->priv->statusbar),
            window->priv->generic_message_cid,
            _("Loading file “%s”\342\200\246"), uri_for_display);

        g_free(uri_for_display);
    } else {
        bedit_statusbar_flash_message(
            BEDIT_STATUSBAR(window->priv->statusbar),
            window->priv->generic_message_cid,
            ngettext(
                "Loading %d file\342\200\246", "Loading %d files\342\200\246",
                num_loaded_files),
            num_loaded_files);
    }

    g_slist_free(files_to_load);

    return loaded_files;
}

/**
 * bedit_commands_load_location:
 * @window: a #BeditWindow
 * @location: a #GFile to load
 * @encoding: (allow-none): the #GtkSourceEncoding of @location
 * @line_pos: the line position to place the cursor
 * @column_pos: the line column to place the cursor
 *
 * Loads @location. Ignores non-existing locations.
 */
void bedit_commands_load_location(
    BeditWindow *window, GFile *location, const GtkSourceEncoding *encoding,
    gint line_pos, gint column_pos) {
    GSList *locations = NULL;
    gchar *uri;
    GSList *ret;

    g_return_if_fail(BEDIT_IS_WINDOW(window));
    g_return_if_fail(G_IS_FILE(location));
    g_return_if_fail(bedit_utils_is_valid_location(location));

    uri = g_file_get_uri(location);
    bedit_debug_message(DEBUG_COMMANDS, "Loading URI '%s'", uri);
    g_free(uri);

    locations = g_slist_prepend(locations, location);

    ret = load_file_list(
        window, locations, encoding, line_pos, column_pos, FALSE);
    g_slist_free(ret);

    g_slist_free(locations);
}

/**
 * bedit_commands_load_locations:
 * @window: a #BeditWindow
 * @locations: (element-type Gio.File): the locations to load
 * @encoding: (allow-none): the #GtkSourceEncoding
 * @line_pos: the line position to place the cursor
 * @column_pos: the line column to place the cursor
 *
 * Loads @locations. Ignore non-existing locations.
 *
 * Returns: (element-type Bedit.Document) (transfer container): the locations
 * that were loaded.
 */
GSList *bedit_commands_load_locations(
    BeditWindow *window, const GSList *locations,
    const GtkSourceEncoding *encoding, gint line_pos, gint column_pos) {
    g_return_val_if_fail(BEDIT_IS_WINDOW(window), NULL);
    g_return_val_if_fail(locations != NULL && locations->data != NULL, NULL);

    bedit_debug(DEBUG_COMMANDS);

    return load_file_list(
        window, locations, encoding, line_pos, column_pos, FALSE);
}

/*
 * From the command line we can specify a line position for the
 * first doc. Beside specifying a non-existing file creates a
 * titled document.
 */
GSList *_bedit_cmd_load_files_from_prompt(
    BeditWindow *window, GSList *files, const GtkSourceEncoding *encoding,
    gint line_pos, gint column_pos) {
    bedit_debug(DEBUG_COMMANDS);

    return load_file_list(window, files, encoding, line_pos, column_pos, TRUE);
}

static void open_dialog_destroyed(
    BeditWindow *window, BeditFileChooserDialog *dialog) {
    bedit_debug(DEBUG_COMMANDS);

    g_object_set_data(G_OBJECT(window), BEDIT_OPEN_DIALOG_KEY, NULL);
}

static void open_dialog_response_cb(
    BeditFileChooserDialog *dialog, gint response_id, BeditWindow *window) {
    GSList *files;
    const GtkSourceEncoding *encoding;
    GSList *loaded;

    bedit_debug(DEBUG_COMMANDS);

    if (response_id != GTK_RESPONSE_OK) {
        bedit_file_chooser_dialog_destroy(dialog);
        return;
    }

    files = bedit_file_chooser_dialog_get_files(dialog);
    g_return_if_fail(files != NULL);

    encoding = bedit_file_chooser_dialog_get_encoding(dialog);

    bedit_file_chooser_dialog_destroy(dialog);

    if (window == NULL) {
        window = bedit_app_create_window(
            BEDIT_APP(g_application_get_default()), NULL);

        gtk_widget_show(GTK_WIDGET(window));
        gtk_window_present(GTK_WINDOW(window));
    }

    /* Remember the folder we navigated to */
    _bedit_window_set_default_location(window, files->data);

    loaded = bedit_commands_load_locations(window, files, encoding, 0, 0);

    g_slist_free(loaded);
    g_slist_free_full(files, g_object_unref);
}

void _bedit_cmd_file_open(
    GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    BeditWindow *window = NULL;
    BeditFileChooserDialog *open_dialog;

    if (BEDIT_IS_WINDOW(user_data)) {
        window = user_data;
    }

    bedit_debug(DEBUG_COMMANDS);

    if (window != NULL) {
        gpointer data;

        data = g_object_get_data(G_OBJECT(window), BEDIT_OPEN_DIALOG_KEY);

        if (data != NULL) {
            g_return_if_fail(BEDIT_IS_FILE_CHOOSER_DIALOG(data));

            gtk_window_present(GTK_WINDOW(data));

            return;
        }
    }

    /* Translators: "Open" is the title of the file chooser window. */
    open_dialog = bedit_file_chooser_dialog_create(
        C_("window title", "Open"), window != NULL ? GTK_WINDOW(window) : NULL,
        BEDIT_FILE_CHOOSER_OPEN | BEDIT_FILE_CHOOSER_ENABLE_ENCODING |
            BEDIT_FILE_CHOOSER_ENABLE_DEFAULT_FILTERS,
        NULL, _("_Cancel"), GTK_RESPONSE_CANCEL, _("_Open"), GTK_RESPONSE_OK);

    if (window != NULL) {
        BeditDocument *doc;
        GFile *default_path = NULL;

        /* The file chooser dialog for opening files is not modal, so
         * ensure that at most one file chooser is opened.
         */
        g_object_set_data(G_OBJECT(window), BEDIT_OPEN_DIALOG_KEY, open_dialog);

        g_object_weak_ref(
            G_OBJECT(open_dialog), (GWeakNotify)open_dialog_destroyed, window);

        /* Set the current folder */
        doc = bedit_window_get_active_document(window);

        if (doc != NULL) {
            GtkSourceFile *file = bedit_document_get_file(doc);
            GFile *location = gtk_source_file_get_location(file);

            if (location != NULL) {
                default_path = g_file_get_parent(location);
            }
        }

        if (default_path == NULL) {
            default_path = _bedit_window_get_default_location(window);
        }

        if (default_path != NULL) {
            bedit_file_chooser_dialog_set_current_folder(
                open_dialog, default_path);
            g_object_unref(default_path);
        }
    }

    g_signal_connect(
        open_dialog, "response", G_CALLBACK(open_dialog_response_cb), window);

    bedit_file_chooser_dialog_show(open_dialog);
}

void _bedit_cmd_file_reopen_closed_tab(
    GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    BeditWindow *window = BEDIT_WINDOW(user_data);
    GFile *file;

    file = _bedit_window_pop_last_closed_doc(window);
    if (file != NULL) {
        bedit_commands_load_location(window, file, NULL, 0, 0);
    }
}

/* File saving */

/* FIXME: modify this dialog to be similar to the one provided by gtk+ for
 * already existing files - Paolo (Oct. 11, 2005) */
static gboolean replace_read_only_file(GtkWindow *parent, GFile *file) {
    GtkWidget *dialog;
    gint ret;
    gchar *parse_name;
    gchar *name_for_display;

    bedit_debug(DEBUG_COMMANDS);

    parse_name = g_file_get_parse_name(file);

    /* Truncate the name so it doesn't get insanely wide. Note that even
     * though the dialog uses wrapped text, if the name doesn't contain
     * white space then the text-wrapping code is too stupid to wrap it.
     */
    name_for_display = bedit_utils_str_middle_truncate(parse_name, 50);
    g_free(parse_name);

    dialog = gtk_message_dialog_new(
        parent, GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_QUESTION,
        GTK_BUTTONS_NONE, _("The file “%s” is read-only."), name_for_display);
    g_free(name_for_display);

    gtk_message_dialog_format_secondary_text(
        GTK_MESSAGE_DIALOG(dialog),
        _("Do you want to try to replace it "
          "with the one you are saving?"));

    gtk_dialog_add_buttons(
        GTK_DIALOG(dialog), _("_Cancel"), GTK_RESPONSE_CANCEL, _("_Replace"),
        GTK_RESPONSE_YES, NULL);

    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_CANCEL);

    gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);

    ret = gtk_dialog_run(GTK_DIALOG(dialog));

    gtk_widget_destroy(dialog);

    return ret == GTK_RESPONSE_YES;
}

static gboolean change_compression(
    GtkWindow *parent, GFile *file, gboolean compressed) {
    GtkWidget *dialog;
    gint ret;
    gchar *parse_name;
    gchar *name_for_display;
    const gchar *primary_message;
    const gchar *button_label;

    bedit_debug(DEBUG_COMMANDS);

    parse_name = g_file_get_parse_name(file);

    /* Truncate the name so it doesn't get insanely wide. Note that even
     * though the dialog uses wrapped text, if the name doesn't contain
     * white space then the text-wrapping code is too stupid to wrap it.
     */
    name_for_display = bedit_utils_str_middle_truncate(parse_name, 50);
    g_free(parse_name);

    if (compressed) {
        primary_message = _("Save the file using compression?");
    } else {
        primary_message = _("Save the file as plain text?");
    }

    dialog = gtk_message_dialog_new(
        parent, GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_QUESTION,
        GTK_BUTTONS_NONE, "%s", primary_message);

    if (compressed) {
        gtk_message_dialog_format_secondary_text(
            GTK_MESSAGE_DIALOG(dialog),
            _("The file “%s” was previously saved as plain "
              "text and will now be saved using compression."),
            name_for_display);

        button_label = _("_Save Using Compression");
    } else {
        gtk_message_dialog_format_secondary_text(
            GTK_MESSAGE_DIALOG(dialog),
            _("The file “%s” was previously saved "
              "using compression and will now be saved as plain text."),
            name_for_display);
        button_label = _("_Save As Plain Text");
    }

    g_free(name_for_display);

    gtk_dialog_add_buttons(
        GTK_DIALOG(dialog), _("_Cancel"), GTK_RESPONSE_CANCEL, button_label,
        GTK_RESPONSE_YES, NULL);

    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_CANCEL);

    gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);

    ret = gtk_dialog_run(GTK_DIALOG(dialog));

    gtk_widget_destroy(dialog);

    return ret == GTK_RESPONSE_YES;
}

static GtkSourceCompressionType get_compression_type_from_file(GFile *file) {
    gchar *name;
    gchar *content_type;
    GtkSourceCompressionType type;

    name = g_file_get_basename(file);
    content_type = g_content_type_guess(name, NULL, 0, NULL);

    type = bedit_utils_get_compression_type_from_content_type(content_type);

    g_free(name);
    g_free(content_type);

    return type;
}

static void tab_save_as_ready_cb(
    BeditTab *tab, GAsyncResult *result, GTask *task) {
    gboolean success = _bedit_tab_save_finish(tab, result);
    g_task_return_boolean(task, success);
    g_object_unref(task);
}

static void save_dialog_response_cb(
    BeditFileChooserDialog *dialog, gint response_id, GTask *task) {
    BeditTab *tab;
    BeditWindow *window;
    BeditDocument *doc;
    GtkSourceFile *file;
    GFile *location;
    gchar *parse_name;
    GtkSourceNewlineType newline_type;
    GtkSourceCompressionType compression_type;
    GtkSourceCompressionType current_compression_type;
    const GtkSourceEncoding *encoding;

    bedit_debug(DEBUG_COMMANDS);

    tab = g_task_get_source_object(task);
    window = g_task_get_task_data(task);

    if (response_id != GTK_RESPONSE_OK) {
        bedit_file_chooser_dialog_destroy(dialog);
        g_task_return_boolean(task, FALSE);
        g_object_unref(task);
        return;
    }

    doc = bedit_tab_get_document(tab);
    file = bedit_document_get_file(doc);

    location = bedit_file_chooser_dialog_get_file(dialog);
    g_return_if_fail(location != NULL);

    compression_type = get_compression_type_from_file(location);
    current_compression_type = gtk_source_file_get_compression_type(file);

    if ((compression_type == GTK_SOURCE_COMPRESSION_TYPE_NONE) !=
        (current_compression_type == GTK_SOURCE_COMPRESSION_TYPE_NONE)) {
        GtkWindow *dialog_window = bedit_file_chooser_dialog_get_window(dialog);

        if (!change_compression(
                dialog_window, location,
                compression_type != GTK_SOURCE_COMPRESSION_TYPE_NONE)) {
            bedit_file_chooser_dialog_destroy(dialog);
            g_object_unref(location);

            g_task_return_boolean(task, FALSE);
            g_object_unref(task);
            return;
        }
    }

    encoding = bedit_file_chooser_dialog_get_encoding(dialog);
    newline_type = bedit_file_chooser_dialog_get_newline_type(dialog);

    bedit_file_chooser_dialog_destroy(dialog);

    parse_name = g_file_get_parse_name(location);

    bedit_statusbar_flash_message(
        BEDIT_STATUSBAR(window->priv->statusbar),
        window->priv->generic_message_cid, _("Saving file “%s”\342\200\246"),
        parse_name);

    g_free(parse_name);

    /* Let's remember the dir we navigated to, even if the saving fails... */
    _bedit_window_set_default_location(window, location);

    _bedit_tab_save_as_async(
        tab, location, encoding, newline_type, compression_type,
        g_task_get_cancellable(task), (GAsyncReadyCallback)tab_save_as_ready_cb,
        task);

    g_object_unref(location);
}

static GtkFileChooserConfirmation confirm_overwrite_callback(
    BeditFileChooserDialog *dialog, gpointer data) {
    GtkFileChooserConfirmation res;
    GFile *file;
    GFileInfo *info;

    bedit_debug(DEBUG_COMMANDS);

    /* fall back to the default confirmation dialog */
    res = GTK_FILE_CHOOSER_CONFIRMATION_CONFIRM;

    file = bedit_file_chooser_dialog_get_file(dialog);

    info = g_file_query_info(
        file, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE, G_FILE_QUERY_INFO_NONE, NULL,
        NULL);

    if (info != NULL) {
        if (g_file_info_has_attribute(
                info, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE) &&
            !g_file_info_get_attribute_boolean(
                info, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE)) {
            GtkWindow *win;

            win = bedit_file_chooser_dialog_get_window(dialog);

            if (replace_read_only_file(win, file)) {
                res = GTK_FILE_CHOOSER_CONFIRMATION_ACCEPT_FILENAME;
            } else {
                res = GTK_FILE_CHOOSER_CONFIRMATION_SELECT_AGAIN;
            }
        }

        g_object_unref(info);
    }

    g_object_unref(file);

    return res;
}

/* Call save_as_tab_finish() in @callback. */
static void save_as_tab_async(
    BeditTab *tab, BeditWindow *window, GCancellable *cancellable,
    GAsyncReadyCallback callback, gpointer user_data) {
    GTask *task;
    BeditFileChooserDialog *save_dialog;
    GtkWindowGroup *window_group;
    GtkWindow *dialog_window;
    BeditDocument *doc;
    GtkSourceFile *file;
    GFile *location;
    const GtkSourceEncoding *encoding;
    GtkSourceNewlineType newline_type;

    g_return_if_fail(BEDIT_IS_TAB(tab));
    g_return_if_fail(BEDIT_IS_WINDOW(window));

    bedit_debug(DEBUG_COMMANDS);

    task = g_task_new(tab, cancellable, callback, user_data);
    g_task_set_task_data(task, g_object_ref(window), g_object_unref);

    /* Translators: "Save As" is the title of the file chooser window. */
    save_dialog = bedit_file_chooser_dialog_create(
        C_("window title", "Save As"), GTK_WINDOW(window),
        BEDIT_FILE_CHOOSER_SAVE | BEDIT_FILE_CHOOSER_ENABLE_ENCODING |
            BEDIT_FILE_CHOOSER_ENABLE_LINE_ENDING |
            BEDIT_FILE_CHOOSER_ENABLE_DEFAULT_FILTERS,
        NULL, _("_Cancel"), GTK_RESPONSE_CANCEL, _("_Save"), GTK_RESPONSE_OK);

    bedit_file_chooser_dialog_set_do_overwrite_confirmation(save_dialog, TRUE);

    g_signal_connect(
        save_dialog, "confirm-overwrite",
        G_CALLBACK(confirm_overwrite_callback), NULL);

    window_group = bedit_window_get_group(window);

    dialog_window = bedit_file_chooser_dialog_get_window(save_dialog);

    if (dialog_window != NULL) {
        gtk_window_group_add_window(window_group, dialog_window);
    }

    /* Save As dialog is modal to its main window */
    bedit_file_chooser_dialog_set_modal(save_dialog, TRUE);

    /* Set the suggested file name */
    doc = bedit_tab_get_document(tab);
    file = bedit_document_get_file(doc);
    location = gtk_source_file_get_location(file);

    if (location != NULL) {
        bedit_file_chooser_dialog_set_file(save_dialog, location);
    } else {
        GFile *default_path;
        gchar *docname;

        default_path = _bedit_window_get_default_location(window);
        docname = bedit_document_get_short_name_for_display(doc);

        if (default_path != NULL) {
            bedit_file_chooser_dialog_set_current_folder(
                save_dialog, default_path);

            g_object_unref(default_path);
        }

        bedit_file_chooser_dialog_set_current_name(save_dialog, docname);

        g_free(docname);
    }

    /* Set suggested encoding and newline type. */
    encoding = gtk_source_file_get_encoding(file);

    if (encoding == NULL) {
        encoding = gtk_source_encoding_get_utf8();
    }

    newline_type = gtk_source_file_get_newline_type(file);

    bedit_file_chooser_dialog_set_encoding(
        BEDIT_FILE_CHOOSER_DIALOG(save_dialog), encoding);

    bedit_file_chooser_dialog_set_newline_type(
        BEDIT_FILE_CHOOSER_DIALOG(save_dialog), newline_type);

    g_signal_connect(
        save_dialog, "response", G_CALLBACK(save_dialog_response_cb), task);

    bedit_file_chooser_dialog_show(save_dialog);
}

static gboolean save_as_tab_finish(BeditTab *tab, GAsyncResult *result) {
    g_return_val_if_fail(g_task_is_valid(result, tab), FALSE);

    return g_task_propagate_boolean(G_TASK(result), NULL);
}

static void save_as_tab_ready_cb(
    BeditTab *tab, GAsyncResult *result, GTask *task) {
    gboolean success = save_as_tab_finish(tab, result);

    g_task_return_boolean(task, success);
    g_object_unref(task);
}

static void tab_save_ready_cb(
    BeditTab *tab, GAsyncResult *result, GTask *task) {
    gboolean success = _bedit_tab_save_finish(tab, result);

    g_task_return_boolean(task, success);
    g_object_unref(task);
}

/**
 * bedit_commands_save_document_async:
 * @document: the #BeditDocument to save.
 * @window: a #BeditWindow.
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @callback: (scope async): a #GAsyncReadyCallback to call when the operation
 *   is finished.
 * @user_data: (closure): the data to pass to the @callback function.
 *
 * Asynchronously save the @document. @document must belong to @window. The
 * source object of the async task is @document (which will be the first
 * parameter of the #GAsyncReadyCallback).
 *
 * When the operation is finished, @callback will be called. You can then call
 * bedit_commands_save_document_finish() to get the result of the operation.
 *
 * Since: 3.14
 */
void bedit_commands_save_document_async(
    BeditDocument *document, BeditWindow *window, GCancellable *cancellable,
    GAsyncReadyCallback callback, gpointer user_data) {
    GTask *task;
    BeditTab *tab;
    GtkSourceFile *file;
    gchar *uri_for_display;

    bedit_debug(DEBUG_COMMANDS);

    g_return_if_fail(BEDIT_IS_DOCUMENT(document));
    g_return_if_fail(BEDIT_IS_WINDOW(window));
    g_return_if_fail(cancellable == NULL || G_IS_CANCELLABLE(cancellable));

    task = g_task_new(document, cancellable, callback, user_data);

    tab = bedit_tab_get_from_document(document);
    file = bedit_document_get_file(document);

    if (bedit_document_is_untitled(document) ||
        gtk_source_file_is_readonly(file)) {
        bedit_debug_message(DEBUG_COMMANDS, "Untitled or Readonly");

        save_as_tab_async(
            tab, window, cancellable, (GAsyncReadyCallback)save_as_tab_ready_cb,
            task);
        return;
    }

    uri_for_display = bedit_document_get_uri_for_display(document);
    bedit_statusbar_flash_message(
        BEDIT_STATUSBAR(window->priv->statusbar),
        window->priv->generic_message_cid, _("Saving file “%s”\342\200\246"),
        uri_for_display);

    g_free(uri_for_display);

    _bedit_tab_save_async(
        tab, cancellable, (GAsyncReadyCallback)tab_save_ready_cb, task);
}

/**
 * bedit_commands_save_document_finish:
 * @document: a #BeditDocument.
 * @result: a #GAsyncResult.
 *
 * Finishes an asynchronous document saving operation started with
 * bedit_commands_save_document_async().
 *
 * Note that there is no error parameter because the errors are already handled
 * by bedit.
 *
 * Returns: %TRUE if the document has been correctly saved, %FALSE otherwise.
 * Since: 3.14
 */
gboolean bedit_commands_save_document_finish(
    BeditDocument *document, GAsyncResult *result) {
    g_return_val_if_fail(g_task_is_valid(result, document), FALSE);

    return g_task_propagate_boolean(G_TASK(result), NULL);
}

static void save_tab_ready_cb(
    BeditDocument *doc, GAsyncResult *result, gpointer user_data) {
    bedit_commands_save_document_finish(doc, result);
}

/* Save tab asynchronously, but without results. */
static void save_tab(BeditTab *tab, BeditWindow *window) {
    BeditDocument *doc = bedit_tab_get_document(tab);

    bedit_commands_save_document_async(
        doc, window, NULL, (GAsyncReadyCallback)save_tab_ready_cb, NULL);
}

void _bedit_cmd_file_save(
    GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    BeditWindow *window = BEDIT_WINDOW(user_data);
    BeditTab *tab;

    bedit_debug(DEBUG_COMMANDS);

    tab = bedit_window_get_active_tab(window);
    if (tab != NULL) {
        save_tab(tab, window);
    }
}

static void _bedit_cmd_file_save_as_cb(
    BeditTab *tab, GAsyncResult *result, gpointer user_data) {
    save_as_tab_finish(tab, result);
}

void _bedit_cmd_file_save_as(
    GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    BeditWindow *window = BEDIT_WINDOW(user_data);
    BeditTab *tab;

    bedit_debug(DEBUG_COMMANDS);

    tab = bedit_window_get_active_tab(window);
    if (tab != NULL) {
        save_as_tab_async(
            tab, window, NULL, (GAsyncReadyCallback)_bedit_cmd_file_save_as_cb,
            NULL);
    }
}

static void quit_if_needed(BeditWindow *window) {
    gboolean is_quitting;
    gboolean is_quitting_all;

    is_quitting = GPOINTER_TO_BOOLEAN(
        g_object_get_data(G_OBJECT(window), BEDIT_IS_QUITTING));

    is_quitting_all = GPOINTER_TO_BOOLEAN(
        g_object_get_data(G_OBJECT(window), BEDIT_IS_QUITTING_ALL));

    if (is_quitting) {
        gtk_widget_destroy(GTK_WIDGET(window));
    }

    if (is_quitting_all) {
        GtkApplication *app;

        app = GTK_APPLICATION(g_application_get_default());

        if (gtk_application_get_windows(app) == NULL) {
            g_application_quit(G_APPLICATION(app));
        }
    }
}

static gboolean really_close_tab(BeditTab *tab) {
    GtkWidget *toplevel;
    BeditWindow *window;

    bedit_debug(DEBUG_COMMANDS);

    g_return_val_if_fail(
        bedit_tab_get_state(tab) == BEDIT_TAB_STATE_CLOSING, FALSE);

    toplevel = gtk_widget_get_toplevel(GTK_WIDGET(tab));
    g_return_val_if_fail(BEDIT_IS_WINDOW(toplevel), FALSE);

    window = BEDIT_WINDOW(toplevel);

    bedit_window_close_tab(window, tab);

    if (bedit_window_get_active_tab(window) == NULL) {
        quit_if_needed(window);
    }

    return FALSE;
}

static void close_tab(BeditTab *tab) {
    BeditDocument *doc;

    doc = bedit_tab_get_document(tab);
    g_return_if_fail(doc != NULL);

    /* If the user has modified again the document, do not close the tab. */
    if (_bedit_document_needs_saving(doc)) {
        return;
    }

    /* Close the document only if it has been succesfully saved.
     * Tab state is set to CLOSING (it is a state without exiting
     * transitions) and the tab is closed in an idle handler.
     */
    _bedit_tab_mark_for_closing(tab);

    g_idle_add_full(
        G_PRIORITY_HIGH_IDLE, (GSourceFunc)really_close_tab, tab, NULL);
}

typedef struct _SaveAsData SaveAsData;

struct _SaveAsData {
    /* Reffed */
    BeditWindow *window;

    /* List of reffed BeditTab's */
    GSList *tabs_to_save_as;

    guint close_tabs : 1;
};

static void save_as_documents_list(SaveAsData *data);

static void save_as_documents_list_cb(
    BeditTab *tab, GAsyncResult *result, SaveAsData *data) {
    gboolean saved = save_as_tab_finish(tab, result);

    if (saved && data->close_tabs) {
        close_tab(tab);
    }

    g_return_if_fail(tab == BEDIT_TAB(data->tabs_to_save_as->data));
    g_object_unref(data->tabs_to_save_as->data);
    data->tabs_to_save_as =
        g_slist_delete_link(data->tabs_to_save_as, data->tabs_to_save_as);

    if (data->tabs_to_save_as != NULL) {
        save_as_documents_list(data);
    } else {
        g_object_unref(data->window);
        g_slice_free(SaveAsData, data);
    }
}

static void save_as_documents_list(SaveAsData *data) {
    BeditTab *next_tab = BEDIT_TAB(data->tabs_to_save_as->data);

    bedit_window_set_active_tab(data->window, next_tab);

    save_as_tab_async(
        next_tab, data->window, NULL,
        (GAsyncReadyCallback)save_as_documents_list_cb, data);
}

/*
 * The docs in the list must belong to the same BeditWindow.
 */
static void save_documents_list(BeditWindow *window, GList *docs) {
    SaveAsData *data = NULL;
    GList *l;

    bedit_debug(DEBUG_COMMANDS);

    g_return_if_fail(
        (bedit_window_get_state(window) & BEDIT_WINDOW_STATE_PRINTING) == 0);

    for (l = docs; l != NULL; l = l->next) {
        BeditDocument *doc;
        BeditTab *tab;
        BeditTabState state;

        g_return_if_fail(BEDIT_IS_DOCUMENT(l->data));
        doc = l->data;

        tab = bedit_tab_get_from_document(doc);
        state = bedit_tab_get_state(tab);

        g_return_if_fail(state != BEDIT_TAB_STATE_PRINTING);
        g_return_if_fail(state != BEDIT_TAB_STATE_CLOSING);

        if (state == BEDIT_TAB_STATE_NORMAL ||
            state == BEDIT_TAB_STATE_SHOWING_PRINT_PREVIEW) {
            if (_bedit_document_needs_saving(doc)) {
                GtkSourceFile *file = bedit_document_get_file(doc);

                /* FIXME: manage the case of local readonly files owned by the
                   user is running bedit - Paolo (Dec. 8, 2005) */
                if (bedit_document_is_untitled(doc) ||
                    gtk_source_file_is_readonly(file)) {
                    if (data == NULL) {
                        data = g_slice_new(SaveAsData);
                        data->window = g_object_ref(window);
                        data->tabs_to_save_as = NULL;
                        data->close_tabs = FALSE;
                    }

                    data->tabs_to_save_as = g_slist_prepend(
                        data->tabs_to_save_as, g_object_ref(tab));
                } else {
                    save_tab(tab, window);
                }
            }
        } else {
            /* If the state is:
               - BEDIT_TAB_STATE_LOADING: we do not save since we are sure the
               file is unmodified
               - BEDIT_TAB_STATE_REVERTING: we do not save since the user wants
                 to return back to the version of the file she previously saved
               - BEDIT_TAB_STATE_SAVING: well, we are already saving (no need to
               save again)
               - BEDIT_TAB_STATE_PRINTING: there is not a
                 real reason for not saving in this case, we do not save to
               avoid to run two operations using the message area at the same
               time (may be we can remove this limitation in the future). Note
               that SaveAll, ClosAll and Quit are unsensitive if the window
               state is PRINTING.
               - BEDIT_TAB_STATE_GENERIC_ERROR: we do not save since the
               document contains errors (I don't think this is a very frequent
               case, we should probably remove this state)
               - BEDIT_TAB_STATE_LOADING_ERROR: there is nothing to save
               - BEDIT_TAB_STATE_REVERTING_ERROR: there is nothing to save and
               saving the current document will overwrite the copy of the file
               the user wants to go back to
               - BEDIT_TAB_STATE_SAVING_ERROR: we do not save since we just
               failed to save, so there is no reason to automatically retry...
               we wait for user intervention
               - BEDIT_TAB_STATE_CLOSING: this state is invalid in this case
            */

            gchar *uri_for_display;

            uri_for_display = bedit_document_get_uri_for_display(doc);
            bedit_debug_message(
                DEBUG_COMMANDS, "File '%s' not saved. State: %d",
                uri_for_display, state);
            g_free(uri_for_display);
        }
    }

    if (data != NULL) {
        data->tabs_to_save_as = g_slist_reverse(data->tabs_to_save_as);
        save_as_documents_list(data);
    }
}

/**
 * bedit_commands_save_all_documents:
 * @window: a #BeditWindow.
 *
 * Asynchronously save all documents belonging to @window. The result of the
 * operation is not available, so it's difficult to know whether all the
 * documents are correctly saved.
 */
void bedit_commands_save_all_documents(BeditWindow *window) {
    GList *docs;

    g_return_if_fail(BEDIT_IS_WINDOW(window));

    bedit_debug(DEBUG_COMMANDS);

    docs = bedit_window_get_documents(window);

    save_documents_list(window, docs);

    g_list_free(docs);
}

void _bedit_cmd_file_save_all(
    GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    bedit_commands_save_all_documents(BEDIT_WINDOW(user_data));
}

/**
 * bedit_commands_save_document:
 * @window: a #BeditWindow.
 * @document: the #BeditDocument to save.
 *
 * Asynchronously save @document. @document must belong to @window. If you need
 * the result of the operation, use bedit_commands_save_document_async().
 */
void bedit_commands_save_document(
    BeditWindow *window, BeditDocument *document) {
    BeditTab *tab;

    g_return_if_fail(BEDIT_IS_WINDOW(window));
    g_return_if_fail(BEDIT_IS_DOCUMENT(document));

    bedit_debug(DEBUG_COMMANDS);

    tab = bedit_tab_get_from_document(document);
    save_tab(tab, window);
}

/* File revert */
static void do_revert(BeditWindow *window, BeditTab *tab) {
    BeditDocument *doc;
    gchar *docname;

    bedit_debug(DEBUG_COMMANDS);

    doc = bedit_tab_get_document(tab);
    docname = bedit_document_get_short_name_for_display(doc);

    bedit_statusbar_flash_message(
        BEDIT_STATUSBAR(window->priv->statusbar),
        window->priv->generic_message_cid,
        _("Reverting the document “%s”\342\200\246"), docname);

    g_free(docname);

    _bedit_tab_revert(tab);
}

static void revert_dialog_response_cb(
    GtkDialog *dialog, gint response_id, BeditWindow *window) {
    BeditTab *tab;

    bedit_debug(DEBUG_COMMANDS);

    /* FIXME: we are relying on the fact that the dialog is
       modal so the active tab can't be changed...
       not very nice - Paolo (Oct 11, 2005) */
    tab = bedit_window_get_active_tab(window);
    if (tab == NULL) {
        return;
    }

    gtk_widget_destroy(GTK_WIDGET(dialog));

    if (response_id == GTK_RESPONSE_OK) {
        do_revert(window, tab);
    }
}

static GtkWidget *revert_dialog(BeditWindow *window, BeditDocument *doc) {
    GtkWidget *dialog;
    gchar *docname;
    gchar *primary_msg;
    gchar *secondary_msg;
    glong seconds;

    bedit_debug(DEBUG_COMMANDS);

    docname = bedit_document_get_short_name_for_display(doc);
    primary_msg =
        g_strdup_printf(_("Revert unsaved changes to document “%s”?"), docname);
    g_free(docname);

    seconds = MAX(1, _bedit_document_get_seconds_since_last_save_or_load(doc));

    if (seconds < 55) {
        secondary_msg = g_strdup_printf(
            ngettext(
                "Changes made to the document in the last %ld second "
                "will be permanently lost.",
                "Changes made to the document in the last %ld seconds "
                "will be permanently lost.",
                seconds),
            seconds);
    } else if (seconds < 75) /* 55 <= seconds < 75 */
    {
        secondary_msg =
            g_strdup(_("Changes made to the document in the last minute "
                       "will be permanently lost."));
    } else if (seconds < 110) /* 75 <= seconds < 110 */
    {
        secondary_msg = g_strdup_printf(
            ngettext(
                "Changes made to the document in the last minute and "
                "%ld second will be permanently lost.",
                "Changes made to the document in the last minute and "
                "%ld seconds will be permanently lost.",
                seconds - 60),
            seconds - 60);
    } else if (seconds < 3600) {
        secondary_msg = g_strdup_printf(
            ngettext(
                "Changes made to the document in the last %ld minute "
                "will be permanently lost.",
                "Changes made to the document in the last %ld minutes "
                "will be permanently lost.",
                seconds / 60),
            seconds / 60);
    } else if (seconds < 7200) {
        gint minutes;
        seconds -= 3600;

        minutes = seconds / 60;
        if (minutes < 5) {
            secondary_msg =
                g_strdup(_("Changes made to the document in the last hour "
                           "will be permanently lost."));
        } else {
            secondary_msg = g_strdup_printf(
                ngettext(
                    "Changes made to the document in the last hour and "
                    "%d minute will be permanently lost.",
                    "Changes made to the document in the last hour and "
                    "%d minutes will be permanently lost.",
                    minutes),
                minutes);
        }
    } else {
        gint hours;

        hours = seconds / 3600;

        secondary_msg = g_strdup_printf(
            ngettext(
                "Changes made to the document in the last %d hour "
                "will be permanently lost.",
                "Changes made to the document in the last %d hours "
                "will be permanently lost.",
                hours),
            hours);
    }

    dialog = gtk_message_dialog_new(
        GTK_WINDOW(window), GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE, "%s", primary_msg);

    gtk_message_dialog_format_secondary_text(
        GTK_MESSAGE_DIALOG(dialog), "%s", secondary_msg);
    g_free(primary_msg);
    g_free(secondary_msg);

    gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);

    gtk_dialog_add_buttons(
        GTK_DIALOG(dialog), _("_Cancel"), GTK_RESPONSE_CANCEL, _("_Revert"),
        GTK_RESPONSE_OK, NULL);

    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_CANCEL);

    return dialog;
}

void _bedit_cmd_file_revert(
    GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    BeditWindow *window = BEDIT_WINDOW(user_data);
    BeditTab *tab;
    BeditDocument *doc;
    GtkWidget *dialog;
    GtkWindowGroup *window_group;

    bedit_debug(DEBUG_COMMANDS);

    tab = bedit_window_get_active_tab(window);
    g_return_if_fail(tab != NULL);

    /* If we are already displaying a notification reverting will drop local
     * modifications or if the document has not been modified, do not bug
     * the user further.
     */
    if (bedit_tab_get_state(tab) ==
            BEDIT_TAB_STATE_EXTERNALLY_MODIFIED_NOTIFICATION ||
        _bedit_tab_get_can_close(tab)) {
        do_revert(window, tab);
        return;
    }

    doc = bedit_tab_get_document(tab);
    g_return_if_fail(doc != NULL);
    g_return_if_fail(!bedit_document_is_untitled(doc));

    dialog = revert_dialog(window, doc);

    window_group = bedit_window_get_group(window);

    gtk_window_group_add_window(window_group, GTK_WINDOW(dialog));

    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);

    g_signal_connect(
        dialog, "response", G_CALLBACK(revert_dialog_response_cb), window);

    gtk_widget_show(dialog);
}

static void tab_state_changed_while_saving(
    BeditTab *tab, GParamSpec *pspec, BeditWindow *window) {
    BeditTabState state;

    state = bedit_tab_get_state(tab);

    bedit_debug_message(DEBUG_COMMANDS, "State while saving: %d\n", state);

    /* When the state becomes NORMAL, it means the saving operation is
     * finished.
     */
    if (state == BEDIT_TAB_STATE_NORMAL) {
        g_signal_handlers_disconnect_by_func(
            tab, G_CALLBACK(tab_state_changed_while_saving), window);

        close_tab(tab);
    }
}

static void save_and_close(BeditTab *tab, BeditWindow *window) {
    bedit_debug(DEBUG_COMMANDS);

    /* Trace tab state changes */
    g_signal_connect(
        tab, "notify::state", G_CALLBACK(tab_state_changed_while_saving),
        window);

    save_tab(tab, window);
}

static void save_and_close_documents(
    GList *docs, BeditWindow *window, BeditNotebook *notebook) {
    GList *tabs;
    GList *l;
    GSList *sl;
    SaveAsData *data = NULL;
    GSList *tabs_to_save_and_close = NULL;
    GList *tabs_to_close = NULL;

    bedit_debug(DEBUG_COMMANDS);

    g_return_if_fail(
        (bedit_window_get_state(window) & BEDIT_WINDOW_STATE_PRINTING) == 0);

    if (notebook != NULL) {
        tabs = gtk_container_get_children(GTK_CONTAINER(notebook));
    } else {
        tabs = _bedit_window_get_all_tabs(window);
    }

    for (l = tabs; l != NULL; l = l->next) {
        BeditTab *tab = BEDIT_TAB(l->data);
        BeditTabState state;
        BeditDocument *doc;

        state = bedit_tab_get_state(tab);
        doc = bedit_tab_get_document(tab);

        /* If the state is: ([*] invalid states)
           - BEDIT_TAB_STATE_NORMAL: close (and if needed save)
           - BEDIT_TAB_STATE_LOADING: close, we are sure the file is unmodified
           - BEDIT_TAB_STATE_REVERTING: since the user wants
             to return back to the version of the file she previously saved, we
           can close without saving (CHECK: are we sure this is the right
           behavior, suppose the case the original file has been deleted)
           - [*] BEDIT_TAB_STATE_SAVING: invalid, ClosAll
             and Quit are unsensitive if the window state is SAVING.
           - [*] BEDIT_TAB_STATE_PRINTING: there is not a
             real reason for not closing in this case, we do not save to avoid
           to run two operations using the message area at the same time (may be
           we can remove this limitation in the future). Note that ClosAll and
           Quit are unsensitive if the window state is PRINTING.
           - BEDIT_TAB_STATE_SHOWING_PRINT_PREVIEW: close (and if needed save)
           - BEDIT_TAB_STATE_LOADING_ERROR: close without saving (if the state
           is LOADING_ERROR then the document is not modified)
           - BEDIT_TAB_STATE_REVERTING_ERROR: we do not close since the document
           contains errors
           - BEDIT_TAB_STATE_SAVING_ERROR: we do not close since the document
           contains errors
           - BEDIT_TAB_STATE_GENERIC_ERROR: we do not close since the document
           contains errors (CHECK: we should problably remove this state)
           - [*] BEDIT_TAB_STATE_CLOSING: this state is invalid in this case
        */

        g_return_if_fail(state != BEDIT_TAB_STATE_PRINTING);
        g_return_if_fail(state != BEDIT_TAB_STATE_CLOSING);
        g_return_if_fail(state != BEDIT_TAB_STATE_SAVING);

        if (state != BEDIT_TAB_STATE_SAVING_ERROR &&
            state != BEDIT_TAB_STATE_GENERIC_ERROR &&
            state != BEDIT_TAB_STATE_REVERTING_ERROR) {
            if (g_list_index(docs, doc) >= 0 &&
                state != BEDIT_TAB_STATE_LOADING &&
                state != BEDIT_TAB_STATE_LOADING_ERROR &&
                state !=
                    BEDIT_TAB_STATE_REVERTING) /* FIXME: is this the right
                                                  behavior with REVERTING ?*/
            {
                GtkSourceFile *file = bedit_document_get_file(doc);

                /* The document must be saved before closing */
                g_return_if_fail(_bedit_document_needs_saving(doc));

                /* FIXME: manage the case of local readonly files owned by the
                 * user is running bedit - Paolo (Dec. 8, 2005) */
                if (bedit_document_is_untitled(doc) ||
                    gtk_source_file_is_readonly(file)) {
                    if (data == NULL) {
                        data = g_slice_new(SaveAsData);
                        data->window = g_object_ref(window);
                        data->tabs_to_save_as = NULL;
                        data->close_tabs = TRUE;
                    }

                    data->tabs_to_save_as = g_slist_prepend(
                        data->tabs_to_save_as, g_object_ref(tab));
                } else {
                    tabs_to_save_and_close =
                        g_slist_prepend(tabs_to_save_and_close, tab);
                }
            } else {
                /* The document can be closed without saving */
                tabs_to_close = g_list_prepend(tabs_to_close, tab);
            }
        }
    }

    g_list_free(tabs);

    /* Close all tabs to close (in a sync way) */
    bedit_window_close_tabs(window, tabs_to_close);
    g_list_free(tabs_to_close);

    /* Save and close all the files in tabs_to_save_and_close */
    for (sl = tabs_to_save_and_close; sl != NULL; sl = sl->next) {
        save_and_close(BEDIT_TAB(sl->data), window);
    }

    g_slist_free(tabs_to_save_and_close);

    /* Save As and close all the files in data->tabs_to_save_as. */
    if (data != NULL) {
        data->tabs_to_save_as = g_slist_reverse(data->tabs_to_save_as);
        save_as_documents_list(data);
    }
}

static void save_and_close_document(const GList *docs, BeditWindow *window) {
    BeditTab *tab;

    bedit_debug(DEBUG_COMMANDS);

    g_return_if_fail(docs->next == NULL);

    tab = bedit_tab_get_from_document(BEDIT_DOCUMENT(docs->data));
    g_return_if_fail(tab != NULL);

    save_and_close(tab, window);
}

static void close_all_tabs(BeditWindow *window) {
    bedit_debug(DEBUG_COMMANDS);

    /* There is no document to save -> close all tabs */
    bedit_window_close_all_tabs(window);

    quit_if_needed(window);
}

static void close_document(BeditWindow *window, BeditDocument *doc) {
    BeditTab *tab;

    bedit_debug(DEBUG_COMMANDS);

    tab = bedit_tab_get_from_document(doc);
    g_return_if_fail(tab != NULL);

    bedit_window_close_tab(window, tab);
}

static void close_confirmation_dialog_response_handler(
    BeditCloseConfirmationDialog *dlg, gint response_id, BeditWindow *window) {
    GList *selected_documents;
    gboolean is_closing_all;
    BeditNotebook *notebook_to_close;

    bedit_debug(DEBUG_COMMANDS);

    is_closing_all = GPOINTER_TO_BOOLEAN(
        g_object_get_data(G_OBJECT(window), BEDIT_IS_CLOSING_ALL));

    notebook_to_close =
        g_object_get_data(G_OBJECT(window), BEDIT_NOTEBOOK_TO_CLOSE);

    gtk_widget_hide(GTK_WIDGET(dlg));

    switch (response_id) {
    /* Save and Close */
    case GTK_RESPONSE_YES:
        selected_documents =
            bedit_close_confirmation_dialog_get_selected_documents(dlg);

        if (selected_documents == NULL) {
            if (is_closing_all) {
                /* There is no document to save -> close all tabs */
                /* We call gtk_widget_destroy before close_all_tabs
                 * because close_all_tabs could destroy the bedit window */
                gtk_widget_destroy(GTK_WIDGET(dlg));

                close_all_tabs(window);
                return;
            } else if (notebook_to_close) {
                bedit_notebook_remove_all_tabs(notebook_to_close);
            } else {
                g_return_if_reached();
            }
        } else {
            if (is_closing_all || notebook_to_close) {
                BeditNotebook *notebook =
                    is_closing_all ? NULL : notebook_to_close;

                save_and_close_documents(selected_documents, window, notebook);
            } else {
                save_and_close_document(selected_documents, window);
            }
        }

        g_list_free(selected_documents);
        break;

    /* Close without Saving */
    case GTK_RESPONSE_NO:
        if (is_closing_all) {
            /* We call gtk_widget_destroy before close_all_tabs
             * because close_all_tabs could destroy the bedit window */
            gtk_widget_destroy(GTK_WIDGET(dlg));

            close_all_tabs(window);
            return;
        } else if (notebook_to_close) {
            bedit_notebook_remove_all_tabs(notebook_to_close);
        } else {
            const GList *unsaved_documents;

            unsaved_documents =
                bedit_close_confirmation_dialog_get_unsaved_documents(dlg);
            g_return_if_fail(unsaved_documents->next == NULL);

            close_document(window, BEDIT_DOCUMENT(unsaved_documents->data));
        }

        break;

    /* Do not close */
    default:
        /* Reset is_quitting flag */
        g_object_set_data(
            G_OBJECT(window), BEDIT_IS_QUITTING, GBOOLEAN_TO_POINTER(FALSE));

        g_object_set_data(
            G_OBJECT(window), BEDIT_IS_QUITTING_ALL,
            GBOOLEAN_TO_POINTER(FALSE));
        break;
    }

    g_object_set_data(G_OBJECT(window), BEDIT_NOTEBOOK_TO_CLOSE, NULL);

    gtk_widget_destroy(GTK_WIDGET(dlg));
}

/* Returns TRUE if the tab can be immediately closed */
static gboolean tab_can_close(BeditTab *tab, GtkWindow *window) {
    BeditDocument *doc;

    bedit_debug(DEBUG_COMMANDS);

    doc = bedit_tab_get_document(tab);

    if (!_bedit_tab_get_can_close(tab)) {
        GtkWidget *dlg;

        dlg = bedit_close_confirmation_dialog_new_single(window, doc);
        g_signal_connect(
            dlg, "response",
            G_CALLBACK(close_confirmation_dialog_response_handler), window);

        gtk_widget_show(dlg);

        return FALSE;
    }

    return TRUE;
}

/* FIXME: we probably need this one public for plugins...
 * maybe even a _list variant. Or maybe it's better make
 * bedit_window_close_tab always run the confirm dialog?
 * we should not allow closing a tab without resetting the
 * BEDIT_IS_CLOSING_ALL flag!
 */
void _bedit_cmd_file_close_tab(BeditTab *tab, BeditWindow *window) {
    bedit_debug(DEBUG_COMMANDS);

    g_return_if_fail(
        GTK_WIDGET(window) == gtk_widget_get_toplevel(GTK_WIDGET(tab)));

    g_object_set_data(
        G_OBJECT(window), BEDIT_IS_CLOSING_ALL, GBOOLEAN_TO_POINTER(FALSE));

    g_object_set_data(
        G_OBJECT(window), BEDIT_IS_QUITTING, GBOOLEAN_TO_POINTER(FALSE));

    g_object_set_data(
        G_OBJECT(window), BEDIT_IS_QUITTING_ALL, GBOOLEAN_TO_POINTER(FALSE));

    if (tab_can_close(tab, GTK_WINDOW(window))) {
        bedit_window_close_tab(window, tab);
    }
}

void _bedit_cmd_file_close(
    GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    BeditWindow *window = BEDIT_WINDOW(user_data);
    BeditTab *active_tab;

    bedit_debug(DEBUG_COMMANDS);

    active_tab = bedit_window_get_active_tab(window);

    if (active_tab == NULL) {
        gtk_widget_destroy(GTK_WIDGET(window));
        return;
    }

    _bedit_cmd_file_close_tab(active_tab, window);
}

static void file_close_dialog(BeditWindow *window, GList *unsaved_docs) {
    GtkWidget *dlg;

    if (unsaved_docs->next == NULL) {
        /* There is only one unsaved document */
        BeditTab *tab;
        BeditDocument *doc;

        doc = BEDIT_DOCUMENT(unsaved_docs->data);

        tab = bedit_tab_get_from_document(doc);
        g_return_if_fail(tab != NULL);

        bedit_window_set_active_tab(window, tab);

        dlg =
            bedit_close_confirmation_dialog_new_single(GTK_WINDOW(window), doc);
    } else {
        dlg = bedit_close_confirmation_dialog_new(
            GTK_WINDOW(window), unsaved_docs);
    }

    g_signal_connect(
        dlg, "response", G_CALLBACK(close_confirmation_dialog_response_handler),
        window);

    gtk_widget_show(dlg);
}

static GList *notebook_get_unsaved_documents(BeditNotebook *notebook) {
    GList *children;
    GList *unsaved_docs = NULL;
    GList *l;

    children = gtk_container_get_children(GTK_CONTAINER(notebook));

    for (l = children; l != NULL; l = g_list_next(l)) {
        BeditTab *tab = BEDIT_TAB(l->data);

        if (!_bedit_tab_get_can_close(tab)) {
            BeditDocument *doc;

            doc = bedit_tab_get_document(tab);
            unsaved_docs = g_list_prepend(unsaved_docs, doc);
        }
    }

    g_list_free(children);

    return g_list_reverse(unsaved_docs);
}

/* Close a notebook */
void _bedit_cmd_file_close_notebook(
    BeditWindow *window, BeditNotebook *notebook) {
    GList *unsaved_docs;

    g_object_set_data(
        G_OBJECT(window), BEDIT_IS_CLOSING_ALL, GBOOLEAN_TO_POINTER(FALSE));
    g_object_set_data(
        G_OBJECT(window), BEDIT_IS_QUITTING, GBOOLEAN_TO_POINTER(FALSE));
    g_object_set_data(
        G_OBJECT(window), BEDIT_IS_QUITTING_ALL, GBOOLEAN_TO_POINTER(FALSE));

    g_object_set_data(G_OBJECT(window), BEDIT_NOTEBOOK_TO_CLOSE, notebook);

    unsaved_docs = notebook_get_unsaved_documents(notebook);

    if (unsaved_docs == NULL) {
        /* There is no document to save -> close the notebook */
        bedit_notebook_remove_all_tabs(BEDIT_NOTEBOOK(notebook));
    } else {
        file_close_dialog(window, unsaved_docs);

        g_list_free(unsaved_docs);
    }
}

/* Close all tabs */
static void file_close_all(BeditWindow *window, gboolean is_quitting) {
    GList *unsaved_docs;

    bedit_debug(DEBUG_COMMANDS);

    g_return_if_fail(
        !(bedit_window_get_state(window) &
          (BEDIT_WINDOW_STATE_SAVING | BEDIT_WINDOW_STATE_PRINTING)));

    g_object_set_data(
        G_OBJECT(window), BEDIT_IS_CLOSING_ALL, GBOOLEAN_TO_POINTER(TRUE));

    g_object_set_data(
        G_OBJECT(window), BEDIT_IS_QUITTING, GBOOLEAN_TO_POINTER(is_quitting));

    unsaved_docs = bedit_window_get_unsaved_documents(window);

    if (unsaved_docs != NULL) {
        file_close_dialog(window, unsaved_docs);

        g_list_free(unsaved_docs);
    } else {
        /* There is no document to save -> close all tabs */
        bedit_window_close_all_tabs(window);
        quit_if_needed(window);
    }
}

void _bedit_cmd_file_close_all(
    GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    BeditWindow *window = BEDIT_WINDOW(user_data);

    bedit_debug(DEBUG_COMMANDS);

    g_return_if_fail(
        !(bedit_window_get_state(window) &
          (BEDIT_WINDOW_STATE_SAVING | BEDIT_WINDOW_STATE_PRINTING)));

    file_close_all(window, FALSE);
}

/* Quit */
static void quit_all(void) {
    GList *windows;
    GList *l;
    GApplication *app;

    app = g_application_get_default();
    windows = bedit_app_get_main_windows(BEDIT_APP(app));

    if (windows == NULL) {
        g_application_quit(app);
        return;
    }

    for (l = windows; l != NULL; l = g_list_next(l)) {
        BeditWindow *window = l->data;

        g_object_set_data(
            G_OBJECT(window), BEDIT_IS_QUITTING_ALL, GBOOLEAN_TO_POINTER(TRUE));

        if (!(bedit_window_get_state(window) &
              (BEDIT_WINDOW_STATE_SAVING | BEDIT_WINDOW_STATE_PRINTING))) {
            file_close_all(window, TRUE);
        }
    }

    g_list_free(windows);
}

void _bedit_cmd_file_quit(
    GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    BeditWindow *window = BEDIT_WINDOW(user_data);

    bedit_debug(DEBUG_COMMANDS);

    if (window == NULL) {
        quit_all();
        return;
    }

    g_return_if_fail(
        !(bedit_window_get_state(window) &
          (BEDIT_WINDOW_STATE_SAVING | BEDIT_WINDOW_STATE_PRINTING)));

    file_close_all(window, TRUE);
}

