/*
 * bedit-tab.c
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * Based on gedit-tab.c from Gedit.
 *
 * Copyright (C) 2005-2015 - Paolo Borelli
 * Copyright (C) 2006 - Bruno Boaventura, Frank Arnold, Paolo Maggi
 * Copyright (C) 2006-2010 - Steve Frécinaux
 * Copyright (C) 2007 - Christian Kirbach
 * Copyright (C) 2008-2010 - Jesse van den Kieboom
 * Copyright (C) 2008-2013 - Ignacio Casal Quinteiro
 * Copyright (C) 2009 - Marek Kašík
 * Copyright (C) 2010-2013 - Garrett Regier
 * Copyright (C) 2011 - Cosimo Cecchi
 * Copyright (C) 2012 - Timothy Arceri
 * Copyright (C) 2013 - Volker Sobek
 * Copyright (C) 2013-2019 - Sébastien Wilmet
 * Copyright (C) 2014 - Robert Roth, Sagar Ghuge
 * Copyright (C) 2019 - Andrea Azzarone, Michael Catanzaro
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

#include "bedit-tab.h"
#include "bedit-tab-private.h"

#include <glib/gi18n.h>
#include <stdlib.h>

#include "bedit-app-private.h"
#include "bedit-app.h"
#include "bedit-debug.h"
#include "bedit-document-private.h"
#include "bedit-document.h"
#include "bedit-enum-types.h"
#include "bedit-io-error-info-bar.h"
#include "bedit-print-job.h"
#include "bedit-print-preview.h"
#include "bedit-progress-info-bar.h"
#include "bedit-recent.h"
#include "bedit-settings.h"
#include "bedit-utils.h"
#include "bedit-view-frame.h"

#define BEDIT_TAB_KEY "BEDIT_TAB_KEY"

struct _BeditTab {
    GtkBox parent_instance;

    BeditTabState state;

    GSettings *editor_settings;

    BeditViewFrame *frame;

    GtkWidget *info_bar;
    GtkWidget *info_bar_hidden;

    BeditPrintJob *print_job;
    GtkWidget *print_preview;

    GtkSourceFileSaverFlags save_flags;

    guint idle_scroll;

    gint auto_save_interval;
    guint auto_save_timeout;

    GCancellable *cancellable;

    guint editable : 1;
    guint auto_save : 1;

    guint ask_if_externally_modified : 1;
};

typedef struct _SaverData SaverData;
typedef struct _LoaderData LoaderData;

struct _SaverData {
    GtkSourceFileSaver *saver;

    GTimer *timer;

    /* Notes about the create_backup saver flag:
     * - At the beginning of a new file saving, force_no_backup is FALSE.
     *   The create_backup flag is set to the saver if it is enabled in
     *   GSettings and if it isn't an auto-save.
     * - If creating the backup gives an error, and if the user wants to
     *   save the file without the backup, force_no_backup is set to TRUE
     *   and the create_backup flag is removed from the saver.
     *   force_no_backup as TRUE means that the create_backup flag should
     *   never be added again to the saver (for the current file saving).
     * - When another error occurs and if the user explicitly retry again
     *   the file saving, the create_backup flag is added to the saver if
     *   (1) it is enabled in GSettings, (2) if force_no_backup is FALSE.
     * - The create_backup flag is added when the user expressed his or her
     *   willing to save the file, by pressing a button for example. For an
     *   auto-save, the create_backup flag is thus not added initially, but
     *   can be added later when an error occurs and the user clicks on a
     *   button in the info bar to retry the file saving.
     */
    guint force_no_backup : 1;
};

struct _LoaderData {
    BeditTab *tab;
    GtkSourceFileLoader *loader;
    GTimer *timer;
    gint line_pos;
    gint column_pos;
    guint user_requested_encoding : 1;
};

G_DEFINE_TYPE(BeditTab, bedit_tab, GTK_TYPE_BOX)

enum {
    PROP_0,
    PROP_NAME,
    PROP_STATE,
    PROP_AUTO_SAVE,
    PROP_AUTO_SAVE_INTERVAL,
    PROP_CAN_CLOSE,
    LAST_PROP
};

static GParamSpec *properties[LAST_PROP];

enum { DROP_URIS, LAST_SIGNAL };

static guint signals[LAST_SIGNAL];

static gboolean bedit_tab_auto_save(BeditTab *tab);

static void launch_loader(
    GTask *loading_task, const GtkSourceEncoding *encoding
);

static void launch_saver(GTask *saving_task);

static SaverData *saver_data_new(void) {
    return g_slice_new0(SaverData);
}

static void saver_data_free(SaverData *data) {
    if (data != NULL) {
        if (data->saver != NULL) {
            g_object_unref(data->saver);
        }

        if (data->timer != NULL) {
            g_timer_destroy(data->timer);
        }

        g_slice_free(SaverData, data);
    }
}

static LoaderData *loader_data_new(void) {
    return g_slice_new0(LoaderData);
}

static void loader_data_free(LoaderData *data) {
    if (data != NULL) {
        if (data->loader != NULL) {
            g_object_unref(data->loader);
        }

        if (data->timer != NULL) {
            g_timer_destroy(data->timer);
        }

        g_slice_free(LoaderData, data);
    }
}

static void set_editable(BeditTab *tab, gboolean editable) {
    BeditView *view;
    gboolean val;

    tab->editable = editable != FALSE;

    view = bedit_tab_get_view(tab);

    val = (tab->state == BEDIT_TAB_STATE_NORMAL && tab->editable);

    gtk_text_view_set_editable(GTK_TEXT_VIEW(view), val);
}

static void install_auto_save_timeout(BeditTab *tab) {
    if (tab->auto_save_timeout == 0) {
        g_return_if_fail(tab->auto_save_interval > 0);

        tab->auto_save_timeout = g_timeout_add_seconds(
            tab->auto_save_interval * 60,
            (GSourceFunc)bedit_tab_auto_save, tab
        );
    }
}

static void remove_auto_save_timeout(BeditTab *tab) {
    bedit_debug(DEBUG_TAB);

    if (tab->auto_save_timeout > 0) {
        g_source_remove(tab->auto_save_timeout);
        tab->auto_save_timeout = 0;
    }
}

static void update_auto_save_timeout(BeditTab *tab) {
    BeditDocument *doc;
    GtkSourceFile *file;

    bedit_debug(DEBUG_TAB);

    doc = bedit_tab_get_document(tab);
    file = bedit_document_get_file(doc);

    if (
        tab->state == BEDIT_TAB_STATE_NORMAL && tab->auto_save &&
        !bedit_document_is_untitled(doc) &&
        !gtk_source_file_is_readonly(file)
    ) {
        install_auto_save_timeout(tab);
    } else {
        remove_auto_save_timeout(tab);
    }
}

