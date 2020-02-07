/*
 * bedit-file-browser-plugin.c - Bedit plugin providing easy file access
 * from the sidepanel
 *
 * Copyright (C) 2006 - Jesse van den Kieboom <jesse@icecrew.nl>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
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
#include "bedit-file-browser-plugin.h"
#include "bedit-file-browser-utils.h"
#include "bedit-file-browser-widget.h"

#define FILEBROWSER_BASE_SETTINGS "com.bwhmather.bedit.plugins.filebrowser"
#define FILEBROWSER_TREE_VIEW "tree-view"
#define FILEBROWSER_FILTER_MODE "filter-mode"
#define FILEBROWSER_FILTER_PATTERN "filter-pattern"
#define FILEBROWSER_BINARY_PATTERNS "binary-patterns"

#define NAUTILUS_BASE_SETTINGS "org.gnome.nautilus.preferences"
#define NAUTILUS_FALLBACK_SETTINGS                                             \
    "com.bwhmather.bedit.plugins.filebrowser.nautilus"
#define NAUTILUS_CLICK_POLICY_KEY "click-policy"
#define NAUTILUS_CONFIRM_TRASH_KEY "confirm-trash"

#define TERMINAL_BASE_SETTINGS "org.gnome.desktop.default-applications.terminal"
#define TERMINAL_EXEC_KEY "exec"

struct _BeditFileBrowserPluginPrivate {
    GSettings *settings;
    GSettings *nautilus_settings;
    GSettings *terminal_settings;

    BeditWindow *window;

    GtkWidget *action_area_button;
    BeditFileBrowserWidget *tree_widget;
    gboolean auto_root;
    gulong end_loading_handle;
    gboolean confirm_trash;

    guint click_policy_handle;
    guint confirm_trash_handle;
};

enum { PROP_0, PROP_WINDOW };

static void bedit_window_activatable_iface_init(
    BeditWindowActivatableInterface *iface);

static void on_location_activated_cb(
    BeditFileBrowserWidget *widget, GFile *location, BeditWindow *window);
static void on_error_cb(
    BeditFileBrowserWidget *widget, guint code, gchar const *message,
    BeditFileBrowserPlugin *plugin);
static void on_virtual_root_changed_cb(
    BeditFileBrowserStore *model, GParamSpec *param,
    BeditFileBrowserPlugin *plugin);
static void on_rename_cb(
    BeditFileBrowserStore *model, GFile *oldfile, GFile *newfile,
    BeditWindow *window);
static gboolean on_confirm_delete_cb(
    BeditFileBrowserWidget *widget, BeditFileBrowserStore *store, GList *rows,
    BeditFileBrowserPlugin *plugin);
static gboolean on_confirm_no_trash_cb(
    BeditFileBrowserWidget *widget, GList *files, BeditWindow *window);

G_DEFINE_DYNAMIC_TYPE_EXTENDED(
    BeditFileBrowserPlugin, bedit_file_browser_plugin, G_TYPE_OBJECT, 0,
    G_ADD_PRIVATE_DYNAMIC(BeditFileBrowserPlugin) G_IMPLEMENT_INTERFACE_DYNAMIC(
        BEDIT_TYPE_WINDOW_ACTIVATABLE, bedit_window_activatable_iface_init)

        bedit_file_browser_enum_and_flag_register_type(type_module);
    _bedit_file_bookmarks_store_register_type(type_module);
    _bedit_file_browser_store_register_type(type_module);
    _bedit_file_browser_view_register_type(type_module);
    _bedit_file_browser_widget_register_type(type_module);)

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

static void bedit_file_browser_plugin_init(BeditFileBrowserPlugin *plugin) {
    plugin->priv = bedit_file_browser_plugin_get_instance_private(plugin);

    plugin->priv->settings = g_settings_new(FILEBROWSER_BASE_SETTINGS);
    plugin->priv->terminal_settings = g_settings_new(TERMINAL_BASE_SETTINGS);
    plugin->priv->nautilus_settings = settings_try_new(NAUTILUS_BASE_SETTINGS);

    if (plugin->priv->nautilus_settings == NULL) {
        plugin->priv->nautilus_settings =
            g_settings_new(NAUTILUS_FALLBACK_SETTINGS);
    }
}

static void bedit_file_browser_plugin_dispose(GObject *object) {
    BeditFileBrowserPlugin *plugin = BEDIT_FILE_BROWSER_PLUGIN(object);

    g_clear_object(&plugin->priv->settings);
    g_clear_object(&plugin->priv->nautilus_settings);
    g_clear_object(&plugin->priv->terminal_settings);
    g_clear_object(&plugin->priv->window);

    G_OBJECT_CLASS(bedit_file_browser_plugin_parent_class)->dispose(object);
}

static void bedit_file_browser_plugin_set_property(
    GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) {
    BeditFileBrowserPlugin *plugin = BEDIT_FILE_BROWSER_PLUGIN(object);

    switch (prop_id) {
    case PROP_WINDOW:
        plugin->priv->window = BEDIT_WINDOW(g_value_dup_object(value));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void bedit_file_browser_plugin_get_property(
    GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) {
    BeditFileBrowserPlugin *plugin = BEDIT_FILE_BROWSER_PLUGIN(object);

    switch (prop_id) {
    case PROP_WINDOW:
        g_value_set_object(value, plugin->priv->window);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void on_end_loading_cb(
    BeditFileBrowserStore *store, GtkTreeIter *iter,
    BeditFileBrowserPlugin *plugin) {
    BeditFileBrowserPluginPrivate *priv = plugin->priv;

    /* Disconnect the signal */
    g_signal_handler_disconnect(store, priv->end_loading_handle);
    priv->end_loading_handle = 0;
    priv->auto_root = FALSE;
}

