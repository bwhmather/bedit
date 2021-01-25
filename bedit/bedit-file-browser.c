/*
 * bedit-file-browser-utils.h
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * Based on gedit-file-browser-utils.h from Gedit.
 *
 * Copyright (C) 2014 - Ignacio Casal Quinteiro
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

#include "bedit-file-browser.h"

#include <string.h>

#include <glib/gi18n-lib.h>
#include <gmodule.h>

#include "bedit-app.h"
#include "bedit-commands.h"
#include "bedit-debug.h"
#include "bedit-enum-types.h"
#include "bedit-file-browser-error.h"
#include "bedit-file-browser-messages.h"
#include "bedit-file-browser-utils.h"
#include "bedit-file-browser-widget.h"
#include "bedit-utils.h"
#include "bedit-window.h"

#define FILEBROWSER_BASE_SETTINGS "com.bwhmather.bedit.plugins.filebrowser"
#define FILEBROWSER_TREE_VIEW "tree-view"
#define FILEBROWSER_BINARY_PATTERNS "binary-patterns"

#define NAUTILUS_BASE_SETTINGS "org.gnome.nautilus.preferences"
#define NAUTILUS_FALLBACK_SETTINGS                                          \
    "com.bwhmather.bedit.plugins.filebrowser.nautilus"
#define NAUTILUS_CLICK_POLICY_KEY "click-policy"
#define NAUTILUS_CONFIRM_TRASH_KEY "confirm-trash"

#define TERMINAL_BASE_SETTINGS "org.gnome.desktop.default-applications.terminal"
#define TERMINAL_EXEC_KEY "exec"

struct _BeditFileBrowser {
    GObject parent_instance;

    BeditWindow *window;

    GSimpleAction *toggle_action;

    GSettings *settings;
    GSettings *nautilus_settings;
    GSettings *terminal_settings;

    GtkWidget *action_area_button;
    GtkPopover *popover;
    BeditFileBrowserWidget *tree_widget;
    gboolean confirm_trash;

    guint click_policy_handle;
    guint confirm_trash_handle;
};

enum { PROP_0, PROP_WINDOW };

static void on_toggle_action_cb(
    GAction *action, GVariant *parameter,
    BeditFileBrowser *file_browser
);
static void on_file_activated_cb(
    BeditFileBrowserWidget *widget, GFile *location,
    BeditFileBrowser *file_browser
);
static void on_directory_activated_cb(
    BeditFileBrowserWidget *widget, GFile *location,
    BeditFileBrowser *file_browser
);
static void on_error_cb(
    BeditFileBrowserWidget *widget, guint code, gchar const *message,
    BeditFileBrowser *file_browser
);
static void on_rename_cb(
    BeditFileBrowserStore *model, GFile *oldfile, GFile *newfile,
    BeditFileBrowser *file_browser
);
static gboolean on_confirm_delete_cb(
    BeditFileBrowserWidget *widget, BeditFileBrowserStore *store, GList *rows,
    BeditFileBrowser *file_browser
);
static gboolean on_confirm_no_trash_cb(
    BeditFileBrowserWidget *widget, GList *files,
    BeditFileBrowser *file_browser
);
static void on_popover_closed_cb(
    GtkPopover *popover, BeditFileBrowser *file_browser
);
static void on_window_size_allocate_cb(
    BeditWindow *window, GdkRectangle *allocation,
    BeditFileBrowser *file_browser
);

G_DEFINE_TYPE(
    BeditFileBrowser,
    bedit_file_browser,
    G_TYPE_OBJECT
)

static GSettings *settings_try_new(const gchar *schema_id) {
    GSettings *settings = NULL;
    GSettingsSchemaSource *source;
    GSettingsSchema *schema;

    source = g_settings_schema_source_get_default();

    schema = g_settings_schema_source_lookup(source, schema_id, TRUE);

    if (schema != NULL) {
        settings = g_settings_new_full(schema, NULL, NULL);
        g_settings_schema_unref(schema);
    }

    return settings;
}

static void bedit_file_browser_init(
    BeditFileBrowser *file_browser
) {
    file_browser->settings = g_settings_new(FILEBROWSER_BASE_SETTINGS);
    file_browser->terminal_settings = g_settings_new(TERMINAL_BASE_SETTINGS);
    file_browser->nautilus_settings = settings_try_new(NAUTILUS_BASE_SETTINGS);

    if (file_browser->nautilus_settings == NULL) {
        file_browser->nautilus_settings = g_settings_new(
            NAUTILUS_FALLBACK_SETTINGS
        );
    }
}

static void bedit_file_browser_dispose(GObject *object) {
    BeditFileBrowser *file_browser;

    file_browser = BEDIT_FILE_BROWSER(object);

    g_clear_object(&file_browser->settings);
    g_clear_object(&file_browser->nautilus_settings);
    g_clear_object(&file_browser->terminal_settings);
    g_clear_object(&file_browser->window);

    G_OBJECT_CLASS(bedit_file_browser_parent_class)->dispose(object);
}

static void bedit_file_browser_set_property(
    GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec
) {
    BeditFileBrowser *file_browser;

    file_browser = BEDIT_FILE_BROWSER(object);

    switch (prop_id) {
    case PROP_WINDOW:
        file_browser->window = BEDIT_WINDOW(g_value_dup_object(value));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void bedit_file_browser_get_property(
    GObject *object, guint prop_id, GValue *value, GParamSpec *pspec
) {
    BeditFileBrowser *file_browser;

    file_browser = BEDIT_FILE_BROWSER(object);

    switch (prop_id) {
    case PROP_WINDOW:
        g_value_set_object(value, file_browser->window);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void on_click_policy_changed(
    GSettings *settings, const gchar *key,
    BeditFileBrowser *file_browser
) {
    BeditFileBrowserFolderViewClickPolicy policy;
    BeditFileBrowserFolderView *view;

    g_return_if_fail(BEDIT_IS_FILE_BROWSER(file_browser));

    policy = g_settings_get_enum(settings, key);

    view = bedit_file_browser_widget_get_browser_view(
        file_browser->tree_widget
    );
    bedit_file_browser_folder_view_set_click_policy(view, policy);
}

static void on_confirm_trash_changed(
    GSettings *settings, const gchar *key,
    BeditFileBrowser *file_browser
) {
    g_return_if_fail(BEDIT_IS_FILE_BROWSER(file_browser));

    file_browser->confirm_trash = g_settings_get_boolean(settings, key);
}

static void install_nautilus_prefs(BeditFileBrowser *file_browser) {
    gboolean prefb;
    BeditFileBrowserFolderViewClickPolicy policy;
    BeditFileBrowserFolderView *view;

    g_return_if_fail(BEDIT_IS_FILE_BROWSER(file_browser));

    /* Get click_policy */
    policy = g_settings_get_enum(
        file_browser->nautilus_settings, NAUTILUS_CLICK_POLICY_KEY
    );

    view = bedit_file_browser_widget_get_browser_view(
        file_browser->tree_widget
    );
    bedit_file_browser_folder_view_set_click_policy(view, policy);

    file_browser->click_policy_handle = g_signal_connect(
        file_browser->nautilus_settings, "changed::" NAUTILUS_CLICK_POLICY_KEY,
        G_CALLBACK(on_click_policy_changed), file_browser
    );

    /* Get confirm_trash */
    prefb = g_settings_get_boolean(
        file_browser->nautilus_settings, NAUTILUS_CONFIRM_TRASH_KEY
    );

    file_browser->confirm_trash = prefb;

    file_browser->confirm_trash_handle = g_signal_connect(
        file_browser->nautilus_settings, "changed::" NAUTILUS_CONFIRM_TRASH_KEY,
        G_CALLBACK(on_confirm_trash_changed), file_browser
    );
}