static void bedit_tab_get_property(
    GObject *object, guint prop_id, GValue *value, GParamSpec *pspec
) {
    BeditTab *tab = BEDIT_TAB(object);

    switch (prop_id) {
    case PROP_NAME:
        g_value_take_string(value, _bedit_tab_get_name(tab));
        break;

    case PROP_STATE:
        g_value_set_enum(value, bedit_tab_get_state(tab));
        break;

    case PROP_AUTO_SAVE:
        g_value_set_boolean(value, bedit_tab_get_auto_save_enabled(tab));
        break;

    case PROP_AUTO_SAVE_INTERVAL:
        g_value_set_int(value, bedit_tab_get_auto_save_interval(tab));
        break;

    case PROP_CAN_CLOSE:
        g_value_set_boolean(value, _bedit_tab_get_can_close(tab));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void bedit_tab_set_property(
    GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec
) {
    BeditTab *tab = BEDIT_TAB(object);

    switch (prop_id) {
    case PROP_AUTO_SAVE:
        bedit_tab_set_auto_save_enabled(tab, g_value_get_boolean(value));
        break;

    case PROP_AUTO_SAVE_INTERVAL:
        bedit_tab_set_auto_save_interval(tab, g_value_get_int(value));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void bedit_tab_dispose(GObject *object) {
    BeditTab *tab = BEDIT_TAB(object);

    g_clear_object(&tab->editor_settings);
    g_clear_object(&tab->print_job);
    g_clear_object(&tab->print_preview);

    remove_auto_save_timeout(tab);

    if (tab->idle_scroll != 0) {
        g_source_remove(tab->idle_scroll);
        tab->idle_scroll = 0;
    }

    if (tab->cancellable != NULL) {
        g_cancellable_cancel(tab->cancellable);
        g_clear_object(&tab->cancellable);
    }

    G_OBJECT_CLASS(bedit_tab_parent_class)->dispose(object);
}

static void bedit_tab_grab_focus(GtkWidget *widget) {
    BeditTab *tab = BEDIT_TAB(widget);

    GTK_WIDGET_CLASS(bedit_tab_parent_class)->grab_focus(widget);

    if (tab->info_bar != NULL) {
        gtk_widget_grab_focus(tab->info_bar);
    } else {
        BeditView *view = bedit_tab_get_view(tab);
        gtk_widget_grab_focus(GTK_WIDGET(view));
    }
}

static void bedit_tab_drop_uris(BeditTab *tab, gchar **uri_list) {
    bedit_debug(DEBUG_TAB);
}

static void bedit_tab_class_init(BeditTabClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *gtkwidget_class = GTK_WIDGET_CLASS(klass);

    object_class->dispose = bedit_tab_dispose;
    object_class->get_property = bedit_tab_get_property;
    object_class->set_property = bedit_tab_set_property;

    gtkwidget_class->grab_focus = bedit_tab_grab_focus;

    properties[PROP_NAME] = g_param_spec_string(
        "name", "Name", "The tab's name", NULL,
        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS
    );

    properties[PROP_STATE] = g_param_spec_enum(
        "state", "State", "The tab's state", BEDIT_TYPE_TAB_STATE,
        BEDIT_TAB_STATE_NORMAL, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS
    );

    properties[PROP_AUTO_SAVE] = g_param_spec_boolean(
        "autosave", "Autosave", "Autosave feature", TRUE,
        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS
    );

    properties[PROP_AUTO_SAVE_INTERVAL] = g_param_spec_int(
        "autosave-interval", "AutosaveInterval", "Time between two autosaves",
        0, G_MAXINT, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS
    );

    properties[PROP_CAN_CLOSE] = g_param_spec_boolean(
        "can-close", "Can close", "Whether the tab can be closed", TRUE,
        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS
    );

    g_object_class_install_properties(object_class, LAST_PROP, properties);

    signals[DROP_URIS] = g_signal_new_class_handler(
        "drop-uris", G_TYPE_FROM_CLASS(klass),
        G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION, G_CALLBACK(bedit_tab_drop_uris),
        NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_STRV
    );
}

/**
 * bedit_tab_get_state:
 * @tab: a #BeditTab
 *
 * Gets the #BeditTabState of @tab.
 *
 * Returns: the #BeditTabState of @tab
 */
BeditTabState bedit_tab_get_state(BeditTab *tab) {
    g_return_val_if_fail(BEDIT_IS_TAB(tab), BEDIT_TAB_STATE_NORMAL);

    return tab->state;
}

static void set_cursor_according_to_state(
    GtkTextView *view, BeditTabState state
) {
    GdkDisplay *display;
    GdkCursor *cursor;
    GdkWindow *text_window;
    GdkWindow *left_window;

    display = gtk_widget_get_display(GTK_WIDGET(view));

    text_window = gtk_text_view_get_window(view, GTK_TEXT_WINDOW_TEXT);
    left_window = gtk_text_view_get_window(view, GTK_TEXT_WINDOW_LEFT);

    if (
        (state == BEDIT_TAB_STATE_LOADING) ||
        (state == BEDIT_TAB_STATE_REVERTING) ||
        (state == BEDIT_TAB_STATE_SAVING) ||
        (state == BEDIT_TAB_STATE_PRINTING) ||
        (state == BEDIT_TAB_STATE_CLOSING)
    ) {
        cursor = gdk_cursor_new_from_name(display, "progress");

        if (text_window != NULL) {
            gdk_window_set_cursor(text_window, cursor);
        }
        if (left_window != NULL) {
            gdk_window_set_cursor(left_window, cursor);
        }

        g_clear_object(&cursor);
    } else {
        cursor = gdk_cursor_new_from_name(display, "text");

        if (text_window != NULL) {
            gdk_window_set_cursor(text_window, cursor);
        }
        if (left_window != NULL) {
            gdk_window_set_cursor(left_window, NULL);
        }

        g_clear_object(&cursor);
    }
}

static void view_realized(GtkTextView *view, BeditTab *tab) {
    set_cursor_according_to_state(view, tab->state);
}

static void set_view_properties_according_to_state(
    BeditTab *tab, BeditTabState state
) {
    BeditView *view;
    gboolean val;
    gboolean hl_current_line;

    hl_current_line = g_settings_get_boolean(
        tab->editor_settings, BEDIT_SETTINGS_HIGHLIGHT_CURRENT_LINE
    );

    view = bedit_tab_get_view(tab);

    val = ((state == BEDIT_TAB_STATE_NORMAL) && tab->editable);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(view), val);

    val = (
        (state != BEDIT_TAB_STATE_LOADING) &&
        (state != BEDIT_TAB_STATE_CLOSING)
    );
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(view), val);

    val = (
        (state != BEDIT_TAB_STATE_LOADING) &&
        (state != BEDIT_TAB_STATE_CLOSING) && (hl_current_line)
    );
    gtk_source_view_set_highlight_current_line(GTK_SOURCE_VIEW(view), val);
}

static void bedit_tab_set_state(BeditTab *tab, BeditTabState state) {
    g_return_if_fail((state >= 0) && (state < BEDIT_TAB_NUM_OF_STATES));

    if (tab->state == state) {
        return;
    }

    tab->state = state;

    set_view_properties_according_to_state(tab, state);

    /* Hide or show the document.
     * For BEDIT_TAB_STATE_LOADING_ERROR, tab->frame is either shown or
     * hidden, depending on the error.
     */
    if (state == BEDIT_TAB_STATE_SHOWING_PRINT_PREVIEW) {
        gtk_widget_hide(GTK_WIDGET(tab->frame));
    } else if (state != BEDIT_TAB_STATE_LOADING_ERROR) {
        gtk_widget_show(GTK_WIDGET(tab->frame));
    }

    set_cursor_according_to_state(
        GTK_TEXT_VIEW(bedit_tab_get_view(tab)), state
    );

    update_auto_save_timeout(tab);

    g_object_notify_by_pspec(G_OBJECT(tab), properties[PROP_STATE]);
    g_object_notify_by_pspec(G_OBJECT(tab), properties[PROP_CAN_CLOSE]);
}

static void document_location_notify_handler(
    GtkSourceFile *file, GParamSpec *pspec, BeditTab *tab
) {
    bedit_debug(DEBUG_TAB);

    /* Notify the change in the location */
    g_object_notify_by_pspec(G_OBJECT(tab), properties[PROP_NAME]);
}

static void document_shortname_notify_handler(
    BeditDocument *document, GParamSpec *pspec, BeditTab *tab
) {
    bedit_debug(DEBUG_TAB);

    /* Notify the change in the shortname */
    g_object_notify_by_pspec(G_OBJECT(tab), properties[PROP_NAME]);
}

static void document_modified_changed(GtkTextBuffer *document, BeditTab *tab) {
    g_object_notify_by_pspec(G_OBJECT(tab), properties[PROP_NAME]);
    g_object_notify_by_pspec(G_OBJECT(tab), properties[PROP_CAN_CLOSE]);
}

static void set_info_bar(
    BeditTab *tab, GtkWidget *info_bar, GtkResponseType default_response
) {
    bedit_debug(DEBUG_TAB);

    if (tab->info_bar == info_bar) {
        return;
    }

    if (info_bar == NULL) {
        /* Don't destroy the old info_bar right away,
           we want the hide animation. */
        if (tab->info_bar_hidden != NULL) {
            gtk_widget_destroy(tab->info_bar_hidden);
        }

        tab->info_bar_hidden = tab->info_bar;
        gtk_widget_hide(tab->info_bar_hidden);

        tab->info_bar = NULL;
    } else {
        if (tab->info_bar != NULL) {
            bedit_debug_message(DEBUG_TAB, "Replacing existing notification");
            gtk_widget_destroy(tab->info_bar);
        }

        /* Make sure to stop a possibly still ongoing hiding animation. */
        if (tab->info_bar_hidden != NULL) {
            gtk_widget_destroy(tab->info_bar_hidden);
            tab->info_bar_hidden = NULL;
        }

        tab->info_bar = info_bar;
        gtk_box_pack_start(GTK_BOX(tab), info_bar, FALSE, FALSE, 0);

        /* Note this must be done after the info bar is added to the window */
        if (default_response != GTK_RESPONSE_NONE) {
            gtk_info_bar_set_default_response(
                GTK_INFO_BAR(info_bar), default_response
            );
        }

        gtk_widget_show(info_bar);
    }
}

static void remove_tab(BeditTab *tab) {
    GtkWidget *notebook;

    notebook = gtk_widget_get_parent(GTK_WIDGET(tab));
    gtk_container_remove(GTK_CONTAINER(notebook), GTK_WIDGET(tab));
}

static void io_loading_error_info_bar_response(
    GtkWidget *info_bar, gint response_id, GTask *loading_task
) {
    LoaderData *data = g_task_get_task_data(loading_task);
    GFile *location;
    const GtkSourceEncoding *encoding;

    location = gtk_source_file_loader_get_location(data->loader);

    switch (response_id) {
    case GTK_RESPONSE_OK:
        encoding = bedit_conversion_error_info_bar_get_encoding(
            GTK_WIDGET(info_bar)
        );

        set_info_bar(data->tab, NULL, GTK_RESPONSE_NONE);
        bedit_tab_set_state(data->tab, BEDIT_TAB_STATE_LOADING);

        launch_loader(loading_task, encoding);
        break;

    case GTK_RESPONSE_YES:
        /* This means that we want to edit the document anyway */
        set_editable(data->tab, TRUE);
        set_info_bar(data->tab, NULL, GTK_RESPONSE_NONE);
        bedit_tab_set_state(data->tab, BEDIT_TAB_STATE_NORMAL);

        g_task_return_boolean(loading_task, TRUE);
        g_object_unref(loading_task);
        break;

    default:
        if (location != NULL) {
            bedit_recent_remove_if_local(location);
        }

        remove_tab(data->tab);

        g_task_return_boolean(loading_task, FALSE);
        g_object_unref(loading_task);
        break;
    }
}

static void file_already_open_warning_info_bar_response(
    GtkWidget *info_bar, gint response_id, BeditTab *tab
) {
    BeditView *view = bedit_tab_get_view(tab);

    if (response_id == GTK_RESPONSE_YES) {
        set_editable(tab, TRUE);
    }

    set_info_bar(tab, NULL, GTK_RESPONSE_NONE);

    gtk_widget_grab_focus(GTK_WIDGET(view));
}

static void load_cancelled(
    GtkWidget *bar, gint response_id, GTask *loading_task
) {
    LoaderData *data = g_task_get_task_data(loading_task);

    g_return_if_fail(BEDIT_IS_PROGRESS_INFO_BAR(data->tab->info_bar));

    g_cancellable_cancel(g_task_get_cancellable(loading_task));
    remove_tab(data->tab);
}

static void unrecoverable_reverting_error_info_bar_response(
    GtkWidget *info_bar, gint response_id, GTask *loading_task
) {
    LoaderData *data = g_task_get_task_data(loading_task);
    BeditView *view;

    bedit_tab_set_state(data->tab, BEDIT_TAB_STATE_NORMAL);

    set_info_bar(data->tab, NULL, GTK_RESPONSE_NONE);

    view = bedit_tab_get_view(data->tab);
    gtk_widget_grab_focus(GTK_WIDGET(view));

    g_task_return_boolean(loading_task, FALSE);
    g_object_unref(loading_task);
}

#define MAX_MSG_LENGTH 100

static void show_loading_info_bar(GTask *loading_task) {
    LoaderData *data = g_task_get_task_data(loading_task);
    GtkWidget *bar;
    BeditDocument *doc;
    gchar *name;
    gchar *dirname = NULL;
    gchar *msg = NULL;
    gchar *name_markup;
    gchar *dirname_markup;
    gint len;

    if (data->tab->info_bar != NULL) {
        return;
    }

    bedit_debug(DEBUG_TAB);

    doc = bedit_tab_get_document(data->tab);

    name = bedit_document_get_short_name_for_display(doc);
    len = g_utf8_strlen(name, -1);

    /* if the name is awfully long, truncate it and be done with it,
     * otherwise also show the directory (ellipsized if needed)
     */
    if (len > MAX_MSG_LENGTH) {
        gchar *str;

        str = bedit_utils_str_middle_truncate(name, MAX_MSG_LENGTH);
        g_free(name);
        name = str;
    } else {
        GtkSourceFile *file = bedit_document_get_file(doc);
        GFile *location = gtk_source_file_get_location(file);

        if (location != NULL) {
            gchar *str =
                bedit_utils_location_get_dirname_for_display(location);

            /* use the remaining space for the dir, but use a min of 20 chars
             * so that we do not end up with a dirname like "(a...b)".
             * This means that in the worst case when the filename is long 99
             * we have a title long 99 + 20, but I think it's a rare enough
             * case to be acceptable. It's justa darn title afterall :)
             */
            dirname = bedit_utils_str_middle_truncate(
                str, MAX(20, MAX_MSG_LENGTH - len)
            );
            g_free(str);
        }
    }

    name_markup = g_markup_printf_escaped("<b>%s</b>", name);

    if (data->tab->state == BEDIT_TAB_STATE_REVERTING) {
        if (dirname != NULL) {
            dirname_markup = g_markup_printf_escaped("<b>%s</b>", dirname);

            /* Translators: the first %s is a file name (e.g. test.txt) the
               second one is a directory (e.g.
               ssh://master.gnome.org/home/users/paolo) */
            msg = g_strdup_printf(
                _("Reverting %s from %s"), name_markup, dirname_markup
            );
            g_free(dirname_markup);
        } else {
            msg = g_strdup_printf(_("Reverting %s"), name_markup);
        }

        bar = bedit_progress_info_bar_new("document-revert", msg, TRUE);
    } else {
        if (dirname != NULL) {
            dirname_markup = g_markup_printf_escaped("<b>%s</b>", dirname);

            /* Translators: the first %s is a file name (e.g. test.txt) the
               second one is a directory (e.g.
               ssh://master.gnome.org/home/users/paolo) */
            msg = g_strdup_printf(
                _("Loading %s from %s"), name_markup, dirname_markup
            );
            g_free(dirname_markup);
        } else {
            msg = g_strdup_printf(_("Loading %s"), name_markup);
        }

        bar = bedit_progress_info_bar_new("document-open", msg, TRUE);
    }

    g_signal_connect_object(
        bar, "response",
        G_CALLBACK(load_cancelled), loading_task, 0
    );

    set_info_bar(data->tab, bar, GTK_RESPONSE_NONE);

    g_free(msg);
    g_free(name);
    g_free(name_markup);
    g_free(dirname);
}

static void show_saving_info_bar(GTask *saving_task) {
    BeditTab *tab = g_task_get_source_object(saving_task);
    GtkWidget *bar;
    BeditDocument *doc;
    gchar *short_name;
    gchar *from;
    gchar *to = NULL;
    gchar *from_markup;
    gchar *to_markup;
    gchar *msg = NULL;
    gint len;

    if (tab->info_bar != NULL) {
        return;
    }

    bedit_debug(DEBUG_TAB);

    doc = bedit_tab_get_document(tab);

    short_name = bedit_document_get_short_name_for_display(doc);

    len = g_utf8_strlen(short_name, -1);

    /* if the name is awfully long, truncate it and be done with it,
     * otherwise also show the directory (ellipsized if needed)
     */
    if (len > MAX_MSG_LENGTH) {
        from = bedit_utils_str_middle_truncate(short_name, MAX_MSG_LENGTH);
        g_free(short_name);
    } else {
        gchar *str;
        SaverData *data;
        GFile *location;

        data = g_task_get_task_data(saving_task);
        location = gtk_source_file_saver_get_location(data->saver);

        from = short_name;
        to = g_file_get_parse_name(location);
        str = bedit_utils_str_middle_truncate(
            to, MAX(20, MAX_MSG_LENGTH - len)
        );
        g_free(to);

        to = str;
    }

    from_markup = g_markup_printf_escaped("<b>%s</b>", from);

    if (to != NULL) {
        to_markup = g_markup_printf_escaped("<b>%s</b>", to);

        /* Translators: the first %s is a file name (e.g. test.txt) the second
           one is a directory (e.g. ssh://master.gnome.org/home/users/paolo) */
        msg = g_strdup_printf(_("Saving %s to %s"), from_markup, to_markup);
        g_free(to_markup);
    } else {
        msg = g_strdup_printf(_("Saving %s"), from_markup);
    }

    bar = bedit_progress_info_bar_new("document-save", msg, FALSE);

    set_info_bar(tab, bar, GTK_RESPONSE_NONE);

    g_free(msg);
    g_free(to);
    g_free(from);
    g_free(from_markup);
}

static void info_bar_set_progress(
    BeditTab *tab, goffset size, goffset total_size
) {
    BeditProgressInfoBar *progress_info_bar;

    if (tab->info_bar == NULL) {
        return;
    }

    bedit_debug_message(
        DEBUG_TAB,
        "%" G_GOFFSET_FORMAT "/%" G_GOFFSET_FORMAT,
        size, total_size
    );

    g_return_if_fail(BEDIT_IS_PROGRESS_INFO_BAR(tab->info_bar));

    progress_info_bar = BEDIT_PROGRESS_INFO_BAR(tab->info_bar);

    if (total_size != 0) {
        gdouble frac = (gdouble)size / (gdouble)total_size;

        bedit_progress_info_bar_set_fraction(progress_info_bar, frac);
    } else if (size != 0) {
        bedit_progress_info_bar_pulse(progress_info_bar);
    } else {
        bedit_progress_info_bar_set_fraction(progress_info_bar, 0);
    }
}

/* Returns whether progress info should be shown. */
static gboolean should_show_progress_info(
    GTimer **timer, goffset size, goffset total_size
) {
    gdouble elapsed_time;
    gdouble total_time;
    gdouble remaining_time;

    g_assert(timer != NULL);

    if (*timer == NULL) {
        return TRUE;
    }

    elapsed_time = g_timer_elapsed(*timer, NULL);

    /* Wait a little, because at the very beginning it's maybe not very
     * accurate (it takes initially more time for the first bytes, the
     * following chunks should arrive more quickly, as a rough guess).
     */
    if (elapsed_time < 0.5) {
        return FALSE;
    }

    /* elapsed_time / total_time = size / total_size */
    total_time = (elapsed_time * total_size) / size;

    remaining_time = total_time - elapsed_time;

    /* Approximately more than 3 seconds remaining. */
    if (remaining_time > 3.0) {
        /* Once the progress info bar is shown, it must remain
         * shown until the end, so we don't need the timer
         * anymore.
         */
        g_timer_destroy(*timer);
        *timer = NULL;

        return TRUE;
    }

    return FALSE;
}

static gboolean scroll_to_cursor(BeditTab *tab) {
    BeditView *view;

    view = bedit_tab_get_view(tab);
    bedit_view_scroll_to_cursor(view);

    tab->idle_scroll = 0;
    return G_SOURCE_REMOVE;
}

static void unrecoverable_saving_error_info_bar_response(
    GtkWidget *info_bar, gint response_id, GTask *saving_task
) {
    BeditTab *tab = g_task_get_source_object(saving_task);
    BeditView *view;

    bedit_tab_set_state(tab, BEDIT_TAB_STATE_NORMAL);

    set_info_bar(tab, NULL, GTK_RESPONSE_NONE);

    view = bedit_tab_get_view(tab);
    gtk_widget_grab_focus(GTK_WIDGET(view));

    g_task_return_boolean(saving_task, FALSE);
    g_object_unref(saving_task);
}

/* Sets the save flags after an info bar response. */
static void response_set_save_flags(
    GTask *saving_task, GtkSourceFileSaverFlags save_flags
) {
    BeditTab *tab = g_task_get_source_object(saving_task);
    SaverData *data = g_task_get_task_data(saving_task);
    gboolean create_backup;

    create_backup = g_settings_get_boolean(
        tab->editor_settings, BEDIT_SETTINGS_CREATE_BACKUP_COPY
    );

    /* If we are here, it means that the user expressed his or her willing
     * to save the file, by pressing a button in the info bar. So even if
     * the file saving was initially an auto-save, we set the create_backup
     * flag (if the conditions are met).
     */
    if (create_backup && !data->force_no_backup) {
        save_flags |= GTK_SOURCE_FILE_SAVER_FLAGS_CREATE_BACKUP;
    } else {
        save_flags &= ~GTK_SOURCE_FILE_SAVER_FLAGS_CREATE_BACKUP;
    }

    gtk_source_file_saver_set_flags(data->saver, save_flags);
}

static void invalid_character_info_bar_response(
    GtkWidget *info_bar, gint response_id, GTask *saving_task
) {
    if (response_id == GTK_RESPONSE_YES) {
        BeditTab *tab = g_task_get_source_object(saving_task);
        SaverData *data = g_task_get_task_data(saving_task);
        GtkSourceFileSaverFlags save_flags;

        set_info_bar(tab, NULL, GTK_RESPONSE_NONE);

        /* Don't bug the user again with this... */
        tab->save_flags |= GTK_SOURCE_FILE_SAVER_FLAGS_IGNORE_INVALID_CHARS;

        save_flags = gtk_source_file_saver_get_flags(data->saver);
        save_flags |= GTK_SOURCE_FILE_SAVER_FLAGS_IGNORE_INVALID_CHARS;
        response_set_save_flags(saving_task, save_flags);

        /* Force saving */
        launch_saver(saving_task);
    } else {
        unrecoverable_saving_error_info_bar_response(
            info_bar, response_id, saving_task
        );
    }
}

static void no_backup_error_info_bar_response(
    GtkWidget *info_bar, gint response_id, GTask *saving_task
) {
    if (response_id == GTK_RESPONSE_YES) {
        BeditTab *tab = g_task_get_source_object(saving_task);
        SaverData *data = g_task_get_task_data(saving_task);
        GtkSourceFileSaverFlags save_flags;

        set_info_bar(tab, NULL, GTK_RESPONSE_NONE);

        data->force_no_backup = TRUE;
        save_flags = gtk_source_file_saver_get_flags(data->saver);
        response_set_save_flags(saving_task, save_flags);

        /* Force saving */
        launch_saver(saving_task);
    } else {
        unrecoverable_saving_error_info_bar_response(
            info_bar, response_id, saving_task
        );
    }
}

static void externally_modified_error_info_bar_response(
    GtkWidget *info_bar, gint response_id, GTask *saving_task
) {
    if (response_id == GTK_RESPONSE_YES) {
        BeditTab *tab = g_task_get_source_object(saving_task);
        SaverData *data = g_task_get_task_data(saving_task);
        GtkSourceFileSaverFlags save_flags;

        set_info_bar(tab, NULL, GTK_RESPONSE_NONE);

        /* ignore_modification_time should not be persisted in save
         * flags across saves (i.e. tab->save_flags is not modified).
         */
        save_flags = gtk_source_file_saver_get_flags(data->saver);
        save_flags |= GTK_SOURCE_FILE_SAVER_FLAGS_IGNORE_MODIFICATION_TIME;
        response_set_save_flags(saving_task, save_flags);

        /* Force saving */
        launch_saver(saving_task);
    } else {
        unrecoverable_saving_error_info_bar_response(
            info_bar, response_id, saving_task
        );
    }
}

static void recoverable_saving_error_info_bar_response(
    GtkWidget *info_bar, gint response_id, GTask *saving_task
) {
    if (response_id == GTK_RESPONSE_OK) {
        BeditTab *tab = g_task_get_source_object(saving_task);
        SaverData *data = g_task_get_task_data(saving_task);
        const GtkSourceEncoding *encoding;

        set_info_bar(tab, NULL, GTK_RESPONSE_NONE);

        encoding = bedit_conversion_error_info_bar_get_encoding(
            GTK_WIDGET(info_bar)
        );
        g_return_if_fail(encoding != NULL);

        gtk_source_file_saver_set_encoding(data->saver, encoding);
        launch_saver(saving_task);
    } else {
        unrecoverable_saving_error_info_bar_response(
            info_bar, response_id, saving_task
        );
    }
}

static void externally_modified_notification_info_bar_response(
    GtkWidget *info_bar, gint response_id, BeditTab *tab
) {
    BeditView *view;

    set_info_bar(tab, NULL, GTK_RESPONSE_NONE);

    view = bedit_tab_get_view(tab);

    if (response_id == GTK_RESPONSE_OK) {
        _bedit_tab_revert(tab);
    } else {
        tab->ask_if_externally_modified = FALSE;

        /* go back to normal state */
        bedit_tab_set_state(tab, BEDIT_TAB_STATE_NORMAL);
    }

    gtk_widget_grab_focus(GTK_WIDGET(view));
}

static void display_externally_modified_notification(BeditTab *tab) {
    GtkWidget *info_bar;
    BeditDocument *doc;
    GtkSourceFile *file;
    GFile *location;
    gboolean document_modified;

    doc = bedit_tab_get_document(tab);
    file = bedit_document_get_file(doc);

    /* we're here because the file we're editing changed on disk */
    location = gtk_source_file_get_location(file);
    g_return_if_fail(location != NULL);

    document_modified = gtk_text_buffer_get_modified(GTK_TEXT_BUFFER(doc));
    info_bar = bedit_externally_modified_info_bar_new(
        location, document_modified
    );

    set_info_bar(tab, info_bar, GTK_RESPONSE_OK);

    g_signal_connect(
        info_bar, "response",
        G_CALLBACK(externally_modified_notification_info_bar_response), tab
    );
}

static gboolean view_focused_in(
    GtkWidget *widget, GdkEventFocus *event, BeditTab *tab
) {
    BeditDocument *doc;
    GtkSourceFile *file;

    g_return_val_if_fail(BEDIT_IS_TAB(tab), GDK_EVENT_PROPAGATE);

    /* we try to detect file changes only in the normal state */
    if (tab->state != BEDIT_TAB_STATE_NORMAL) {
        return GDK_EVENT_PROPAGATE;
    }

    /* we already asked, don't bug the user again */
    if (!tab->ask_if_externally_modified) {
        return GDK_EVENT_PROPAGATE;
    }

    doc = bedit_tab_get_document(tab);
    file = bedit_document_get_file(doc);

    /* If file was never saved or is remote we do not check */
    if (gtk_source_file_is_local(file)) {
        gtk_source_file_check_file_on_disk(file);

        if (gtk_source_file_is_externally_modified(file)) {
            bedit_tab_set_state(
                tab, BEDIT_TAB_STATE_EXTERNALLY_MODIFIED_NOTIFICATION
            );

            display_externally_modified_notification(tab);
        }
    }

    return GDK_EVENT_PROPAGATE;
}

static void on_drop_uris(BeditView *view, gchar **uri_list, BeditTab *tab) {
    g_signal_emit(G_OBJECT(tab), signals[DROP_URIS], 0, uri_list);
}

static void network_available_warning_info_bar_response(
    GtkWidget *info_bar, gint response_id, BeditTab *tab
) {
    if (response_id == GTK_RESPONSE_CLOSE) {
        gtk_widget_hide(info_bar);
    }
}

void _bedit_tab_set_network_available(BeditTab *tab, gboolean enable) {
    BeditDocument *doc;
    GtkSourceFile *file;
    GFile *location;

    g_return_if_fail(BEDIT_IS_TAB(tab));

    doc = bedit_tab_get_document(tab);
    file = bedit_document_get_file(doc);
    location = gtk_source_file_get_location(file);

    if (gtk_source_file_is_local(file) || location == NULL) {
        return;
    }

    if (enable) {
        set_info_bar(tab, NULL, GTK_RESPONSE_NONE);
    } else {
        GtkWidget *info_bar = bedit_network_unavailable_info_bar_new(location);

        g_signal_connect(
            info_bar, "response",
            G_CALLBACK(network_available_warning_info_bar_response), tab
        );

        set_info_bar(tab, info_bar, GTK_RESPONSE_CLOSE);
    }
}

static void bedit_tab_init(BeditTab *tab) {
    gboolean auto_save;
    gint auto_save_interval;
    BeditDocument *doc;
    BeditView *view;
    GtkSourceFile *file;

    tab->state = BEDIT_TAB_STATE_NORMAL;

    tab->editor_settings = g_settings_new(
        "com.bwhmather.bedit.preferences.editor"
    );

    tab->editable = TRUE;

    tab->ask_if_externally_modified = TRUE;

    gtk_orientable_set_orientation(
        GTK_ORIENTABLE(tab), GTK_ORIENTATION_VERTICAL
    );

    /* Manage auto save data */
    auto_save = g_settings_get_boolean(
        tab->editor_settings, BEDIT_SETTINGS_AUTO_SAVE
    );
    g_settings_get(
        tab->editor_settings, BEDIT_SETTINGS_AUTO_SAVE_INTERVAL, "u",
        &auto_save_interval
    );

    tab->auto_save = auto_save != FALSE;

    tab->auto_save_interval = auto_save_interval;

    /* Create the frame */
    tab->frame = bedit_view_frame_new();
    gtk_widget_show(GTK_WIDGET(tab->frame));

    gtk_box_pack_end(GTK_BOX(tab), GTK_WIDGET(tab->frame), TRUE, TRUE, 0);

    doc = bedit_tab_get_document(tab);
    g_object_set_data(G_OBJECT(doc), BEDIT_TAB_KEY, tab);

    file = bedit_document_get_file(doc);

    g_signal_connect_object(
        file, "notify::location",
        G_CALLBACK(document_location_notify_handler), tab, 0
    );

    g_signal_connect(
        doc, "notify::shortname",
        G_CALLBACK(document_shortname_notify_handler), tab
    );

    g_signal_connect(
        doc, "modified_changed",
        G_CALLBACK(document_modified_changed), tab
    );

    view = bedit_tab_get_view(tab);

    g_signal_connect_after(
        view, "focus-in-event",
        G_CALLBACK(view_focused_in), tab
    );

    g_signal_connect_after(
        view, "realize",
        G_CALLBACK(view_realized), tab
    );

    g_signal_connect(
        view, "drop-uris",
        G_CALLBACK(on_drop_uris), tab
    );
}

BeditTab *_bedit_tab_new(void) {
    return g_object_new(BEDIT_TYPE_TAB, NULL);
}

/**
 * bedit_tab_get_view:
 * @tab: a #BeditTab
 *
 * Gets the #BeditView inside @tab.
 *
 * Returns: (transfer none): the #BeditView inside @tab
 */
BeditView *bedit_tab_get_view(BeditTab *tab) {
    g_return_val_if_fail(BEDIT_IS_TAB(tab), NULL);

    return bedit_view_frame_get_view(tab->frame);
}

/**
 * bedit_tab_get_document:
 * @tab: a #BeditTab
 *
 * Gets the #BeditDocument associated to @tab.
 *
 * Returns: (transfer none): the #BeditDocument associated to @tab
 */
BeditDocument *bedit_tab_get_document(BeditTab *tab) {
    BeditView *view;

    g_return_val_if_fail(BEDIT_IS_TAB(tab), NULL);

    view = bedit_view_frame_get_view(tab->frame);

    return BEDIT_DOCUMENT(gtk_text_view_get_buffer(GTK_TEXT_VIEW(view)));
}

#define MAX_DOC_NAME_LENGTH 40

gchar *_bedit_tab_get_name(BeditTab *tab) {
    BeditDocument *doc;
    gchar *name;
    gchar *docname;
    gchar *tab_name;

    g_return_val_if_fail(BEDIT_IS_TAB(tab), NULL);

    doc = bedit_tab_get_document(tab);

    name = bedit_document_get_short_name_for_display(doc);

    /* Truncate the name so it doesn't get insanely wide. */
    docname = bedit_utils_str_middle_truncate(name, MAX_DOC_NAME_LENGTH);

    if (gtk_text_buffer_get_modified(GTK_TEXT_BUFFER(doc))) {
        tab_name = g_strdup_printf("*%s", docname);
    } else {
        tab_name = g_strdup(docname);
    }

    g_free(docname);
    g_free(name);

    return tab_name;
}

gchar *_bedit_tab_get_tooltip(BeditTab *tab) {
    BeditDocument *doc;
    gchar *tip;
    gchar *uri;
    gchar *ruri;
    gchar *ruri_markup;

    g_return_val_if_fail(BEDIT_IS_TAB(tab), NULL);

    doc = bedit_tab_get_document(tab);

    uri = bedit_document_get_uri_for_display(doc);
    g_return_val_if_fail(uri != NULL, NULL);

    ruri = bedit_utils_replace_home_dir_with_tilde(uri);
    g_free(uri);

    ruri_markup = g_markup_printf_escaped("<i>%s</i>", ruri);

    switch (tab->state) {
        gchar *content_type;
        gchar *mime_type;
        gchar *content_description;
        gchar *content_full_description;
        gchar *encoding;
        GtkSourceFile *file;
        const GtkSourceEncoding *enc;

    case BEDIT_TAB_STATE_LOADING_ERROR:
        tip = g_strdup_printf(_("Error opening file %s"), ruri_markup);
        break;

    case BEDIT_TAB_STATE_REVERTING_ERROR:
        tip = g_strdup_printf(_("Error reverting file %s"), ruri_markup);
        break;

    case BEDIT_TAB_STATE_SAVING_ERROR:
        tip = g_strdup_printf(_("Error saving file %s"), ruri_markup);
        break;
    default:
        content_type = bedit_document_get_content_type(doc);
        mime_type = bedit_document_get_mime_type(doc);
        content_description = g_content_type_get_description(content_type);

        if (content_description == NULL) {
            content_full_description = g_strdup(mime_type);
        } else {
            content_full_description =
                g_strdup_printf("%s (%s)", content_description, mime_type
            );
        }

        g_free(content_type);
        g_free(mime_type);
        g_free(content_description);

        file = bedit_document_get_file(doc);
        enc = gtk_source_file_get_encoding(file);

        if (enc == NULL) {
            enc = gtk_source_encoding_get_utf8();
        }

        encoding = gtk_source_encoding_to_string(enc);

        tip = g_markup_printf_escaped(
            "<b>%s</b> %s\n\n"
            "<b>%s</b> %s\n"
            "<b>%s</b> %s",
            _("Name:"), ruri, _("MIME Type:"), content_full_description,
            _("Encoding:"), encoding
        );

        g_free(encoding);
        g_free(content_full_description);
        break;
    }

    g_free(ruri);
    g_free(ruri_markup);

    return tip;
}

GdkPixbuf *_bedit_tab_get_icon(BeditTab *tab) {
    const gchar *icon_name;
    GdkPixbuf *pixbuf = NULL;

    g_return_val_if_fail(BEDIT_IS_TAB(tab), NULL);

    switch (tab->state) {
    case BEDIT_TAB_STATE_PRINTING:
        icon_name = "printer-printing-symbolic";
        break;

    case BEDIT_TAB_STATE_SHOWING_PRINT_PREVIEW:
        icon_name = "printer-symbolic";
        break;

    case BEDIT_TAB_STATE_LOADING_ERROR:
    case BEDIT_TAB_STATE_REVERTING_ERROR:
    case BEDIT_TAB_STATE_SAVING_ERROR:
    case BEDIT_TAB_STATE_GENERIC_ERROR:
        icon_name = "dialog-error-symbolic";
        break;

    case BEDIT_TAB_STATE_EXTERNALLY_MODIFIED_NOTIFICATION:
        icon_name = "dialog-warning-symbolic";
        break;

    default:
        icon_name = NULL;
    }

    if (icon_name != NULL) {
        GdkScreen *screen;
        GtkIconTheme *theme;
        gint icon_size;

        screen = gtk_widget_get_screen(GTK_WIDGET(tab));
        theme = gtk_icon_theme_get_for_screen(screen);
        g_return_val_if_fail(theme != NULL, NULL);

        gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, NULL, &icon_size);

        pixbuf = gtk_icon_theme_load_icon(
            theme, icon_name, icon_size, 0, NULL
        );
    }

    return pixbuf;
}

/**
 * bedit_tab_get_from_document:
 * @doc: a #BeditDocument
 *
 * Gets the #BeditTab associated with @doc.
 *
 * Returns: (transfer none): the #BeditTab associated with @doc
 */
BeditTab *bedit_tab_get_from_document(BeditDocument *doc) {
    g_return_val_if_fail(BEDIT_IS_DOCUMENT(doc), NULL);

    return g_object_get_data(G_OBJECT(doc), BEDIT_TAB_KEY);
}

static void loader_progress_cb(
    goffset size, goffset total_size, GTask *loading_task
) {
    LoaderData *data = g_task_get_task_data(loading_task);

    g_return_if_fail(
        data->tab->state == BEDIT_TAB_STATE_LOADING ||
        data->tab->state == BEDIT_TAB_STATE_REVERTING
    );

    if (should_show_progress_info(&data->timer, size, total_size)) {
        show_loading_info_bar(loading_task);
        info_bar_set_progress(data->tab, size, total_size);
    }
}

static void goto_line(GTask *loading_task) {
    LoaderData *data = g_task_get_task_data(loading_task);
    BeditDocument *doc = bedit_tab_get_document(data->tab);
    GtkTextIter iter;

    /* Move the cursor at the requested line if any. */
    if (data->line_pos > 0) {
        bedit_document_goto_line_offset(
            doc, data->line_pos - 1, MAX(0, data->column_pos - 1)
        );
        return;
    }

    gtk_text_buffer_get_start_iter(GTK_TEXT_BUFFER(doc), &iter);
    gtk_text_buffer_place_cursor(GTK_TEXT_BUFFER(doc), &iter);
}

static gboolean file_already_opened(BeditDocument *doc, GFile *location) {
    GList *all_documents;
    GList *l;
    gboolean already_opened = FALSE;

    if (location == NULL) {
        return FALSE;
    }

    all_documents = bedit_app_get_documents(
        BEDIT_APP(g_application_get_default())
    );

    for (l = all_documents; l != NULL; l = l->next) {
        BeditDocument *cur_doc = l->data;
        GtkSourceFile *cur_file;
        GFile *cur_location;

        if (cur_doc == doc) {
            continue;
        }

        cur_file = bedit_document_get_file(cur_doc);
        cur_location = gtk_source_file_get_location(cur_file);

        if (cur_location != NULL && g_file_equal(location, cur_location)) {
            already_opened = TRUE;
            break;
        }
    }

    g_list_free(all_documents);

    return already_opened;
}

static void successful_load(GTask *loading_task) {
    LoaderData *data = g_task_get_task_data(loading_task);
    BeditDocument *doc = bedit_tab_get_document(data->tab);
    GtkSourceFile *file = bedit_document_get_file(doc);
    GFile *location;

    goto_line(loading_task);

    /* Scroll to the cursor when the document is loaded, we need to do it in
     * an idle as after the document is loaded the textview is still
     * redrawing and relocating its internals.
     */
    if (data->tab->idle_scroll == 0) {
        data->tab->idle_scroll = g_idle_add(
            (GSourceFunc)scroll_to_cursor, data->tab
        );
    }

    location = gtk_source_file_loader_get_location(data->loader);

    /* If the document is readonly we don't care how many times the file
     * is opened.
     */
    if (
        !gtk_source_file_is_readonly(file) &&
        file_already_opened(doc, location)
    ) {
        GtkWidget *info_bar;

        set_editable(data->tab, FALSE);

        info_bar = bedit_file_already_open_warning_info_bar_new(location);

        g_signal_connect(
            info_bar, "response",
            G_CALLBACK(file_already_open_warning_info_bar_response), data->tab
        );

        set_info_bar(data->tab, info_bar, GTK_RESPONSE_CANCEL);
    }

    /* When loading from stdin, the contents may not be saved, so set the
     * buffer as modified.
     */
    if (location == NULL) {
        gtk_text_buffer_set_modified(GTK_TEXT_BUFFER(doc), TRUE);
    }

    data->tab->ask_if_externally_modified = TRUE;

    g_signal_emit_by_name(doc, "loaded");
}

static void load_cb(
    GtkSourceFileLoader *loader, GAsyncResult *result, GTask *loading_task
) {
    LoaderData *data = g_task_get_task_data(loading_task);
    BeditDocument *doc;
    GFile *location = gtk_source_file_loader_get_location(loader);
    gboolean create_named_new_doc;
    GError *error = NULL;

    g_clear_pointer(&data->timer, g_timer_destroy);

    gtk_source_file_loader_load_finish(loader, result, &error);

    if (error != NULL) {
        bedit_debug_message(
            DEBUG_TAB, "File loading error: %s", error->message
        );

        if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
            g_task_return_boolean(loading_task, FALSE);
            g_object_unref(loading_task);

            g_error_free(error);
            return;
        }
    }

    doc = bedit_tab_get_document(data->tab);

    g_return_if_fail(
        data->tab->state == BEDIT_TAB_STATE_LOADING ||
        data->tab->state == BEDIT_TAB_STATE_REVERTING
    );

    set_info_bar(data->tab, NULL, GTK_RESPONSE_NONE);

    /* Special case creating a named new doc. */
    create_named_new_doc = (
        _bedit_document_get_create(doc) &&
        g_error_matches(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND) &&
        g_file_has_uri_scheme(location, "file")
    );

    if (create_named_new_doc) {
        g_error_free(error);
        error = NULL;
    }

    if (g_error_matches(
        error, GTK_SOURCE_FILE_LOADER_ERROR,
        GTK_SOURCE_FILE_LOADER_ERROR_CONVERSION_FALLBACK
    )) {
        GtkWidget *info_bar;
        const GtkSourceEncoding *encoding;

        /* Set the tab as not editable as we have an error, the user can
         * decide to make it editable again.
         */
        set_editable(data->tab, FALSE);

        encoding = gtk_source_file_loader_get_encoding(loader);

        info_bar = bedit_io_loading_error_info_bar_new(
            location, encoding, error
        );

        g_signal_connect(
            info_bar, "response",
            G_CALLBACK(io_loading_error_info_bar_response), loading_task
        );

        set_info_bar(data->tab, info_bar, GTK_RESPONSE_CANCEL);

        if (data->tab->state == BEDIT_TAB_STATE_LOADING) {
            gtk_widget_show(GTK_WIDGET(data->tab->frame));
            bedit_tab_set_state(data->tab, BEDIT_TAB_STATE_LOADING_ERROR);
        } else {
            bedit_tab_set_state(data->tab, BEDIT_TAB_STATE_REVERTING_ERROR);
        }

        /* The loading was successful, despite some invalid characters. */
        successful_load(loading_task);
        bedit_recent_add_document(doc);

        g_error_free(error);
        return;
    }

    if (error != NULL) {
        GtkWidget *info_bar;

        if (data->tab->state == BEDIT_TAB_STATE_LOADING) {
            gtk_widget_hide(GTK_WIDGET(data->tab->frame));
            bedit_tab_set_state(data->tab, BEDIT_TAB_STATE_LOADING_ERROR);
        } else {
            bedit_tab_set_state(data->tab, BEDIT_TAB_STATE_REVERTING_ERROR);
        }

        if (location != NULL) {
            bedit_recent_remove_if_local(location);
        }

        if (data->tab->state == BEDIT_TAB_STATE_LOADING_ERROR) {
            const GtkSourceEncoding *encoding;

            encoding = gtk_source_file_loader_get_encoding(loader);

            info_bar = bedit_io_loading_error_info_bar_new(
                location, encoding, error
            );

            g_signal_connect(
                info_bar, "response",
                G_CALLBACK(io_loading_error_info_bar_response), loading_task
            );
        } else {
            g_return_if_fail(
                data->tab->state == BEDIT_TAB_STATE_REVERTING_ERROR
            );

            info_bar = bedit_unrecoverable_reverting_error_info_bar_new(
                location, error
            );

            g_signal_connect(
                info_bar, "response",
                G_CALLBACK(unrecoverable_reverting_error_info_bar_response),
                loading_task
            );
        }

        set_info_bar(data->tab, info_bar, GTK_RESPONSE_CANCEL);

        g_error_free(error);
        return;
    }

    g_assert(error == NULL);

    bedit_tab_set_state(data->tab, BEDIT_TAB_STATE_NORMAL);
    successful_load(loading_task);

    if (!create_named_new_doc) {
        bedit_recent_add_document(doc);
    }

    g_task_return_boolean(loading_task, TRUE);
    g_object_unref(loading_task);
}