static void prepare_auto_root(BeditFileBrowserPlugin *plugin) {
    BeditFileBrowserPluginPrivate *priv = plugin->priv;
    BeditFileBrowserStore *store;

    priv->auto_root = TRUE;

    store = bedit_file_browser_widget_get_browser_store(priv->tree_widget);

    if (priv->end_loading_handle != 0) {
        g_signal_handler_disconnect(store, priv->end_loading_handle);
        priv->end_loading_handle = 0;
    }

    priv->end_loading_handle = g_signal_connect(
        store, "end-loading", G_CALLBACK(on_end_loading_cb), plugin);
}

static void on_click_policy_changed(
    GSettings *settings, const gchar *key, BeditFileBrowserPlugin *plugin) {
    BeditFileBrowserPluginPrivate *priv = plugin->priv;
    BeditFileBrowserViewClickPolicy policy;
    BeditFileBrowserView *view;

    policy = g_settings_get_enum(settings, key);

    view = bedit_file_browser_widget_get_browser_view(priv->tree_widget);
    bedit_file_browser_view_set_click_policy(view, policy);
}

static void on_confirm_trash_changed(
    GSettings *settings, const gchar *key, BeditFileBrowserPlugin *plugin) {
    plugin->priv->confirm_trash = g_settings_get_boolean(settings, key);
}

static void install_nautilus_prefs(BeditFileBrowserPlugin *plugin) {
    BeditFileBrowserPluginPrivate *priv = plugin->priv;
    gboolean prefb;
    BeditFileBrowserViewClickPolicy policy;
    BeditFileBrowserView *view;

    /* Get click_policy */
    policy =
        g_settings_get_enum(priv->nautilus_settings, NAUTILUS_CLICK_POLICY_KEY);

    view = bedit_file_browser_widget_get_browser_view(priv->tree_widget);
    bedit_file_browser_view_set_click_policy(view, policy);

    priv->click_policy_handle = g_signal_connect(
        priv->nautilus_settings, "changed::" NAUTILUS_CLICK_POLICY_KEY,
        G_CALLBACK(on_click_policy_changed), plugin);

    /* Get confirm_trash */
    prefb = g_settings_get_boolean(
        priv->nautilus_settings, NAUTILUS_CONFIRM_TRASH_KEY);

    priv->confirm_trash = prefb;

    priv->confirm_trash_handle = g_signal_connect(
        priv->nautilus_settings, "changed::" NAUTILUS_CONFIRM_TRASH_KEY,
        G_CALLBACK(on_confirm_trash_changed), plugin);
}

