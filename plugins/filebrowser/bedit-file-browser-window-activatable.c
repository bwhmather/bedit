 /*
 * bedit-file_browser-window-activatable.c
 * This file is part of bedit
 *
 * Copyright (C) 2014 - Ignacio Casal Quinteiro
 *
 * bedit is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * bedit is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with bedit. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <bedit/bedit-app.h>
#include <bedit/bedit-commands.h>
#include <bedit/bedit-debug.h>
#include <bedit/bedit-utils.h>
#include <bedit/bedit-window-activatable.h>
#include <bedit/bedit-window.h>
#include <glib/gi18n-lib.h>
#include <gmodule.h>
#include <string.h>

#include "bedit-file-browser-enum-types.h"
#include "bedit-file-browser-error.h"
#include "bedit-file-browser-messages.h"
#include "bedit-file-browser-window-activatable.h"
#include "bedit-file-browser-utils.h"
#include "bedit-file-browser-widget.h"

#define FILEBROWSER_BASE_SETTINGS "com.bwhmather.bedit.plugins.filebrowser"
#define FILEBROWSER_TREE_VIEW "tree-view"
#define FILEBROWSER_FILTER_MODE "filter-mode"
#define FILEBROWSER_FILTER_PATTERN "filter-pattern"
#define FILEBROWSER_BINARY_PATTERNS "binary-patterns"

#define NAUTILUS_BASE_SETTINGS "org.gnome.nautilus.preferences"
#define NAUTILUS_FALLBACK_SETTINGS                                          \
    "com.bwhmather.bedit.plugins.filebrowser.nautilus"
#define NAUTILUS_CLICK_POLICY_KEY "click-policy"
#define NAUTILUS_CONFIRM_TRASH_KEY "confirm-trash"

#define TERMINAL_BASE_SETTINGS "org.gnome.desktop.default-applications.terminal"
#define TERMINAL_EXEC_KEY "exec"

struct _BeditFileBrowserWindowActivatable {
    GObject parent;
};

typedef struct _BeditFileBrowserWindowActivatablePrivate {
    BeditWindow *window;

    GSettings *settings;
    GSettings *nautilus_settings;
    GSettings *terminal_settings;

    GtkWidget *action_area_button;
    BeditFileBrowserWidget *tree_widget;
    gulong end_loading_handle;
    gboolean confirm_trash;

    guint click_policy_handle;
    guint confirm_trash_handle;
} BeditFileBrowserWindowActivatablePrivate;

enum { PROP_0, PROP_WINDOW };

static void bedit_window_activatable_iface_init(
    BeditWindowActivatableInterface *iface
);

static void on_location_activated_cb(
    BeditFileBrowserWidget *widget, GFile *location, BeditWindow *window
);
static void on_error_cb(
    BeditFileBrowserWidget *widget, guint code, gchar const *message,
    BeditFileBrowserWindowActivatable *plugin
);
static void on_virtual_root_changed_cb(
    BeditFileBrowserStore *model, GParamSpec *param,
    BeditFileBrowserWindowActivatable *plugin
);
static void on_rename_cb(
    BeditFileBrowserStore *model, GFile *oldfile, GFile *newfile,
    BeditWindow *window
);
static gboolean on_confirm_delete_cb(
    BeditFileBrowserWidget *widget, BeditFileBrowserStore *store, GList *rows,
    BeditFileBrowserWindowActivatable *plugin
);
static gboolean on_confirm_no_trash_cb(
    BeditFileBrowserWidget *widget, GList *files, BeditWindow *window
);

G_DEFINE_DYNAMIC_TYPE_EXTENDED(
    BeditFileBrowserWindowActivatable,
    bedit_file_browser_window_activatable,
    G_TYPE_OBJECT, 0,
    G_ADD_PRIVATE_DYNAMIC(BeditFileBrowserWindowActivatable)
    G_IMPLEMENT_INTERFACE_DYNAMIC(
        BEDIT_TYPE_WINDOW_ACTIVATABLE, bedit_window_activatable_iface_init
    )
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

static void bedit_file_browser_window_activatable_init(
    BeditFileBrowserWindowActivatable *plugin
) {
    BeditFileBrowserWindowActivatablePrivate *priv;

    priv = bedit_file_browser_window_activatable_get_instance_private(plugin);

    priv->settings = g_settings_new(FILEBROWSER_BASE_SETTINGS);
    priv->terminal_settings = g_settings_new(TERMINAL_BASE_SETTINGS);
    priv->nautilus_settings = settings_try_new(NAUTILUS_BASE_SETTINGS);

    if (priv->nautilus_settings == NULL) {
        priv->nautilus_settings = g_settings_new(
            NAUTILUS_FALLBACK_SETTINGS
        );
    }
}

static void bedit_file_browser_window_activatable_dispose(GObject *object) {
    BeditFileBrowserWindowActivatable *plugin;
    BeditFileBrowserWindowActivatablePrivate *priv;

    plugin = BEDIT_FILE_BROWSER_WINDOW_ACTIVATABLE(object);
    priv = bedit_file_browser_window_activatable_get_instance_private(plugin);

    g_clear_object(&priv->settings);
    g_clear_object(&priv->nautilus_settings);
    g_clear_object(&priv->terminal_settings);
    g_clear_object(&priv->window);

    G_OBJECT_CLASS(bedit_file_browser_window_activatable_parent_class)->dispose(object);
}

static void bedit_file_browser_window_activatable_set_property(
    GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec
) {
    BeditFileBrowserWindowActivatable *plugin;
    BeditFileBrowserWindowActivatablePrivate *priv;

    plugin = BEDIT_FILE_BROWSER_WINDOW_ACTIVATABLE(object);
    priv = bedit_file_browser_window_activatable_get_instance_private(plugin);

    switch (prop_id) {
    case PROP_WINDOW:
        priv->window = BEDIT_WINDOW(g_value_dup_object(value));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void bedit_file_browser_window_activatable_get_property(
    GObject *object, guint prop_id, GValue *value, GParamSpec *pspec
) {
    BeditFileBrowserWindowActivatable *plugin;
    BeditFileBrowserWindowActivatablePrivate *priv;

    plugin = BEDIT_FILE_BROWSER_WINDOW_ACTIVATABLE(object);
    priv = bedit_file_browser_window_activatable_get_instance_private(plugin);

    switch (prop_id) {
    case PROP_WINDOW:
        g_value_set_object(value, priv->window);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void on_end_loading_cb(
    BeditFileBrowserStore *store, GtkTreeIter *iter,
    BeditFileBrowserWindowActivatable *plugin
) {
    BeditFileBrowserWindowActivatablePrivate *priv;

    g_return_if_fail(BEDIT_IS_FILE_BROWSER_STORE(store));

    priv = bedit_file_browser_window_activatable_get_instance_private(plugin);

    /* Disconnect the signal */
    g_signal_handler_disconnect(store, priv->end_loading_handle);
    priv->end_loading_handle = 0;
}