/* The returned list may contain duplicated encodings. Only the first occurrence
 * of a duplicated encoding should be kept, like it is done by
 * gtk_source_file_loader_set_candidate_encodings().
 */
static GSList *get_candidate_encodings(BeditTab *tab) {
    GSList *candidates = NULL;
    BeditDocument *doc;
    GtkSourceFile *file;
    const GtkSourceEncoding *file_encoding;

    candidates = bedit_settings_get_candidate_encodings(NULL);

    doc = bedit_tab_get_document(tab);
    file = bedit_document_get_file(doc);
    file_encoding = gtk_source_file_get_encoding(file);

    if (file_encoding != NULL) {
        candidates = g_slist_prepend(candidates, (gpointer)file_encoding);
    }

    return candidates;
}

static void launch_loader(
    GTask *loading_task, const GtkSourceEncoding *encoding
) {
    LoaderData *data = g_task_get_task_data(loading_task);
    GSList *candidate_encodings = NULL;
    BeditDocument *doc;

    if (encoding != NULL) {
        data->user_requested_encoding = TRUE;
        candidate_encodings = g_slist_append(NULL, (gpointer)encoding);
    } else {
        data->user_requested_encoding = FALSE;
        candidate_encodings = get_candidate_encodings(data->tab);
    }

    gtk_source_file_loader_set_candidate_encodings(
        data->loader, candidate_encodings
    );
    g_slist_free(candidate_encodings);

    doc = bedit_tab_get_document(data->tab);
    g_signal_emit_by_name(doc, "load");

    if (data->timer != NULL) {
        g_timer_destroy(data->timer);
    }

    data->timer = g_timer_new();

    gtk_source_file_loader_load_async(
        data->loader, G_PRIORITY_DEFAULT, g_task_get_cancellable(loading_task),
        (GFileProgressCallback)loader_progress_cb, loading_task, NULL,
        (GAsyncReadyCallback)load_cb, loading_task
    );
}