static void set_root_from_doc(
    BeditFileBrowser *file_browser, BeditDocument *doc
) {
    GtkSourceFile *file;
    GFile *location;
    GFile *parent;

    g_return_if_fail(BEDIT_IS_FILE_BROWSER(file_browser));

    if (doc == NULL) {
        return;
    }

    file = bedit_document_get_file(doc);
    location = gtk_source_file_get_location(file);
    if (location == NULL) {
        return;
    }

    parent = g_file_get_parent(location);

    if (parent != NULL) {
        bedit_file_browser_widget_set_virtual_root(
            file_browser->tree_widget, parent
        );

        g_object_unref(parent);
    }
}

static void set_active_root(
    BeditFileBrowserWidget *widget, BeditFileBrowser *file_browser
) {
    g_return_if_fail(BEDIT_IS_FILE_BROWSER_WIDGET(widget));
    g_return_if_fail(BEDIT_IS_FILE_BROWSER(file_browser));

    set_root_from_doc(
        file_browser, bedit_window_get_active_document(file_browser->window)
    );
}

static gchar *get_terminal(BeditFileBrowser *file_browser) {
    gchar *terminal;

    g_return_val_if_fail(BEDIT_IS_FILE_BROWSER(file_browser), NULL);

    terminal = g_settings_get_string(
        file_browser->terminal_settings, TERMINAL_EXEC_KEY
    );

    if (terminal == NULL) {
        const gchar *term = g_getenv("TERM");

        if (term != NULL) {
            terminal = g_strdup(term);
        } else {
            terminal = g_strdup("xterm");
        }
    }

    return terminal;
}