static void on_click_policy_changed(
    GSettings *settings, const gchar *key,
    BeditFileBrowserWindowActivatable *plugin
) {
    BeditFileBrowserWindowActivatablePrivate *priv;
    BeditFileBrowserViewClickPolicy policy;
    BeditFileBrowserView *view;

    g_return_if_fail(BEDIT_IS_FILE_BROWSER_WINDOW_ACTIVATABLE(plugin));

    priv = bedit_file_browser_window_activatable_get_instance_private(plugin);

    policy = g_settings_get_enum(settings, key);

    view = bedit_file_browser_widget_get_browser_view(priv->tree_widget);
    bedit_file_browser_view_set_click_policy(view, policy);
}

static void on_confirm_trash_changed(
    GSettings *settings, const gchar *key,
    BeditFileBrowserWindowActivatable *plugin
) {
    BeditFileBrowserWindowActivatablePrivate *priv;

    g_return_if_fail(BEDIT_IS_FILE_BROWSER_WINDOW_ACTIVATABLE(plugin));

    priv = bedit_file_browser_window_activatable_get_instance_private(plugin);

    priv->confirm_trash = g_settings_get_boolean(settings, key);
}

static void install_nautilus_prefs(BeditFileBrowserWindowActivatable *plugin) {
    BeditFileBrowserWindowActivatablePrivate *priv;
    gboolean prefb;
    BeditFileBrowserViewClickPolicy policy;
    BeditFileBrowserView *view;

    g_return_if_fail(BEDIT_IS_FILE_BROWSER_WINDOW_ACTIVATABLE(plugin));

    priv = bedit_file_browser_window_activatable_get_instance_private(plugin);

    /* Get click_policy */
    policy = g_settings_get_enum(
        priv->nautilus_settings, NAUTILUS_CLICK_POLICY_KEY
    );

    view = bedit_file_browser_widget_get_browser_view(priv->tree_widget);
    bedit_file_browser_view_set_click_policy(view, policy);

    priv->click_policy_handle = g_signal_connect(
        priv->nautilus_settings, "changed::" NAUTILUS_CLICK_POLICY_KEY,
        G_CALLBACK(on_click_policy_changed), plugin
    );

    /* Get confirm_trash */
    prefb = g_settings_get_boolean(
        priv->nautilus_settings, NAUTILUS_CONFIRM_TRASH_KEY
    );

    priv->confirm_trash = prefb;

    priv->confirm_trash_handle = g_signal_connect(
        priv->nautilus_settings, "changed::" NAUTILUS_CONFIRM_TRASH_KEY,
        G_CALLBACK(on_confirm_trash_changed), plugin
    );
}