static void load_async(
    BeditTab *tab, GFile *location, const GtkSourceEncoding *encoding,
    gint line_pos, gint column_pos, gboolean create, GCancellable *cancellable,
    GAsyncReadyCallback callback, gpointer user_data
) {
    BeditDocument *doc;
    GtkSourceFile *file;
    GTask *loading_task;
    LoaderData *data;

    g_return_if_fail(BEDIT_IS_TAB(tab));
    g_return_if_fail(G_IS_FILE(location));
    g_return_if_fail(cancellable == NULL || G_IS_CANCELLABLE(cancellable));
    g_return_if_fail(tab->state == BEDIT_TAB_STATE_NORMAL);

    bedit_tab_set_state(tab, BEDIT_TAB_STATE_LOADING);

    doc = bedit_tab_get_document(tab);
    file = bedit_document_get_file(doc);
    gtk_source_file_set_location(file, location);

    loading_task = g_task_new(NULL, cancellable, callback, user_data);

    data = loader_data_new();
    g_task_set_task_data(loading_task, data, (GDestroyNotify)loader_data_free);

    data->tab = tab;
    data->loader = gtk_source_file_loader_new(GTK_SOURCE_BUFFER(doc), file);
    data->line_pos = line_pos;
    data->column_pos = column_pos;

    _bedit_document_set_create(doc, create);

    launch_loader(loading_task, encoding);
}