static void open_in_terminal(
    BeditFileBrowserWidget *widget, GFile *location,
    BeditFileBrowser *file_browser
) {
    g_return_if_fail(BEDIT_IS_FILE_BROWSER_WIDGET(widget));
    g_return_if_fail(BEDIT_IS_FILE_BROWSER(file_browser));

    if (location) {
        gchar *terminal;
        gchar *local;
        gchar *argv[2];

        terminal = get_terminal(file_browser);
        local = g_file_get_path(location);

        argv[0] = terminal;
        argv[1] = NULL;

        g_spawn_async(
            local, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL
        );

        g_free(terminal);
        g_free(local);
    }
}

void bedit_file_browser_update_state(BeditFileBrowser *file_browser) {
    BeditDocument *doc;

    g_return_if_fail(BEDIT_IS_FILE_BROWSER(file_browser));
    doc = bedit_window_get_active_document(file_browser->window);

    bedit_file_browser_widget_set_active_root_enabled(
        file_browser->tree_widget,
        doc != NULL && !bedit_document_is_untitled(doc)
    );
}

void bedit_file_browser_activate(BeditFileBrowser *file_browser) {
    GtkWidget *action_area;
    GtkWidget *action_area_button_image;
    BeditFileBrowserStore *store;

    g_return_if_fail(BEDIT_IS_FILE_BROWSER(file_browser));

    /* Setup tree widget. */
    file_browser->tree_widget =
        BEDIT_FILE_BROWSER_WIDGET(bedit_file_browser_widget_new()
    );

    g_signal_connect(
        file_browser->tree_widget, "file-activated",
        G_CALLBACK(on_file_activated_cb), file_browser
    );
    g_signal_connect(
        file_browser->tree_widget, "directory-activated",
        G_CALLBACK(on_directory_activated_cb), file_browser
    );

    g_signal_connect(
        file_browser->tree_widget, "error",
        G_CALLBACK(on_error_cb), file_browser
    );

    g_signal_connect(
        file_browser->tree_widget, "confirm-delete",
        G_CALLBACK(on_confirm_delete_cb), file_browser
    );

    g_signal_connect(
        file_browser->tree_widget, "confirm-no-trash",
        G_CALLBACK(on_confirm_no_trash_cb), file_browser
    );

    g_signal_connect(
        file_browser->tree_widget, "open-in-terminal",
        G_CALLBACK(open_in_terminal), file_browser
    );

    g_signal_connect(
        file_browser->tree_widget, "set-active-root",
        G_CALLBACK(set_active_root), file_browser
    );

    /* Setup menu button widget. */
    action_area_button_image = gtk_image_new_from_icon_name(
        "folder-symbolic", GTK_ICON_SIZE_BUTTON
    );

    file_browser->action_area_button = gtk_menu_button_new();
    gtk_widget_set_valign(
        GTK_WIDGET(file_browser->action_area_button), GTK_ALIGN_CENTER
    );
    gtk_widget_set_can_focus(
        GTK_WIDGET(file_browser->action_area_button), FALSE
    );
    gtk_widget_set_tooltip_text(
        GTK_WIDGET(file_browser->action_area_button),
        _("File browser (Ctrl+Shift+O)")
    );

    gtk_container_add(
        GTK_CONTAINER(file_browser->action_area_button),
        action_area_button_image
    );

    /* Connect using popover. */
    file_browser->popover = GTK_POPOVER(
        gtk_popover_new(file_browser->action_area_button)
    );
    gtk_menu_button_set_popover(
        GTK_MENU_BUTTON(file_browser->action_area_button),
        GTK_WIDGET(file_browser->popover)
    );

    g_object_set(
        G_OBJECT(file_browser->popover),
        "constrain-to", GTK_POPOVER_CONSTRAINT_WINDOW, NULL
    );

    g_object_set(
        G_OBJECT(file_browser->popover),
        "width-request", 350, NULL
    );
    g_object_set(
        G_OBJECT(file_browser->popover),
        "height-request", 800, NULL
    );

    /* Listen for resize events to fit popover inside window. */
    g_signal_connect(
        file_browser->window, "size-allocate",
        G_CALLBACK(on_window_size_allocate_cb), file_browser
    );

    g_signal_connect(
        file_browser->popover, "closed",
        G_CALLBACK(on_popover_closed_cb), file_browser
    );

    gtk_container_add(
        GTK_CONTAINER(file_browser->popover),
        GTK_WIDGET(file_browser->tree_widget)
    );


    /* Add everything to the area to the left of the tab bar. */
    action_area = bedit_window_get_action_area(file_browser->window);
    gtk_container_add(
        GTK_CONTAINER(action_area), GTK_WIDGET(file_browser->action_area_button)
    );

    gtk_widget_show(action_area_button_image);
    gtk_widget_show(file_browser->action_area_button);
    gtk_widget_show(GTK_WIDGET(file_browser->tree_widget));

    /* Install nautilus preferences */
    install_nautilus_prefs(file_browser);

    /* Connect signals to store the last visited location */
    store = bedit_file_browser_widget_get_browser_store(
        file_browser->tree_widget
    );

    g_settings_bind(
        file_browser->settings, "show-hidden",
        store, "show-hidden",
        G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET
    );

    g_settings_bind(
        file_browser->settings, "show-binary",
        store, "show-binary",
        G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET
    );

    g_settings_bind(
        file_browser->settings, FILEBROWSER_BINARY_PATTERNS,
        store, FILEBROWSER_BINARY_PATTERNS,
        G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET
    );

    g_object_bind_property(
        file_browser->window, "default-location",
        file_browser->tree_widget, "virtual-root",
        G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE
    );

    g_signal_connect(
        store, "rename",
        G_CALLBACK(on_rename_cb), file_browser
    );

    /* Register messages on the bus */
    bedit_file_browser_messages_register(
        file_browser->window, file_browser->tree_widget
    );

    /* Bind action to toggle browser when keyboard shortcut pressed. */
    file_browser->toggle_action = g_simple_action_new("file-browser", NULL);
    g_signal_connect(
        file_browser->toggle_action, "activate",
        G_CALLBACK(on_toggle_action_cb), file_browser
    );
    g_action_map_add_action(
        G_ACTION_MAP(file_browser->window),
        G_ACTION(file_browser->toggle_action)
    );

    bedit_file_browser_update_state(file_browser);
}