static void set_root_from_doc(
    BeditFileBrowserWindowActivatable *plugin, BeditDocument *doc
) {
    BeditFileBrowserWindowActivatablePrivate *priv;
    GtkSourceFile *file;
    GFile *location;
    GFile *parent;

    g_return_if_fail(BEDIT_IS_FILE_BROWSER_WINDOW_ACTIVATABLE(plugin));

    priv = bedit_file_browser_window_activatable_get_instance_private(plugin);

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
        bedit_file_browser_widget_set_virtual_root(priv->tree_widget, parent);

        g_object_unref(parent);
    }
}

static void set_active_root(
    BeditFileBrowserWidget *widget, BeditFileBrowserWindowActivatable *plugin
) {
    BeditFileBrowserWindowActivatablePrivate *priv;

    g_return_if_fail(BEDIT_IS_FILE_BROWSER_WIDGET(widget));
    g_return_if_fail(BEDIT_IS_FILE_BROWSER_WINDOW_ACTIVATABLE(plugin));

    priv = bedit_file_browser_window_activatable_get_instance_private(plugin);

    set_root_from_doc(
        plugin, bedit_window_get_active_document(priv->window)
    );
}

static gchar *get_terminal(BeditFileBrowserWindowActivatable *plugin) {
    BeditFileBrowserWindowActivatablePrivate *priv;
    gchar *terminal;

    g_return_val_if_fail(BEDIT_IS_FILE_BROWSER_WINDOW_ACTIVATABLE(plugin), NULL);

    priv = bedit_file_browser_window_activatable_get_instance_private(plugin);

    terminal = g_settings_get_string(
        priv->terminal_settings, TERMINAL_EXEC_KEY
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
    BeditFileBrowserWindowActivatable *plugin
) {
    g_return_if_fail(BEDIT_IS_FILE_BROWSER_WIDGET(widget));
    g_return_if_fail(BEDIT_IS_FILE_BROWSER_WINDOW_ACTIVATABLE(plugin));

    if (location) {
        gchar *terminal;
        gchar *local;
        gchar *argv[2];

        terminal = get_terminal(plugin);
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

static void bedit_file_browser_window_activatable_update_state(
    BeditWindowActivatable *activatable
) {
    BeditFileBrowserWindowActivatablePrivate *priv;
    BeditFileBrowserWindowActivatable *plugin;
    BeditDocument *doc;

    plugin = BEDIT_FILE_BROWSER_WINDOW_ACTIVATABLE(activatable);
    priv = bedit_file_browser_window_activatable_get_instance_private(plugin);
    doc = bedit_window_get_active_document(priv->window);

    bedit_file_browser_widget_set_active_root_enabled(
        priv->tree_widget, doc != NULL && !bedit_document_is_untitled(doc)
    );
}

static void bedit_file_browser_window_activatable_activate(
    BeditWindowActivatable *activatable
) {
    BeditFileBrowserWindowActivatable *plugin;
    BeditFileBrowserWindowActivatablePrivate *priv;
    GtkWidget *action_area;
    GtkWidget *action_area_button_image;
    GtkWidget *popover;
    BeditFileBrowserStore *store;

    plugin = BEDIT_FILE_BROWSER_WINDOW_ACTIVATABLE(activatable);
    priv = bedit_file_browser_window_activatable_get_instance_private(plugin);

    /* Setup tree widget. */
    priv->tree_widget =
        BEDIT_FILE_BROWSER_WIDGET(bedit_file_browser_widget_new()
    );

    g_signal_connect(
        priv->tree_widget, "location-activated",
        G_CALLBACK(on_location_activated_cb), priv->window
    );

    g_signal_connect(
        priv->tree_widget, "error",
        G_CALLBACK(on_error_cb), plugin
    );

    g_signal_connect(
        priv->tree_widget, "confirm-delete",
        G_CALLBACK(on_confirm_delete_cb), plugin
    );

    g_signal_connect(
        priv->tree_widget, "confirm-no-trash",
        G_CALLBACK(on_confirm_no_trash_cb), priv->window
    );

    g_signal_connect(
        priv->tree_widget, "open-in-terminal",
        G_CALLBACK(open_in_terminal), plugin
    );

    g_signal_connect(
        priv->tree_widget, "set-active-root",
        G_CALLBACK(set_active_root), plugin
    );

    g_settings_bind(
        priv->settings, FILEBROWSER_FILTER_PATTERN,
        priv->tree_widget, FILEBROWSER_FILTER_PATTERN,
        G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET
    );

    /* Setup menu button widget. */
    action_area_button_image = gtk_image_new_from_icon_name(
        "folder-symbolic", GTK_ICON_SIZE_BUTTON
    );

    priv->action_area_button = gtk_menu_button_new();
    gtk_widget_set_valign(GTK_WIDGET(priv->action_area_button), GTK_ALIGN_CENTER);

    gtk_container_add(
        GTK_CONTAINER(priv->action_area_button), action_area_button_image
    );

    /* Connect using popover. */
    popover = gtk_popover_new(priv->action_area_button);
    gtk_menu_button_set_popover(
        GTK_MENU_BUTTON(priv->action_area_button), popover
    );

    g_object_set(
        G_OBJECT(popover),
        "constrain-to", GTK_POPOVER_CONSTRAINT_WINDOW, NULL
    );

    // TODO scale to fit window.
    g_object_set(
        G_OBJECT(popover),
        "width-request", 350, NULL
    );
    g_object_set(
        G_OBJECT(popover),
        "height-request", 800, NULL
    );

    gtk_container_add(
        GTK_CONTAINER(popover), GTK_WIDGET(priv->tree_widget)
    );

    /* Add everything to the area to the left of the tab bar. */
    action_area = bedit_window_get_action_area(priv->window);
    gtk_container_add(
        GTK_CONTAINER(action_area), GTK_WIDGET(priv->action_area_button)
    );

    gtk_widget_show(action_area_button_image);
    gtk_widget_show(priv->action_area_button);
    gtk_widget_show(GTK_WIDGET(priv->tree_widget));

    /* Install nautilus preferences */
    install_nautilus_prefs(plugin);

    /* Connect signals to store the last visited location */
    store = bedit_file_browser_widget_get_browser_store(priv->tree_widget);

    g_settings_bind(
        priv->settings, FILEBROWSER_FILTER_MODE,
        store, FILEBROWSER_FILTER_MODE,
        G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET
    );

    g_settings_bind(
        priv->settings, FILEBROWSER_BINARY_PATTERNS,
        store, FILEBROWSER_BINARY_PATTERNS,
        G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET
    );

    g_signal_connect(
        store, "notify::virtual-root",
        G_CALLBACK(on_virtual_root_changed_cb), plugin
    );

    g_signal_connect(
        store, "rename",
        G_CALLBACK(on_rename_cb), priv->window
    );

    /* Register messages on the bus */
    bedit_file_browser_messages_register(priv->window, priv->tree_widget);

    bedit_file_browser_window_activatable_update_state(activatable);
}

static void bedit_file_browser_window_activatable_deactivate(
    BeditWindowActivatable *activatable
) {
    BeditFileBrowserWindowActivatable *plugin;
    BeditFileBrowserWindowActivatablePrivate *priv;
    GtkWidget *action_area;

    plugin = BEDIT_FILE_BROWSER_WINDOW_ACTIVATABLE(activatable);
    priv = bedit_file_browser_window_activatable_get_instance_private(plugin);

    /* Unregister messages from the bus */
    bedit_file_browser_messages_unregister(priv->window);

    /* Disconnect signals */
    if (priv->click_policy_handle) {
        g_signal_handler_disconnect(
            priv->nautilus_settings, priv->click_policy_handle
        );
    }

    if (priv->confirm_trash_handle) {
        g_signal_handler_disconnect(
            priv->nautilus_settings, priv->confirm_trash_handle
        );
    }

    action_area = bedit_window_get_action_area(priv->window);
    gtk_container_remove(
        GTK_CONTAINER(action_area), GTK_WIDGET(priv->action_area_button)
    );
}

static void bedit_file_browser_window_activatable_class_init(
    BeditFileBrowserWindowActivatableClass *klass
) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->dispose = bedit_file_browser_window_activatable_dispose;
    object_class->set_property = bedit_file_browser_window_activatable_set_property;
    object_class->get_property = bedit_file_browser_window_activatable_get_property;

    g_object_class_override_property(object_class, PROP_WINDOW, "window");
}

static void bedit_file_browser_window_activatable_class_finalize(
    BeditFileBrowserWindowActivatableClass *klass
) {}


static void bedit_window_activatable_iface_init(
    BeditWindowActivatableInterface *iface
) {
    iface->activate = bedit_file_browser_window_activatable_activate;
    iface->deactivate = bedit_file_browser_window_activatable_deactivate;
}

void _bedit_file_browser_window_activatable_register_type(
    GTypeModule *type_module
) {
    bedit_file_browser_window_activatable_register_type(type_module);
}










/* Callbacks */
static void on_location_activated_cb(
    BeditFileBrowserWidget *tree_widget, GFile *location, BeditWindow *window
) {
    g_return_if_fail(BEDIT_IS_FILE_BROWSER_WIDGET(tree_widget));
    g_return_if_fail(BEDIT_IS_WINDOW(window));

    bedit_commands_load_location(window, location, NULL, 0, 0);
}

static void on_error_cb(
    BeditFileBrowserWidget *tree_widget, guint code, gchar const *message,
    BeditFileBrowserWindowActivatable *plugin
) {
    BeditFileBrowserWindowActivatablePrivate *priv;
    gchar *title;
    GtkWidget *dlg;

    g_return_if_fail(BEDIT_IS_FILE_BROWSER_WIDGET(tree_widget));
    g_return_if_fail(BEDIT_IS_FILE_BROWSER_WINDOW_ACTIVATABLE(plugin));

    priv = bedit_file_browser_window_activatable_get_instance_private(plugin);

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
        GTK_WINDOW(priv->window),
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
    BeditWindow *window
) {
    GList *documents;
    GList *item;

    g_return_if_fail(BEDIT_IS_FILE_BROWSER_STORE(store));
    g_return_if_fail(BEDIT_IS_WINDOW(window));

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

static void on_virtual_root_changed_cb(
    BeditFileBrowserStore *store, GParamSpec *param,
    BeditFileBrowserWindowActivatable *plugin
) {
    BeditFileBrowserWindowActivatablePrivate *priv;
    GFile *virtual_root;

    g_return_if_fail(BEDIT_IS_FILE_BROWSER_STORE(store));
    g_return_if_fail(BEDIT_IS_FILE_BROWSER_WINDOW_ACTIVATABLE(plugin));

    priv = bedit_file_browser_window_activatable_get_instance_private(plugin);

    virtual_root = bedit_file_browser_store_get_virtual_root(store);

    if (virtual_root) {
        _bedit_window_set_default_location(priv->window, virtual_root);
    }
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
    BeditFileBrowserWidget *widget, GList *files, BeditWindow *window
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
        window, GTK_MESSAGE_QUESTION, message, secondary, _("_Delete")
    );
    g_free(secondary);

    return result;
}

static gboolean on_confirm_delete_cb(
    BeditFileBrowserWidget *widget, BeditFileBrowserStore *store, GList *paths,
    BeditFileBrowserWindowActivatable *plugin
) {
    BeditFileBrowserWindowActivatablePrivate *priv;
    gchar *normal;
    gchar *message;
    gchar *secondary;
    gboolean result;

    g_return_val_if_fail(BEDIT_IS_FILE_BROWSER_WIDGET(widget), FALSE);
    g_return_val_if_fail(BEDIT_IS_FILE_BROWSER_STORE(store), FALSE);
    g_return_val_if_fail(BEDIT_IS_FILE_BROWSER_WINDOW_ACTIVATABLE(plugin), FALSE);

    priv = bedit_file_browser_window_activatable_get_instance_private(plugin);

    if (!priv->confirm_trash) {
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
        priv->window, GTK_MESSAGE_QUESTION, message, secondary, _("_Delete")
    );

    g_free(message);

    return result;
}