static void load_finish(BeditTab *tab, GAsyncResult *result, void *data) {
    g_return_if_fail(g_task_is_valid(result, tab));

    g_task_propagate_boolean(G_TASK(result), NULL);
}

void _bedit_tab_load(
    BeditTab *tab, GFile *location, const GtkSourceEncoding *encoding,
    gint line_pos, gint column_pos, gboolean create
) {
    if (tab->cancellable != NULL) {
        g_cancellable_cancel(tab->cancellable);
        g_object_unref(tab->cancellable);
    }

    tab->cancellable = g_cancellable_new();

    load_async(
        tab, location, encoding, line_pos, column_pos,
        create, tab->cancellable,
        (GAsyncReadyCallback)load_finish, NULL
    );
}

static void load_stream_async(
    BeditTab *tab, GInputStream *stream, const GtkSourceEncoding *encoding,
    gint line_pos, gint column_pos, GCancellable *cancellable,
    GAsyncReadyCallback callback, gpointer user_data
) {
    BeditDocument *doc;
    GtkSourceFile *file;
    GTask *loading_task;
    LoaderData *data;

    g_return_if_fail(BEDIT_IS_TAB(tab));
    g_return_if_fail(G_IS_INPUT_STREAM(stream));
    g_return_if_fail(cancellable == NULL || G_IS_CANCELLABLE(cancellable));
    g_return_if_fail(tab->state == BEDIT_TAB_STATE_NORMAL);

    bedit_tab_set_state(tab, BEDIT_TAB_STATE_LOADING);

    doc = bedit_tab_get_document(tab);
    file = bedit_document_get_file(doc);

    gtk_source_file_set_location(file, NULL);

    loading_task = g_task_new(NULL, cancellable, callback, user_data);

    data = loader_data_new();
    g_task_set_task_data(loading_task, data, (GDestroyNotify)loader_data_free);

    data->tab = tab;
    data->loader = gtk_source_file_loader_new_from_stream(
        GTK_SOURCE_BUFFER(doc), file, stream
    );
    data->line_pos = line_pos;
    data->column_pos = column_pos;

    _bedit_document_set_create(doc, FALSE);

    launch_loader(loading_task, encoding);
}