static void set_root_from_doc(
    BeditFileBrowserPlugin *plugin, BeditDocument *doc) {
    BeditFileBrowserPluginPrivate *priv = plugin->priv;
    GtkSourceFile *file;
    GFile *location;
    GFile *parent;

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
        bedit_file_browser_widget_set_root(priv->tree_widget, parent, TRUE);

        g_object_unref(parent);
    }
}

static void set_active_root(
    BeditFileBrowserWidget *widget, BeditFileBrowserPlugin *plugin) {
    set_root_from_doc(
        plugin, bedit_window_get_active_document(plugin->priv->window));
}

static gchar *get_terminal(BeditFileBrowserPlugin *plugin) {
    BeditFileBrowserPluginPrivate *priv = plugin->priv;
    gchar *terminal;

    terminal =
        g_settings_get_string(priv->terminal_settings, TERMINAL_EXEC_KEY);

    if (terminal == NULL) {
        const gchar *term = g_getenv("TERM");

        if (term != NULL)
            terminal = g_strdup(term);
        else
            terminal = g_strdup("xterm");
    }

    return terminal;
}

static void open_in_terminal(
    BeditFileBrowserWidget *widget, GFile *location,
    BeditFileBrowserPlugin *plugin) {
    if (location) {
        gchar *terminal;
        gchar *local;
        gchar *argv[2];

        terminal = get_terminal(plugin);
        local = g_file_get_path(location);

        argv[0] = terminal;
        argv[1] = NULL;

        g_spawn_async(
            local, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);

        g_free(terminal);
        g_free(local);
    }
}

static void bedit_file_browser_plugin_update_state(
    BeditWindowActivatable *activatable) {
    BeditFileBrowserPluginPrivate *priv =
        BEDIT_FILE_BROWSER_PLUGIN(activatable)->priv;
    BeditDocument *doc;

    doc = bedit_window_get_active_document(priv->window);
    bedit_file_browser_widget_set_active_root_enabled(
        priv->tree_widget, doc != NULL && !bedit_document_is_untitled(doc));
}