void bedit_file_browser_deactivate(BeditFileBrowser *file_browser) {
    GtkWidget *action_area;

    g_return_if_fail(BEDIT_IS_FILE_BROWSER(file_browser));

    /* Unregister messages from the bus */
    bedit_file_browser_messages_unregister(file_browser->window);

    /* Disconnect signals */
    if (file_browser->click_policy_handle) {
        g_signal_handler_disconnect(
            file_browser->nautilus_settings, file_browser->click_policy_handle
        );
    }

    if (file_browser->confirm_trash_handle) {
        g_signal_handler_disconnect(
            file_browser->nautilus_settings, file_browser->confirm_trash_handle
        );
    }

    action_area = bedit_window_get_action_area(file_browser->window);
    gtk_container_remove(
        GTK_CONTAINER(action_area), GTK_WIDGET(file_browser->action_area_button)
    );

    g_action_map_remove_action(
        G_ACTION_MAP(file_browser->window), "file-browser"
    );
}

static void bedit_file_browser_class_init(
    BeditFileBrowserClass *klass
) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->dispose = bedit_file_browser_dispose;
    object_class->set_property = bedit_file_browser_set_property;
    object_class->get_property = bedit_file_browser_get_property;

    g_object_class_install_property(
        object_class, PROP_WINDOW,
        g_param_spec_object(
            "window", "Window", "The bedit window", BEDIT_TYPE_WINDOW,
            G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
            G_PARAM_STATIC_STRINGS
        )
    );
}