void _bedit_tab_load_stream(
    BeditTab *tab, GInputStream *stream, const GtkSourceEncoding *encoding,
    gint line_pos, gint column_pos
) {
    if (tab->cancellable != NULL) {
        g_cancellable_cancel(tab->cancellable);
        g_object_unref(tab->cancellable);
    }

    tab->cancellable = g_cancellable_new();

    load_stream_async(
        tab, stream, encoding, line_pos, column_pos, tab->cancellable,
        (GAsyncReadyCallback)load_finish, NULL
    );
}

static void revert_async(
    BeditTab *tab, GCancellable *cancellable, GAsyncReadyCallback callback,
    gpointer user_data
) {
    BeditDocument *doc;
    GtkSourceFile *file;
    GFile *location;
    GTask *loading_task;
    LoaderData *data;

    g_return_if_fail(BEDIT_IS_TAB(tab));
    g_return_if_fail(cancellable == NULL || G_IS_CANCELLABLE(cancellable));
    g_return_if_fail(
        tab->state == BEDIT_TAB_STATE_NORMAL ||
        tab->state == BEDIT_TAB_STATE_EXTERNALLY_MODIFIED_NOTIFICATION
    );

    if (tab->state == BEDIT_TAB_STATE_EXTERNALLY_MODIFIED_NOTIFICATION) {
        set_info_bar(tab, NULL, GTK_RESPONSE_NONE);
    }

    doc = bedit_tab_get_document(tab);
    file = bedit_document_get_file(doc);
    location = gtk_source_file_get_location(file);
    g_return_if_fail(location != NULL);

    bedit_tab_set_state(tab, BEDIT_TAB_STATE_REVERTING);

    loading_task = g_task_new(NULL, cancellable, callback, user_data);

    data = loader_data_new();
    g_task_set_task_data(loading_task, data, (GDestroyNotify)loader_data_free);

    data->tab = tab;
    data->loader = gtk_source_file_loader_new(GTK_SOURCE_BUFFER(doc), file);
    data->line_pos = 0;
    data->column_pos = 0;

    launch_loader(loading_task, NULL);
}

void _bedit_tab_revert(BeditTab *tab) {
    if (tab->cancellable != NULL) {
        g_cancellable_cancel(tab->cancellable);
        g_object_unref(tab->cancellable);
    }

    tab->cancellable = g_cancellable_new();

    revert_async(
        tab, tab->cancellable, (GAsyncReadyCallback)load_finish, NULL
    );
}

static void close_printing(BeditTab *tab) {
    if (tab->print_preview != NULL) {
        gtk_widget_destroy(tab->print_preview);
    }

    g_clear_object(&tab->print_job);
    g_clear_object(&tab->print_preview);

    /* destroy the info bar */
    set_info_bar(tab, NULL, GTK_RESPONSE_NONE);

    bedit_tab_set_state(tab, BEDIT_TAB_STATE_NORMAL);
}

static void saver_progress_cb(
    goffset size, goffset total_size, GTask *saving_task
) {
    BeditTab *tab = g_task_get_source_object(saving_task);
    SaverData *data = g_task_get_task_data(saving_task);

    g_return_if_fail(tab->state == BEDIT_TAB_STATE_SAVING);

    if (should_show_progress_info(&data->timer, size, total_size)) {
        show_saving_info_bar(saving_task);
        info_bar_set_progress(tab, size, total_size);
    }
}

static void save_cb(
    GtkSourceFileSaver *saver, GAsyncResult *result, GTask *saving_task
) {
    BeditTab *tab = g_task_get_source_object(saving_task);
    SaverData *data = g_task_get_task_data(saving_task);
    BeditDocument *doc = bedit_tab_get_document(tab);
    GFile *location = gtk_source_file_saver_get_location(saver);
    GError *error = NULL;

    g_return_if_fail(tab->state == BEDIT_TAB_STATE_SAVING);

    gtk_source_file_saver_save_finish(saver, result, &error);

    if (error != NULL) {
        bedit_debug_message(
            DEBUG_TAB, "File saving error: %s", error->message
        );
    }

    if (data->timer != NULL) {
        g_timer_destroy(data->timer);
        data->timer = NULL;
    }

    set_info_bar(tab, NULL, GTK_RESPONSE_NONE);

    if (error != NULL) {
        GtkWidget *info_bar;

        bedit_tab_set_state(tab, BEDIT_TAB_STATE_SAVING_ERROR);

        if (
            error->domain == GTK_SOURCE_FILE_SAVER_ERROR &&
            error->code == GTK_SOURCE_FILE_SAVER_ERROR_EXTERNALLY_MODIFIED
        ) {
            /* This error is recoverable */
            info_bar = bedit_externally_modified_saving_error_info_bar_new(
                location, error
            );
            g_return_if_fail(info_bar != NULL);

            g_signal_connect(
                info_bar, "response",
                G_CALLBACK(externally_modified_error_info_bar_response),
                saving_task
            );
        } else if (
            error->domain == G_IO_ERROR &&
            error->code == G_IO_ERROR_CANT_CREATE_BACKUP
        ) {
            /* This error is recoverable */
            info_bar = bedit_no_backup_saving_error_info_bar_new(
                location, error
            );
            g_return_if_fail(info_bar != NULL);

            g_signal_connect(
                info_bar, "response",
                G_CALLBACK(no_backup_error_info_bar_response), saving_task
            );
        } else if (
            error->domain == GTK_SOURCE_FILE_SAVER_ERROR &&
            error->code == GTK_SOURCE_FILE_SAVER_ERROR_INVALID_CHARS
        ) {
            /* If we have any invalid char in the document we must warn the
             * user as it can make the document useless if it is saved.
             */
            info_bar = bedit_invalid_character_info_bar_new(location);
            g_return_if_fail(info_bar != NULL);

            g_signal_connect(
                info_bar, "response",
                G_CALLBACK(invalid_character_info_bar_response), saving_task
            );
        } else if (error->domain == GTK_SOURCE_FILE_SAVER_ERROR || (
            error->domain == G_IO_ERROR &&
            error->code != G_IO_ERROR_INVALID_DATA &&
            error->code != G_IO_ERROR_PARTIAL_INPUT
        )) {
            /* These errors are _NOT_ recoverable */
            bedit_recent_remove_if_local(location);

            info_bar = bedit_unrecoverable_saving_error_info_bar_new(
                location, error
            );
            g_return_if_fail(info_bar != NULL);

            g_signal_connect(
                info_bar, "response",
                G_CALLBACK(unrecoverable_saving_error_info_bar_response),
                saving_task
            );
        } else {
            const GtkSourceEncoding *encoding;

            /* This error is recoverable */
            g_return_if_fail(
                error->domain == G_CONVERT_ERROR ||
                error->domain == G_IO_ERROR
            );

            encoding = gtk_source_file_saver_get_encoding(saver);

            info_bar = bedit_conversion_error_while_saving_info_bar_new(
                location, encoding, error
            );
            g_return_if_fail(info_bar != NULL);

            g_signal_connect(
                info_bar, "response",
                G_CALLBACK(recoverable_saving_error_info_bar_response),
                saving_task
            );
        }

        set_info_bar(tab, info_bar, GTK_RESPONSE_CANCEL);
    } else {
        bedit_recent_add_document(doc);

        bedit_tab_set_state(tab, BEDIT_TAB_STATE_NORMAL);

        tab->ask_if_externally_modified = TRUE;

        g_signal_emit_by_name(doc, "saved");
        g_task_return_boolean(saving_task, TRUE);
        g_object_unref(saving_task);
    }

    if (error != NULL) {
        g_error_free(error);
    }
}