static void bedit_file_browser_plugin_activate(
    BeditWindowActivatable *activatable) {
    BeditFileBrowserPlugin *plugin = BEDIT_FILE_BROWSER_PLUGIN(activatable);
    BeditFileBrowserPluginPrivate *priv;
    GtkWidget *action_area;
    GtkWidget *action_area_button_image;
    GtkWidget *popover;
    BeditFileBrowserStore *store;

    priv = plugin->priv;

    /* Setup tree widget. */
    priv->tree_widget =
        BEDIT_FILE_BROWSER_WIDGET(bedit_file_browser_widget_new());

    g_signal_connect(
        priv->tree_widget, "location-activated",
        G_CALLBACK(on_location_activated_cb), priv->window);

    g_signal_connect(
        priv->tree_widget, "error", G_CALLBACK(on_error_cb), plugin);

    g_signal_connect(
        priv->tree_widget, "confirm-delete", G_CALLBACK(on_confirm_delete_cb),
        plugin);

    g_signal_connect(
        priv->tree_widget, "confirm-no-trash",
        G_CALLBACK(on_confirm_no_trash_cb), priv->window);

    g_signal_connect(
        priv->tree_widget, "open-in-terminal", G_CALLBACK(open_in_terminal),
        plugin);

    g_signal_connect(
        priv->tree_widget, "set-active-root", G_CALLBACK(set_active_root),
        plugin);

    g_settings_bind(
        priv->settings, FILEBROWSER_FILTER_PATTERN, priv->tree_widget,
        FILEBROWSER_FILTER_PATTERN, G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET);

    /* Setup menu button widget. */
    action_area_button_image = gtk_image_new_from_icon_name(
        "folder-symbolic", GTK_ICON_SIZE_BUTTON);

    priv->action_area_button = gtk_menu_button_new();
    gtk_container_add(
        GTK_CONTAINER(priv->action_area_button), action_area_button_image);

    /* Connect using popover. */
    popover = gtk_popover_new(priv->action_area_button);
    gtk_menu_button_set_popover(
        GTK_MENU_BUTTON(priv->action_area_button), popover);

    g_object_set(
        G_OBJECT(popover),
        "constrain-to", GTK_POPOVER_CONSTRAINT_WINDOW, NULL);

    // TODO scale to fit window.
    g_object_set(
        G_OBJECT(popover),
        "width-request", 350, NULL);
    g_object_set(
        G_OBJECT(popover),
        "height-request", 800, NULL);

    gtk_container_add(
        GTK_CONTAINER(popover), GTK_WIDGET(priv->tree_widget));

    /* Add everything to the area to the left of the tab bar. */
    action_area = bedit_window_get_action_area(priv->window);
    gtk_container_add(
        GTK_CONTAINER(action_area), GTK_WIDGET(priv->action_area_button));

    gtk_widget_show(action_area_button_image);
    gtk_widget_show(priv->action_area_button);
    gtk_widget_show(GTK_WIDGET(priv->tree_widget));

    /* Install nautilus preferences */
    install_nautilus_prefs(plugin);

    /* Connect signals to store the last visited location */
    store = bedit_file_browser_widget_get_browser_store(priv->tree_widget);

    g_settings_bind(
        priv->settings, FILEBROWSER_FILTER_MODE, store, FILEBROWSER_FILTER_MODE,
        G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET);

    g_settings_bind(
        priv->settings, FILEBROWSER_BINARY_PATTERNS, store,
        FILEBROWSER_BINARY_PATTERNS, G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET);

    g_signal_connect(
        store, "notify::virtual-root", G_CALLBACK(on_virtual_root_changed_cb),
        plugin);

    g_signal_connect(store, "rename", G_CALLBACK(on_rename_cb), priv->window);

    /* Register messages on the bus */
    bedit_file_browser_messages_register(priv->window, priv->tree_widget);

    bedit_file_browser_plugin_update_state(activatable);
}

static void bedit_file_browser_plugin_deactivate(
    BeditWindowActivatable *activatable) {
    BeditFileBrowserPlugin *plugin = BEDIT_FILE_BROWSER_PLUGIN(activatable);
    BeditFileBrowserPluginPrivate *priv = plugin->priv;
    GtkWidget *action_area;

    /* Unregister messages from the bus */
    bedit_file_browser_messages_unregister(priv->window);

    /* Disconnect signals */
    if (priv->click_policy_handle) {
        g_signal_handler_disconnect(
            priv->nautilus_settings, priv->click_policy_handle);
    }

    if (priv->confirm_trash_handle) {
        g_signal_handler_disconnect(
            priv->nautilus_settings, priv->confirm_trash_handle);
    }

    action_area = bedit_window_get_action_area(priv->window);
    gtk_container_remove(
        GTK_CONTAINER(action_area), GTK_WIDGET(priv->action_area_button));
}

static void bedit_file_browser_plugin_class_init(
    BeditFileBrowserPluginClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->dispose = bedit_file_browser_plugin_dispose;
    object_class->set_property = bedit_file_browser_plugin_set_property;
    object_class->get_property = bedit_file_browser_plugin_get_property;

    g_object_class_override_property(object_class, PROP_WINDOW, "window");
}

static void bedit_window_activatable_iface_init(
    BeditWindowActivatableInterface *iface) {
    iface->activate = bedit_file_browser_plugin_activate;
    iface->deactivate = bedit_file_browser_plugin_deactivate;
    iface->update_state = bedit_file_browser_plugin_update_state;
}

static void bedit_file_browser_plugin_class_finalize(
    BeditFileBrowserPluginClass *klass) {}

/* Callbacks */
static void on_location_activated_cb(
    BeditFileBrowserWidget *tree_widget, GFile *location, BeditWindow *window) {
    bedit_commands_load_location(window, location, NULL, 0, 0);
}