BeditFileBrowser *bedit_file_browser_new(BeditWindow *window) {
    return BEDIT_FILE_BROWSER(
        g_object_new(BEDIT_TYPE_FILE_BROWSER, "window", window, NULL)
    );
}

/* Callbacks */
static void on_toggle_action_cb(
    GAction *action, GVariant *parameter,
    BeditFileBrowser *file_browser
) {
    GtkToggleButton *toggle_button;

    g_return_if_fail(BEDIT_IS_FILE_BROWSER(file_browser));

    toggle_button = GTK_TOGGLE_BUTTON(file_browser->action_area_button);
    if (gtk_toggle_button_get_active(toggle_button)) {
        gtk_toggle_button_set_active(toggle_button, FALSE);
    } else {
        gtk_toggle_button_set_active(toggle_button, TRUE);
    }
}

static void on_file_activated_cb(
    BeditFileBrowserWidget *tree_widget, GFile *location,
    BeditFileBrowser *file_browser
) {
    g_return_if_fail(BEDIT_IS_FILE_BROWSER_WIDGET(tree_widget));
    g_return_if_fail(BEDIT_IS_FILE_BROWSER(file_browser));

    bedit_commands_load_location(file_browser->window, location, NULL, 0, 0);
}

static void on_directory_activated_cb(
    BeditFileBrowserWidget *tree_widget, GFile *location,
    BeditFileBrowser *file_browser
) {
    g_return_if_fail(BEDIT_IS_FILE_BROWSER_WIDGET(tree_widget));
    g_return_if_fail(G_IS_FILE(location));
    g_return_if_fail(BEDIT_IS_FILE_BROWSER(file_browser));

    _bedit_window_set_default_location(file_browser->window, location);
}


static void on_error_cb(
    BeditFileBrowserWidget *tree_widget, guint code, gchar const *message,
    BeditFileBrowser *file_browser
) {
    gchar *title;
    GtkWidget *dlg;

    g_return_if_fail(BEDIT_IS_FILE_BROWSER_WIDGET(tree_widget));
    g_return_if_fail(BEDIT_IS_FILE_BROWSER(file_browser));

    switch (code) {
    case BEDIT_FILE_BROWSER_ERROR_NEW_DIRECTORY:
        title = _("An error occurred while creating a new directory");
        break;
    case BEDIT_FILE_BROWSER_ERROR_NEW_FILE:
        title = _("An error occurred while creating a new file");
        break;
    case BEDIT_FILE_BROWSER_ERROR_RENAME:
        title = _("An error occurred while renaming a file or directory");
        break;
    case BEDIT_FILE_BROWSER_ERROR_DELETE:
        title = _("An error occurred while deleting a file or directory");
        break;
    case BEDIT_FILE_BROWSER_ERROR_OPEN_DIRECTORY:
        title = _(
            "An error occurred while opening a directory in the file manager"
        );
        break;
    case BEDIT_FILE_BROWSER_ERROR_SET_ROOT:
        title = _("An error occurred while setting a root directory");
        break;
    case BEDIT_FILE_BROWSER_ERROR_LOAD_DIRECTORY:
        title = _("An error occurred while loading a directory");
        break;
    default:
        title = _("An error occurred");
        break;
    }

    dlg = gtk_message_dialog_new(
        GTK_WINDOW(file_browser->window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR,
        GTK_BUTTONS_OK, "%s", title
    );
    gtk_message_dialog_format_secondary_text(
        GTK_MESSAGE_DIALOG(dlg), "%s", message
    );

    gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);
}

static void on_rename_cb(
    BeditFileBrowserStore *store, GFile *oldfile, GFile *newfile,
    BeditFileBrowser *file_browser
) {
    GList *documents;
    GList *item;

    g_return_if_fail(BEDIT_IS_FILE_BROWSER_STORE(store));
    g_return_if_fail(BEDIT_IS_FILE_BROWSER(file_browser));

    /* Find all documents and set its uri to newuri where it matches olduri */
    documents = bedit_app_get_documents(BEDIT_APP(g_application_get_default()));

    for (item = documents; item; item = item->next) {
        BeditDocument *doc;
        GtkSourceFile *source_file;
        GFile *docfile;

        doc = BEDIT_DOCUMENT(item->data);
        source_file = bedit_document_get_file(doc);
        docfile = gtk_source_file_get_location(source_file);

        if (docfile == NULL) {
            continue;
        }

        if (g_file_equal(docfile, oldfile)) {
            gtk_source_file_set_location(source_file, newfile);
        } else {
            gchar *relative;

            relative = g_file_get_relative_path(oldfile, docfile);

            if (relative != NULL) {
                /* Relative now contains the part in docfile without
                   the prefix oldfile */

                docfile = g_file_get_child(newfile, relative);

                gtk_source_file_set_location(source_file, docfile);

                g_object_unref(docfile);
            }

            g_free(relative);
        }
    }

    g_list_free(documents);
}