static void launch_saver(GTask *saving_task) {
    BeditTab *tab = g_task_get_source_object(saving_task);
    BeditDocument *doc = bedit_tab_get_document(tab);
    SaverData *data = g_task_get_task_data(saving_task);

    bedit_tab_set_state(tab, BEDIT_TAB_STATE_SAVING);

    g_signal_emit_by_name(doc, "save");

    if (data->timer != NULL) {
        g_timer_destroy(data->timer);
    }

    data->timer = g_timer_new();

    gtk_source_file_saver_save_async(
        data->saver, G_PRIORITY_DEFAULT, g_task_get_cancellable(saving_task),
        (GFileProgressCallback)saver_progress_cb, saving_task, NULL,
        (GAsyncReadyCallback)save_cb, saving_task
    );
}

/* Gets the initial save flags, when launching a new FileSaver. */
static GtkSourceFileSaverFlags get_initial_save_flags(
    BeditTab *tab, gboolean auto_save
) {
    GtkSourceFileSaverFlags save_flags;
    gboolean create_backup;

    save_flags = tab->save_flags;

    create_backup = g_settings_get_boolean(
        tab->editor_settings, BEDIT_SETTINGS_CREATE_BACKUP_COPY
    );

    /* In case of autosaving, we need to preserve the backup that was produced
     * the last time the user "manually" saved the file. So we don't set the
     * CREATE_BACKUP flag for an automatic file saving.
     */
    if (create_backup && !auto_save) {
        save_flags |= GTK_SOURCE_FILE_SAVER_FLAGS_CREATE_BACKUP;
    }

    return save_flags;
}

void _bedit_tab_save_async(
    BeditTab *tab, GCancellable *cancellable,
    GAsyncReadyCallback callback, gpointer user_data
) {
    GTask *saving_task;
    SaverData *data;
    BeditDocument *doc;
    GtkSourceFile *file;
    GtkSourceFileSaverFlags save_flags;

    g_return_if_fail(BEDIT_IS_TAB(tab));
    g_return_if_fail(
        tab->state == BEDIT_TAB_STATE_NORMAL ||
        tab->state == BEDIT_TAB_STATE_EXTERNALLY_MODIFIED_NOTIFICATION ||
        tab->state == BEDIT_TAB_STATE_SHOWING_PRINT_PREVIEW
    );

    /* The Save and Save As window actions are insensitive when the print
     * preview is shown, but it's still possible to save several documents
     * at once (with the Save All action or when quitting bedit). In that
     * case, the print preview is simply closed. Handling correctly the
     * document saving when the print preview is shown is more complicated
     * and error-prone, it doesn't worth the effort. (the print preview
     * would need to be updated when the filename changes, dealing with file
     * saving errors is also more complicated, etc).
     */
    if (tab->state == BEDIT_TAB_STATE_SHOWING_PRINT_PREVIEW) {
        close_printing(tab);
    }

    doc = bedit_tab_get_document(tab);
    g_return_if_fail(!bedit_document_is_untitled(doc));

    saving_task = g_task_new(tab, cancellable, callback, user_data);

    data = saver_data_new();
    g_task_set_task_data(saving_task, data, (GDestroyNotify)saver_data_free);

    save_flags = get_initial_save_flags(tab, FALSE);

    if (tab->state == BEDIT_TAB_STATE_EXTERNALLY_MODIFIED_NOTIFICATION) {
        /* We already told the user about the external modification:
         * hide the message bar and set the save flag.
         */
        set_info_bar(tab, NULL, GTK_RESPONSE_NONE);
        save_flags |= GTK_SOURCE_FILE_SAVER_FLAGS_IGNORE_MODIFICATION_TIME;
    }

    file = bedit_document_get_file(doc);

    data->saver = gtk_source_file_saver_new(GTK_SOURCE_BUFFER(doc), file);

    gtk_source_file_saver_set_flags(data->saver, save_flags);

    launch_saver(saving_task);
}

gboolean _bedit_tab_save_finish(BeditTab *tab, GAsyncResult *result) {
    g_return_val_if_fail(g_task_is_valid(result, tab), FALSE);

    return g_task_propagate_boolean(G_TASK(result), NULL);
}

static void auto_save_finished_cb(
    BeditTab *tab, GAsyncResult *result, gpointer user_data
) {
    _bedit_tab_save_finish(tab, result);
}

static gboolean bedit_tab_auto_save(BeditTab *tab) {
    GTask *saving_task;
    SaverData *data;
    BeditDocument *doc;
    GtkSourceFile *file;
    GtkSourceFileSaverFlags save_flags;

    bedit_debug(DEBUG_TAB);

    doc = bedit_tab_get_document(tab);
    file = bedit_document_get_file(doc);

    g_return_val_if_fail(!bedit_document_is_untitled(doc), G_SOURCE_REMOVE);
    g_return_val_if_fail(!gtk_source_file_is_readonly(file), G_SOURCE_REMOVE);

    if (!gtk_text_buffer_get_modified(GTK_TEXT_BUFFER(doc))) {
        bedit_debug_message(DEBUG_TAB, "Document not modified");

        return G_SOURCE_CONTINUE;
    }

    if (tab->state != BEDIT_TAB_STATE_NORMAL) {
        bedit_debug_message(DEBUG_TAB, "Retry after 30 seconds");

        tab->auto_save_timeout = g_timeout_add_seconds(
            30, (GSourceFunc)bedit_tab_auto_save, tab
        );

        /* Destroy the old timeout. */
        return G_SOURCE_REMOVE;
    }

    /* Set auto_save_timeout to 0 since the timeout is going to be destroyed */
    tab->auto_save_timeout = 0;

    saving_task = g_task_new(
        tab, NULL, (GAsyncReadyCallback)auto_save_finished_cb, NULL
    );

    data = saver_data_new();
    g_task_set_task_data(saving_task, data, (GDestroyNotify)saver_data_free);

    data->saver = gtk_source_file_saver_new(GTK_SOURCE_BUFFER(doc), file);

    save_flags = get_initial_save_flags(tab, TRUE);
    gtk_source_file_saver_set_flags(data->saver, save_flags);

    launch_saver(saving_task);

    return G_SOURCE_REMOVE;
}

/* Call _bedit_tab_save_finish() in @callback, there is no
 * _bedit_tab_save_as_finish().
 */
void _bedit_tab_save_as_async(
    BeditTab *tab, GFile *location, const GtkSourceEncoding *encoding,
    GtkSourceNewlineType newline_type,
    GtkSourceCompressionType compression_type, GCancellable *cancellable,
    GAsyncReadyCallback callback, gpointer user_data
) {
    GTask *saving_task;
    SaverData *data;
    BeditDocument *doc;
    GtkSourceFile *file;
    GtkSourceFileSaverFlags save_flags;

    g_return_if_fail(BEDIT_IS_TAB(tab));
    g_return_if_fail(
        tab->state == BEDIT_TAB_STATE_NORMAL ||
        tab->state == BEDIT_TAB_STATE_EXTERNALLY_MODIFIED_NOTIFICATION ||
        tab->state == BEDIT_TAB_STATE_SHOWING_PRINT_PREVIEW
    );
    g_return_if_fail(G_IS_FILE(location));
    g_return_if_fail(encoding != NULL);

    /* See note at _bedit_tab_save_async(). */
    if (tab->state == BEDIT_TAB_STATE_SHOWING_PRINT_PREVIEW) {
        close_printing(tab);
    }

    saving_task = g_task_new(tab, cancellable, callback, user_data);

    data = saver_data_new();
    g_task_set_task_data(saving_task, data, (GDestroyNotify)saver_data_free);

    doc = bedit_tab_get_document(tab);

    /* reset the save flags, when saving as */
    tab->save_flags = GTK_SOURCE_FILE_SAVER_FLAGS_NONE;

    save_flags = get_initial_save_flags(tab, FALSE);

    if (tab->state == BEDIT_TAB_STATE_EXTERNALLY_MODIFIED_NOTIFICATION) {
        /* We already told the user about the external modification:
         * hide the message bar and set the save flag.
         */
        set_info_bar(tab, NULL, GTK_RESPONSE_NONE);
        save_flags |= GTK_SOURCE_FILE_SAVER_FLAGS_IGNORE_MODIFICATION_TIME;
    }

    file = bedit_document_get_file(doc);

    data->saver = gtk_source_file_saver_new_with_target(
        GTK_SOURCE_BUFFER(doc), file, location
    );

    gtk_source_file_saver_set_encoding(data->saver, encoding);
    gtk_source_file_saver_set_newline_type(data->saver, newline_type);
    gtk_source_file_saver_set_compression_type(data->saver, compression_type);
    gtk_source_file_saver_set_flags(data->saver, save_flags);

    launch_saver(saving_task);
}

#define BEDIT_PAGE_SETUP_KEY "bedit-page-setup-key"
#define BEDIT_PRINT_SETTINGS_KEY "bedit-print-settings-key"

static GtkPageSetup *get_page_setup(BeditTab *tab) {
    gpointer data;
    BeditDocument *doc;

    doc = bedit_tab_get_document(tab);

    data = g_object_get_data(G_OBJECT(doc), BEDIT_PAGE_SETUP_KEY);

    if (data == NULL) {
        return _bedit_app_get_default_page_setup(
            BEDIT_APP(g_application_get_default())
        );
    } else {
        return gtk_page_setup_copy(GTK_PAGE_SETUP(data));
    }
}