static void on_error_cb(
    BeditFileBrowserWidget *tree_widget, guint code, gchar const *message,
    BeditFileBrowserPlugin *plugin) {
    BeditFileBrowserPluginPrivate *priv = plugin->priv;
    gchar *title;
    GtkWidget *dlg;

    /* Do not show the error when the root has been set automatically */
    if (priv->auto_root &&
        (code == BEDIT_FILE_BROWSER_ERROR_SET_ROOT ||
         code == BEDIT_FILE_BROWSER_ERROR_LOAD_DIRECTORY)) {
        /* Show bookmarks */
        bedit_file_browser_widget_show_bookmarks(priv->tree_widget);
        return;
    }

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
            "An error occurred while opening a directory in the file manager");
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
        GTK_BUTTONS_OK, "%s", title);
    gtk_message_dialog_format_secondary_text(
        GTK_MESSAGE_DIALOG(dlg), "%s", message);

    gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);
}

static void on_rename_cb(
    BeditFileBrowserStore *store, GFile *oldfile, GFile *newfile,
    BeditWindow *window) {
    GList *documents;
    GList *item;

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
    BeditFileBrowserPlugin *plugin) {
    GFile *virtual_root;

    g_return_if_fail(BEDIT_IS_FILE_BROWSER_STORE(store));
    g_return_if_fail(BEDIT_IS_FILE_BROWSER_PLUGIN(plugin));

    virtual_root = bedit_file_browser_store_get_virtual_root(store);

    if (virtual_root) {
        _bedit_window_set_default_location(plugin->priv->window, virtual_root);
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
        model, &iter, BEDIT_FILE_BROWSER_STORE_COLUMN_LOCATION, &location, -1);

    if (location) {
        ret = bedit_file_browser_utils_file_basename(location);
        g_object_unref(location);
    }

    return ret;
}

static gboolean on_confirm_no_trash_cb(
    BeditFileBrowserWidget *widget, GList *files, BeditWindow *window) {
    gchar *normal;
    gchar *message;
    gchar *secondary;
    gboolean result;

    message =
        _("Cannot move file to trash, do you\nwant to delete permanently?");

    if (files->next == NULL) {
        normal = bedit_file_browser_utils_file_basename(G_FILE(files->data));
        secondary = g_strdup_printf(
            _("The file “%s” cannot be moved to the trash."), normal);
        g_free(normal);
    } else {
        secondary =
            g_strdup(_("The selected files cannot be moved to the trash."));
    }

    result = bedit_file_browser_utils_confirmation_dialog(
        window, GTK_MESSAGE_QUESTION, message, secondary, _("_Delete"));
    g_free(secondary);

    return result;
}

static gboolean on_confirm_delete_cb(
    BeditFileBrowserWidget *widget, BeditFileBrowserStore *store, GList *paths,
    BeditFileBrowserPlugin *plugin) {
    BeditFileBrowserPluginPrivate *priv = plugin->priv;
    gchar *normal;
    gchar *message;
    gchar *secondary;
    gboolean result;

    if (!priv->confirm_trash)
        return TRUE;

    if (paths->next == NULL) {
        normal = get_filename_from_path(
            GTK_TREE_MODEL(store), (GtkTreePath *)(paths->data));
        message = g_strdup_printf(
            _("Are you sure you want to permanently delete “%s”?"), normal);
        g_free(normal);
    } else {
        message = g_strdup(_(
            "Are you sure you want to permanently delete the selected files?"));
    }

    secondary = _("If you delete an item, it is permanently lost.");

    result = bedit_file_browser_utils_confirmation_dialog(
        priv->window, GTK_MESSAGE_QUESTION, message, secondary, _("_Delete"));

    g_free(message);

    return result;
}

G_MODULE_EXPORT void peas_register_types(PeasObjectModule *module) {
    bedit_file_browser_plugin_register_type(G_TYPE_MODULE(module));

    peas_object_module_register_extension_type(
        module, BEDIT_TYPE_WINDOW_ACTIVATABLE, BEDIT_TYPE_FILE_BROWSER_PLUGIN);
}