static gchar *get_filename_from_path(GtkTreeModel *model, GtkTreePath *path) {
    GtkTreeIter iter;
    GFile *location;
    gchar *ret = NULL;

    if (!gtk_tree_model_get_iter(model, &iter, path)) {
        return NULL;
    }

    gtk_tree_model_get(
        model, &iter,
        BEDIT_FILE_BROWSER_STORE_COLUMN_LOCATION, &location,
        -1
    );

    if (location) {
        ret = bedit_file_browser_utils_file_basename(location);
        g_object_unref(location);
    }

    return ret;
}

static gboolean on_confirm_no_trash_cb(
    BeditFileBrowserWidget *widget, GList *files, BeditFileBrowser *file_browser
) {
    gchar *normal;
    gchar *message;
    gchar *secondary;
    gboolean result;

    message =
        _("Cannot move file to trash, do you\nwant to delete permanently?");

    if (files->next == NULL) {
        normal = bedit_file_browser_utils_file_basename(G_FILE(files->data));
        secondary = g_strdup_printf(
            _("The file “%s” cannot be moved to the trash."), normal
        );
        g_free(normal);
    } else {
        secondary = g_strdup(
            _("The selected files cannot be moved to the trash.")
        );
    }

    result = bedit_file_browser_utils_confirmation_dialog(
        file_browser->window, GTK_MESSAGE_QUESTION,
        message, secondary,
        _("_Delete")
    );
    g_free(secondary);

    return result;
}

static gboolean on_confirm_delete_cb(
    BeditFileBrowserWidget *widget, BeditFileBrowserStore *store, GList *paths,
    BeditFileBrowser *file_browser
) {
    gchar *normal;
    gchar *message;
    gchar *secondary;
    gboolean result;

    g_return_val_if_fail(BEDIT_IS_FILE_BROWSER_WIDGET(widget), FALSE);
    g_return_val_if_fail(BEDIT_IS_FILE_BROWSER_STORE(store), FALSE);
    g_return_val_if_fail(BEDIT_IS_FILE_BROWSER(file_browser), FALSE);

    if (!file_browser->confirm_trash) {
        return TRUE;
    }

    if (paths->next == NULL) {
        normal = get_filename_from_path(
            GTK_TREE_MODEL(store), (GtkTreePath *)(paths->data)
        );
        message = g_strdup_printf(
            _("Are you sure you want to permanently delete “%s”?"), normal
        );
        g_free(normal);
    } else {
        message = g_strdup(
            _("Are you sure you want to permanently delete the selected files?")
        );
    }

    secondary = _("If you delete an item, it is permanently lost.");

    result = bedit_file_browser_utils_confirmation_dialog(
        file_browser->window, GTK_MESSAGE_QUESTION,
        message, secondary,
        _("_Delete")
    );

    g_free(message);

    return result;
}

static void on_popover_closed_cb(
    GtkPopover *popover, BeditFileBrowser *file_browser
) {
    g_return_if_fail(GTK_IS_POPOVER(popover));
    g_return_if_fail(BEDIT_IS_FILE_BROWSER(file_browser));

    bedit_file_browser_widget_set_filter_enabled(
        file_browser->tree_widget, FALSE
    );
}

static void on_window_size_allocate_cb(
    BeditWindow *window, GdkRectangle *allocation,
    BeditFileBrowser *file_browser
) {
    g_return_if_fail(BEDIT_IS_WINDOW(window));
    g_return_if_fail(BEDIT_IS_FILE_BROWSER(file_browser));

    g_object_set(
        G_OBJECT(file_browser->popover),
        "width-request", CLAMP(allocation->width - 40, 200, 450), NULL
    );

    g_object_set(
        G_OBJECT(file_browser->popover),
        "height-request", CLAMP(allocation->height - 40, 100, 1200), NULL
    );
}