static GtkPrintSettings *get_print_settings(BeditTab *tab) {
    gpointer data;
    BeditDocument *doc;
    GtkPrintSettings *settings;
    gchar *name;

    doc = bedit_tab_get_document(tab);

    data = g_object_get_data(G_OBJECT(doc), BEDIT_PRINT_SETTINGS_KEY);

    if (data == NULL) {
        settings = _bedit_app_get_default_print_settings(
            BEDIT_APP(g_application_get_default())
        );
    } else {
        settings = gtk_print_settings_copy(GTK_PRINT_SETTINGS(data));
    }

    /* Be sure the OUTPUT_URI is unset, because otherwise the
     * OUTPUT_BASENAME is not taken into account.
     */
    gtk_print_settings_set(settings, GTK_PRINT_SETTINGS_OUTPUT_URI, NULL);

    name = bedit_document_get_short_name_for_display(doc);
    gtk_print_settings_set(settings, GTK_PRINT_SETTINGS_OUTPUT_BASENAME, name);

    g_free(name);

    return settings;
}

/* FIXME: show the info bar only if the operation will be "long" */
static void printing_cb(
    BeditPrintJob *job, BeditPrintJobStatus status, BeditTab *tab
) {
    g_return_if_fail(BEDIT_IS_PROGRESS_INFO_BAR(tab->info_bar));

    gtk_widget_show(tab->info_bar);

    bedit_progress_info_bar_set_text(
        BEDIT_PROGRESS_INFO_BAR(tab->info_bar),
        bedit_print_job_get_status_string(job)
    );

    bedit_progress_info_bar_set_fraction(
        BEDIT_PROGRESS_INFO_BAR(tab->info_bar),
        bedit_print_job_get_progress(job)
    );
}

static void store_print_settings(BeditTab *tab, BeditPrintJob *job) {
    BeditDocument *doc;
    GtkPrintSettings *settings;
    GtkPageSetup *page_setup;

    doc = bedit_tab_get_document(tab);

    settings = bedit_print_job_get_print_settings(job);

    /* clear n-copies settings since we do not want to
     * persist that one */
    gtk_print_settings_unset(settings, GTK_PRINT_SETTINGS_N_COPIES);

    /* remember settings for this document */
    g_object_set_data_full(
        G_OBJECT(doc), BEDIT_PRINT_SETTINGS_KEY,
        g_object_ref(settings),
        (GDestroyNotify)g_object_unref
    );

    /* make them the default */
    _bedit_app_set_default_print_settings(
        BEDIT_APP(g_application_get_default()), settings
    );

    page_setup = bedit_print_job_get_page_setup(job);

    /* remember page setup for this document */
    g_object_set_data_full(
        G_OBJECT(doc), BEDIT_PAGE_SETUP_KEY,
        g_object_ref(page_setup),
        (GDestroyNotify)g_object_unref
    );

    /* make it the default */
    _bedit_app_set_default_page_setup(
        BEDIT_APP(g_application_get_default()), page_setup
    );
}

static void done_printing_cb(
    BeditPrintJob *job, BeditPrintJobResult result, GError *error,
    BeditTab *tab
) {
    BeditView *view;

    g_return_if_fail(
        tab->state == BEDIT_TAB_STATE_SHOWING_PRINT_PREVIEW ||
        tab->state == BEDIT_TAB_STATE_PRINTING
    );

    if (result == BEDIT_PRINT_JOB_RESULT_OK) {
        store_print_settings(tab, job);
    }

    /* TODO Show the error in an info bar. */
    if (error != NULL) {
        g_warning("Printing error: %s", error->message);
        g_error_free(error);
        error = NULL;
    }

    close_printing(tab);

    view = bedit_tab_get_view(tab);
    gtk_widget_grab_focus(GTK_WIDGET(view));
}

static void show_preview_cb(
    BeditPrintJob *job, BeditPrintPreview *preview, BeditTab *tab
) {
    g_return_if_fail(tab->print_preview == NULL);

    /* destroy the info bar */
    set_info_bar(tab, NULL, GTK_RESPONSE_NONE);

    tab->print_preview = GTK_WIDGET(preview);
    g_object_ref_sink(tab->print_preview);

    gtk_box_pack_end(GTK_BOX(tab), tab->print_preview, TRUE, TRUE, 0);

    gtk_widget_show(tab->print_preview);
    gtk_widget_grab_focus(tab->print_preview);

    bedit_tab_set_state(tab, BEDIT_TAB_STATE_SHOWING_PRINT_PREVIEW);
}

static void print_cancelled(GtkWidget *bar, gint response_id, BeditTab *tab) {
    bedit_debug(DEBUG_TAB);

    if (tab->print_job != NULL) {
        bedit_print_job_cancel(tab->print_job);
    }
}

static void add_printing_info_bar(BeditTab *tab) {
    GtkWidget *bar;

    bar = bedit_progress_info_bar_new("document-print", "", TRUE);

    g_signal_connect(bar, "response", G_CALLBACK(print_cancelled), tab);

    set_info_bar(tab, bar, GTK_RESPONSE_NONE);

    /* hide until we start printing */
    gtk_widget_hide(bar);
}

void _bedit_tab_print(BeditTab *tab) {
    BeditView *view;
    GtkPageSetup *setup;
    GtkPrintSettings *settings;
    GtkPrintOperationResult res;
    GError *error = NULL;

    g_return_if_fail(BEDIT_IS_TAB(tab));

    /* FIXME: currently we can have just one printoperation going on at a
     * given time, so before starting the print we close the preview.
     * Would be nice to handle it properly though.
     */
    if (tab->state == BEDIT_TAB_STATE_SHOWING_PRINT_PREVIEW) {
        close_printing(tab);
    }

    g_return_if_fail(tab->print_job == NULL);
    g_return_if_fail(tab->state == BEDIT_TAB_STATE_NORMAL);

    view = bedit_tab_get_view(tab);

    tab->print_job = bedit_print_job_new(view);

    add_printing_info_bar(tab);

    g_signal_connect_object(
        tab->print_job, "printing",
        G_CALLBACK(printing_cb), tab, 0
    );

    g_signal_connect_object(
        tab->print_job, "show-preview",
        G_CALLBACK(show_preview_cb), tab, 0
    );

    g_signal_connect_object(
        tab->print_job, "done",
        G_CALLBACK(done_printing_cb), tab, 0
    );

    bedit_tab_set_state(tab, BEDIT_TAB_STATE_PRINTING);

    setup = get_page_setup(tab);
    settings = get_print_settings(tab);

    res = bedit_print_job_print(
        tab->print_job, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG, setup,
        settings, GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(tab))),
        &error
    );

    /* TODO: manage res in the correct way */
    if (res == GTK_PRINT_OPERATION_RESULT_ERROR) {
        /* FIXME: go in error state */
        g_warning("Async print preview failed (%s)", error->message);
        g_error_free(error);

        close_printing(tab);
    }

    g_object_unref(setup);
    g_object_unref(settings);
}

void _bedit_tab_mark_for_closing(BeditTab *tab) {
    g_return_if_fail(BEDIT_IS_TAB(tab));
    g_return_if_fail(tab->state == BEDIT_TAB_STATE_NORMAL);

    bedit_tab_set_state(tab, BEDIT_TAB_STATE_CLOSING);
}

gboolean _bedit_tab_get_can_close(BeditTab *tab) {
    BeditDocument *doc;

    g_return_val_if_fail(BEDIT_IS_TAB(tab), FALSE);

    /* if we are loading or reverting, the tab can be closed */
    if (tab->state == BEDIT_TAB_STATE_LOADING ||
        tab->state == BEDIT_TAB_STATE_LOADING_ERROR ||
        tab->state == BEDIT_TAB_STATE_REVERTING ||
        /* CHECK: I'm not sure this is the
           right behavior for REVERTING ERROR */
        tab->state == BEDIT_TAB_STATE_REVERTING_ERROR
    ) {
        return TRUE;
    }

    /* Do not close tab with saving errors */
    if (tab->state == BEDIT_TAB_STATE_SAVING_ERROR) {
        return FALSE;
    }

    doc = bedit_tab_get_document(tab);

    if (_bedit_document_needs_saving(doc)) {
        return FALSE;
    }

    return TRUE;
}

/**
 * bedit_tab_get_auto_save_enabled:
 * @tab: a #BeditTab
 *
 * Gets the current state for the autosave feature
 *
 * Return value: %TRUE if the autosave is enabled, else %FALSE
 **/
gboolean bedit_tab_get_auto_save_enabled(BeditTab *tab) {
    bedit_debug(DEBUG_TAB);

    g_return_val_if_fail(BEDIT_IS_TAB(tab), FALSE);

    return tab->auto_save;
}

/**
 * bedit_tab_set_auto_save_enabled:
 * @tab: a #BeditTab
 * @enable: enable (%TRUE) or disable (%FALSE) auto save
 *
 * Enables or disables the autosave feature. It does not install an
 * autosave timeout if the document is new or is read-only
 **/
void bedit_tab_set_auto_save_enabled(BeditTab *tab, gboolean enable) {
    bedit_debug(DEBUG_TAB);

    g_return_if_fail(BEDIT_IS_TAB(tab));

    enable = enable != FALSE;

    if (tab->auto_save != enable) {
        tab->auto_save = enable;
        update_auto_save_timeout(tab);
        return;
    }
}

/**
 * bedit_tab_get_auto_save_interval:
 * @tab: a #BeditTab
 *
 * Gets the current interval for the autosaves
 *
 * Return value: the value of the autosave
 **/
gint bedit_tab_get_auto_save_interval(BeditTab *tab) {
    bedit_debug(DEBUG_TAB);

    g_return_val_if_fail(BEDIT_IS_TAB(tab), 0);

    return tab->auto_save_interval;
}

/**
 * bedit_tab_set_auto_save_interval:
 * @tab: a #BeditTab
 * @interval: the new interval
 *
 * Sets the interval for the autosave feature.
 */
void bedit_tab_set_auto_save_interval(BeditTab *tab, gint interval) {
    g_return_if_fail(BEDIT_IS_TAB(tab));
    g_return_if_fail(interval > 0);

    bedit_debug(DEBUG_TAB);

    if (tab->auto_save_interval != interval) {
        tab->auto_save_interval = interval;
        remove_auto_save_timeout(tab);
        update_auto_save_timeout(tab);
    }
}

void bedit_tab_set_info_bar(BeditTab *tab, GtkWidget *info_bar) {
    g_return_if_fail(BEDIT_IS_TAB(tab));
    g_return_if_fail(info_bar == NULL || GTK_IS_WIDGET(info_bar));

    /* FIXME: this can cause problems with the tab state machine */
    set_info_bar(tab, info_bar, GTK_RESPONSE_NONE);
}

BeditViewFrame *_bedit_tab_get_view_frame(BeditTab *tab) {
    return tab->frame;
}

