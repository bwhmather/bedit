/*
 * bedit-file-browser-messages.c
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * Based on gedit-file-browser-messages.c from Gedit.
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

#include "bedit-file-browser-messages.h"
#include "bedit-file-browser-store.h"
#include "messages/messages.h"

#include <bedit/bedit-message.h>

#define MESSAGE_OBJECT_PATH "/plugins/filebrowser"
#define WINDOW_DATA_KEY "BeditFileBrowserMessagesWindowData"

#define BUS_CONNECT(bus, name, data)                                        \
    bedit_message_bus_connect(                                              \
        bus, MESSAGE_OBJECT_PATH, #name,                                    \
        (BeditMessageCallback)message_##name##_cb, data, NULL               \
    )
#define BUS_DISCONNECT(bus, name, data)                                     \
    bedit_message_bus_disconnect_by_func(                                   \
        bus, MESSAGE_OBJECT_PATH, #name,                                    \
        (BeditMessageCallback)message_##name##_cb, data                     \
    )

typedef struct {
    BeditWindow *window;
    BeditMessage *message;
} MessageCacheData;

typedef struct {
    guint row_inserted_id;
    guint before_row_deleted_id;
    guint root_changed_id;
    guint begin_loading_id;
    guint end_loading_id;

    BeditMessageBus *bus;
    BeditFileBrowserWidget *widget;
    GHashTable *row_tracking;

    GHashTable *filters;
} WindowData;

typedef struct {
    gulong id;

    BeditWindow *window;
    BeditMessage *message;
} FilterData;

static WindowData *window_data_new(
    BeditWindow *window, BeditFileBrowserWidget *widget
) {
    WindowData *data = g_slice_new(WindowData);

    data->bus = bedit_window_get_message_bus(window);
    data->widget = widget;
    data->row_tracking = g_hash_table_new_full(
        g_str_hash, g_str_equal,
        (GDestroyNotify)g_free,
        (GDestroyNotify)gtk_tree_row_reference_free
    );

    data->filters = g_hash_table_new_full(
        g_str_hash, g_str_equal, (GDestroyNotify)g_free, NULL
    );

    g_object_set_data(G_OBJECT(window), WINDOW_DATA_KEY, data);

    return data;
}

static WindowData *get_window_data(BeditWindow *window) {
    return (WindowData *)(g_object_get_data(G_OBJECT(window), WINDOW_DATA_KEY));
}

static void window_data_free(BeditWindow *window) {
    WindowData *data = get_window_data(window);

    g_hash_table_destroy(data->row_tracking);
    g_hash_table_destroy(data->filters);

    g_slice_free(WindowData, data);

    g_object_set_data(G_OBJECT(window), WINDOW_DATA_KEY, NULL);
}

static FilterData *filter_data_new(BeditWindow *window, BeditMessage *message) {
    FilterData *data = g_slice_new(FilterData);
    WindowData *wdata;

    data->window = window;
    data->id = 0;
    data->message = message;

    wdata = get_window_data(window);

    g_hash_table_insert(
        wdata->filters,
        bedit_message_type_identifier(
            bedit_message_get_object_path(message),
            bedit_message_get_method(message)
        ),
        data
    );

    return data;
}

static void filter_data_free(FilterData *data) {
    WindowData *wdata = get_window_data(data->window);
    gchar *identifier;

    identifier = bedit_message_type_identifier(
        bedit_message_get_object_path(data->message),
        bedit_message_get_method(data->message)
    );

    g_hash_table_remove(wdata->filters, identifier);
    g_free(identifier);

    g_object_unref(data->message);
    g_slice_free(FilterData, data);
}

static GtkTreePath *track_row_lookup(WindowData *data, const gchar *id) {
    GtkTreeRowReference *ref;

    ref = (GtkTreeRowReference *)g_hash_table_lookup(data->row_tracking, id);

    if (!ref) {
        return NULL;
    }

    return gtk_tree_row_reference_get_path(ref);
}

static void message_cache_data_free(MessageCacheData *data) {
    g_object_unref(data->message);
    g_slice_free(MessageCacheData, data);
}

static MessageCacheData *message_cache_data_new(
    BeditWindow *window, BeditMessage *message
) {
    MessageCacheData *data = g_slice_new(MessageCacheData);

    data->window = window;
    data->message = message;

    return data;
}

static void message_get_root_cb(
    BeditMessageBus *bus, BeditMessage *message, WindowData *data
) {
    BeditFileBrowserStore *store;
    GFile *location;

    store = bedit_file_browser_widget_get_browser_store(data->widget);
    location = bedit_file_browser_store_get_virtual_root(store);

    if (location) {
        g_object_set(message, "location", location, NULL);
        g_object_unref(location);
    }
}

static void message_set_root_cb(
    BeditMessageBus *bus, BeditMessage *message, WindowData *data
) {
    GFile *root;
    GFile *virtual = NULL;

    g_object_get(message, "location", &root, NULL);

    if (!root) {
        return;
    }

    g_object_get(message, "virtual", &virtual, NULL);

    if (virtual) {
        bedit_file_browser_widget_set_root_and_virtual_root(
            data->widget, root, virtual
        );
    } else {
        bedit_file_browser_widget_set_virtual_root(data->widget, root);
    }
}

static void message_set_emblem_cb(
    BeditMessageBus *bus, BeditMessage *message, WindowData *data
) {
    gchar *id = NULL;
    gchar *emblem_name = NULL;
    GtkTreePath *path;
    BeditFileBrowserStore *store;

    g_object_get(message, "id", &id, "emblem", &emblem_name, NULL);

    if (!id) {
        g_free(id);
        g_free(emblem_name);

        return;
    }

    path = track_row_lookup(data, id);

    if (path != NULL) {
        GtkTreeIter iter;
        GValue value = G_VALUE_INIT;
        GIcon *icon = NULL;
        GEmblem *emblem = NULL;

        if (emblem_name != NULL) {
            icon = g_icon_new_for_string(emblem_name, NULL);
            emblem = g_emblem_new(icon);
        }

        store = bedit_file_browser_widget_get_browser_store(data->widget);

        if (gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &iter, path)) {
            g_value_init(&value, G_TYPE_EMBLEM);
            g_value_set_object(&value, emblem);

            bedit_file_browser_store_set_value(
                store, &iter, BEDIT_FILE_BROWSER_STORE_COLUMN_EMBLEM, &value
            );

            g_value_unset(&value);
        }

        if (emblem) {
            g_object_unref(emblem);
        }

        if (icon) {
            g_object_unref(icon);
        }
    }

    g_free(id);
    g_free(emblem_name);
}

static void message_set_markup_cb(
    BeditMessageBus *bus, BeditMessage *message, WindowData *data
) {
    gchar *id = NULL;
    gchar *markup = NULL;
    GtkTreePath *path;
    BeditFileBrowserStore *store;

    g_object_get(message, "id", &id, "markup", &markup, NULL);

    if (!id) {
        g_free(id);
        g_free(markup);

        return;
    }

    path = track_row_lookup(data, id);

    if (path != NULL) {
        GtkTreeIter iter;
        GValue value = G_VALUE_INIT;

        store = bedit_file_browser_widget_get_browser_store(data->widget);

        if (gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &iter, path)) {
            if (markup == NULL) {
                gchar *name;

                gtk_tree_model_get(
                    GTK_TREE_MODEL(store), &iter,
                    BEDIT_FILE_BROWSER_STORE_COLUMN_NAME, &name,
                    -1
                );
                markup = g_markup_escape_text(name, -1);

                g_free(name);
            }

            g_value_init(&value, G_TYPE_STRING);
            g_value_set_string(&value, markup);

            bedit_file_browser_store_set_value(
                store, &iter, BEDIT_FILE_BROWSER_STORE_COLUMN_MARKUP, &value
            );

            g_value_unset(&value);
        }

        gtk_tree_path_free(path);
    }

    g_free(id);
    g_free(markup);
}

static gchar *item_id(const gchar *path, GFile *location) {
    gchar *uri;
    gchar *id;

    uri = g_file_get_uri(location);
    id = g_strconcat(path, "::", uri, NULL);
    g_free(uri);

    return id;
}

static gchar *track_row(
    WindowData *data, BeditFileBrowserStore *store,
    GtkTreePath *path, GFile *location
) {
    GtkTreeRowReference *ref;
    gchar *id;
    gchar *pathstr;

    pathstr = gtk_tree_path_to_string(path);
    id = item_id(pathstr, location);

    ref = gtk_tree_row_reference_new(GTK_TREE_MODEL(store), path);
    g_hash_table_insert(data->row_tracking, g_strdup(id), ref);

    g_free(pathstr);

    return id;
}

static void set_item_message(
    WindowData *data, GtkTreeIter *iter, GtkTreePath *path,
    BeditMessage *message
) {
    BeditFileBrowserStore *store;
    gchar *name;
    GFile *location;
    guint flags = 0;

    store = bedit_file_browser_widget_get_browser_store(data->widget);
    gtk_tree_model_get(
        GTK_TREE_MODEL(store), iter,
        BEDIT_FILE_BROWSER_STORE_COLUMN_NAME, &name,
        BEDIT_FILE_BROWSER_STORE_COLUMN_LOCATION, &location,
        BEDIT_FILE_BROWSER_STORE_COLUMN_FLAGS, &flags,
        -1
    );

    if (location) {
        gchar *track_id;

        if (path && gtk_tree_path_get_depth(path) != 0) {
            track_id = track_row(data, store, path, location);
        } else {
            track_id = NULL;
        }

        g_object_set(message, "id", track_id, "location", location, NULL);

        if (bedit_message_has(message, "name")) {
            g_object_set(message, "name", name, NULL);
        }

        if (bedit_message_has(message, "is_directory")) {
            g_object_set(message, "is_directory", FILE_IS_DIR(flags), NULL);
        }

        g_free(track_id);
        g_object_unref(location);
    }

    g_free(name);
}

static gboolean custom_message_filter_func(
    BeditFileBrowserWidget *widget, BeditFileBrowserStore *store,
    GtkTreeIter *iter, FilterData *data
) {
    WindowData *wdata = get_window_data(data->window);
    GFile *location;
    guint flags = 0;
    gboolean filter = FALSE;
    GtkTreePath *path;

    gtk_tree_model_get(
        GTK_TREE_MODEL(store), iter,
        BEDIT_FILE_BROWSER_STORE_COLUMN_LOCATION, &location,
        BEDIT_FILE_BROWSER_STORE_COLUMN_FLAGS, &flags,
        -1
    );

    if (!location || FILE_IS_DUMMY(flags)) {
        return FALSE;
    }

    path = gtk_tree_model_get_path(GTK_TREE_MODEL(store), iter);
    set_item_message(wdata, iter, path, data->message);
    gtk_tree_path_free(path);

    g_object_set(data->message, "filter", filter, NULL);

    bedit_message_bus_send_message_sync(wdata->bus, data->message);
    g_object_get(data->message, "filter", &filter, NULL);

    g_object_unref(location);

    return !filter;
}

static void message_add_filter_cb(
    BeditMessageBus *bus, BeditMessage *message, BeditWindow *window
) {
    const gchar *object_path = NULL;
    const gchar *method = NULL;
    gulong id;
    BeditMessage *cbmessage;
    FilterData *filter_data;
    WindowData *data;
    GType message_type;

    data = get_window_data(window);

    object_path = bedit_message_get_object_path(message);
    method = bedit_message_get_method(message);

    message_type = bedit_message_bus_lookup(bus, object_path, method);

    if (message_type == G_TYPE_INVALID) {
        return;
    }

    /* Check if the message type has the correct arguments */
    if (
        !bedit_message_type_check(message_type, "id", G_TYPE_STRING) ||
        !bedit_message_type_check(message_type, "location", G_TYPE_FILE) ||
        !bedit_message_type_check(
            message_type, "is-directory", G_TYPE_BOOLEAN
        ) ||
        !bedit_message_type_check(message_type, "filter", G_TYPE_BOOLEAN)
    ) {
        return;
    }

    cbmessage = g_object_new(
        message_type,
        "object-path", object_path,
        "method", method,
        "id", NULL,
        "location", NULL,
        "is-directory", FALSE,
        "filter", FALSE,
        NULL
    );

    /* Register the custom filter on the widget */
    filter_data = filter_data_new(window, cbmessage);

    id = bedit_file_browser_widget_add_filter(
        data->widget,
        (BeditFileBrowserWidgetFilterFunc)custom_message_filter_func,
        filter_data,
        (GDestroyNotify)filter_data_free
    );

    filter_data->id = id;
}

static void message_remove_filter_cb(
    BeditMessageBus *bus, BeditMessage *message, WindowData *data
) {
    gulong id = 0;

    g_object_get(message, "id", &id, NULL);

    if (!id) {
        return;
    }

    bedit_file_browser_widget_remove_filter(data->widget, id);
}

static void message_extend_context_menu_cb(
    BeditMessageBus *bus, BeditMessage *message, BeditWindow *window
) {
    WindowData *data;
    BeditMenuExtension *ext;

    data = get_window_data(window);

    ext = bedit_file_browser_widget_extend_context_menu(data->widget);

    g_object_set(message, "extension", ext, NULL);
    g_object_unref(ext);
}

static void message_up_cb(
    BeditMessageBus *bus, BeditMessage *message, WindowData *data
) {
    BeditFileBrowserStore *store = bedit_file_browser_widget_get_browser_store(
        data->widget
    );

    bedit_file_browser_store_set_virtual_root_up(store);
}

static void message_history_back_cb(
    BeditMessageBus *bus, BeditMessage *message, WindowData *data
) {
    bedit_file_browser_widget_history_back(data->widget);
}

static void message_history_forward_cb(
    BeditMessageBus *bus, BeditMessage *message, WindowData *data
) {
    bedit_file_browser_widget_history_forward(data->widget);
}

static void message_refresh_cb(
    BeditMessageBus *bus, BeditMessage *message, WindowData *data
) {
    bedit_file_browser_widget_refresh(data->widget);
}

static void message_set_show_hidden_cb(
    BeditMessageBus *bus, BeditMessage *message, WindowData *data
) {
    gboolean active = FALSE;
    BeditFileBrowserStore *store;
    BeditFileBrowserStoreFilterMode mode;

    g_object_get(message, "active", &active, NULL);

    store = bedit_file_browser_widget_get_browser_store(data->widget);
    mode = bedit_file_browser_store_get_filter_mode(store);

    if (active) {
        mode &= ~BEDIT_FILE_BROWSER_STORE_FILTER_MODE_HIDE_HIDDEN;
    } else {
        mode |= BEDIT_FILE_BROWSER_STORE_FILTER_MODE_HIDE_HIDDEN;
    }

    bedit_file_browser_store_set_filter_mode(store, mode);
}

static void message_set_show_binary_cb(
    BeditMessageBus *bus, BeditMessage *message, WindowData *data
) {
    gboolean active = FALSE;
    BeditFileBrowserStore *store;
    BeditFileBrowserStoreFilterMode mode;

    g_object_get(message, "active", &active, NULL);

    store = bedit_file_browser_widget_get_browser_store(data->widget);
    mode = bedit_file_browser_store_get_filter_mode(store);

    if (active) {
        mode &= ~BEDIT_FILE_BROWSER_STORE_FILTER_MODE_HIDE_BINARY;
    } else {
        mode |= BEDIT_FILE_BROWSER_STORE_FILTER_MODE_HIDE_BINARY;
    }

    bedit_file_browser_store_set_filter_mode(store, mode);
}

static void message_show_bookmarks_cb(
    BeditMessageBus *bus, BeditMessage *message, WindowData *data
) {
    bedit_file_browser_widget_show_bookmarks(data->widget);
}

static void message_show_files_cb(
    BeditMessageBus *bus, BeditMessage *message, WindowData *data
) {
    bedit_file_browser_widget_show_files(data->widget);
}

static void message_get_view_cb(
    BeditMessageBus *bus, BeditMessage *message, WindowData *data
) {
    BeditFileBrowserView *view;
    view = bedit_file_browser_widget_get_browser_view(data->widget);

    g_object_set(message, "view", view, NULL);
}

static void register_methods(
    BeditWindow *window, BeditFileBrowserWidget *widget
) {
    BeditMessageBus *bus = bedit_window_get_message_bus(window);
    WindowData *data = get_window_data(window);

    /* Register method calls */
    bedit_message_bus_register(
        bus, BEDIT_TYPE_FILE_BROWSER_MESSAGE_GET_ROOT,
        MESSAGE_OBJECT_PATH, "get_root"
    );

    bedit_message_bus_register(
        bus, BEDIT_TYPE_FILE_BROWSER_MESSAGE_SET_ROOT,
        MESSAGE_OBJECT_PATH, "set_root"
    );

    bedit_message_bus_register(
        bus, BEDIT_TYPE_FILE_BROWSER_MESSAGE_SET_EMBLEM,
        MESSAGE_OBJECT_PATH, "set_emblem"
    );

    bedit_message_bus_register(
        bus, BEDIT_TYPE_FILE_BROWSER_MESSAGE_SET_MARKUP,
        MESSAGE_OBJECT_PATH, "set_markup"
    );

    bedit_message_bus_register(
        bus, BEDIT_TYPE_FILE_BROWSER_MESSAGE_ADD_FILTER,
        MESSAGE_OBJECT_PATH, "add_filter"
    );

    bedit_message_bus_register(
        bus, BEDIT_TYPE_FILE_BROWSER_MESSAGE_ID,
        MESSAGE_OBJECT_PATH, "remove_filter"
    );

    bedit_message_bus_register(
        bus, BEDIT_TYPE_FILE_BROWSER_MESSAGE_EXTEND_CONTEXT_MENU,
        MESSAGE_OBJECT_PATH, "extend_context_menu"
    );

    bedit_message_bus_register(
        bus, BEDIT_TYPE_MESSAGE,
        MESSAGE_OBJECT_PATH, "up"
    );

    bedit_message_bus_register(
        bus, BEDIT_TYPE_MESSAGE,
        MESSAGE_OBJECT_PATH, "history_back"
    );

    bedit_message_bus_register(
        bus, BEDIT_TYPE_MESSAGE,
        MESSAGE_OBJECT_PATH, "history_forward"
    );

    bedit_message_bus_register(
        bus, BEDIT_TYPE_MESSAGE,
        MESSAGE_OBJECT_PATH, "refresh"
    );

    bedit_message_bus_register(
        bus, BEDIT_TYPE_FILE_BROWSER_MESSAGE_ACTIVATION,
        MESSAGE_OBJECT_PATH, "set_show_hidden"
    );

    bedit_message_bus_register(
        bus, BEDIT_TYPE_FILE_BROWSER_MESSAGE_ACTIVATION,
        MESSAGE_OBJECT_PATH, "set_show_binary"
    );

    bedit_message_bus_register(
        bus, BEDIT_TYPE_MESSAGE,
        MESSAGE_OBJECT_PATH, "show_bookmarks"
    );

    bedit_message_bus_register(
        bus, BEDIT_TYPE_MESSAGE,
        MESSAGE_OBJECT_PATH, "show_files"
    );

    bedit_message_bus_register(
        bus, BEDIT_TYPE_FILE_BROWSER_MESSAGE_GET_VIEW,
        MESSAGE_OBJECT_PATH, "get_view"
    );

    BUS_CONNECT(bus, get_root, data);
    BUS_CONNECT(bus, set_root, data);
    BUS_CONNECT(bus, set_emblem, data);
    BUS_CONNECT(bus, set_markup, data);
    BUS_CONNECT(bus, add_filter, window);
    BUS_CONNECT(bus, remove_filter, data);
    BUS_CONNECT(bus, extend_context_menu, window);

    BUS_CONNECT(bus, up, data);
    BUS_CONNECT(bus, history_back, data);
    BUS_CONNECT(bus, history_forward, data);

    BUS_CONNECT(bus, refresh, data);

    BUS_CONNECT(bus, set_show_hidden, data);
    BUS_CONNECT(bus, set_show_binary, data);

    BUS_CONNECT(bus, show_bookmarks, data);
    BUS_CONNECT(bus, show_files, data);

    BUS_CONNECT(bus, get_view, data);
}

static void store_row_inserted(
    BeditFileBrowserStore *store, GtkTreePath *path, GtkTreeIter *iter,
    MessageCacheData *data
) {
    guint flags = 0;

    gtk_tree_model_get(
        GTK_TREE_MODEL(store), iter,
        BEDIT_FILE_BROWSER_STORE_COLUMN_FLAGS, &flags,
        -1
    );

    if (!FILE_IS_DUMMY(flags) && !FILE_IS_FILTERED(flags)) {
        WindowData *wdata = get_window_data(data->window);

        set_item_message(wdata, iter, path, data->message);
        bedit_message_bus_send_message_sync(wdata->bus, data->message);
    }
}

static void store_before_row_deleted(
    BeditFileBrowserStore *store, GtkTreePath *path, MessageCacheData *data
) {
    GtkTreeIter iter;
    guint flags = 0;

    if (!gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &iter, path)) {
        return;
    }

    gtk_tree_model_get(
        GTK_TREE_MODEL(store), &iter,
        BEDIT_FILE_BROWSER_STORE_COLUMN_FLAGS, &flags,
        -1
    );

    if (!FILE_IS_DUMMY(flags) && !FILE_IS_FILTERED(flags)) {
        WindowData *wdata = get_window_data(data->window);
        gchar *id;

        set_item_message(wdata, &iter, path, data->message);

        /* Must get the ID before the plugin can modify it */
        g_object_get(data->message, "id", &id, NULL);

        bedit_message_bus_send_message_sync(wdata->bus, data->message);

        g_hash_table_remove(wdata->row_tracking, id);
        g_free(id);
    }
}

static void store_virtual_root_changed(
    BeditFileBrowserStore *store, GParamSpec *spec, MessageCacheData *data
) {
    WindowData *wdata = get_window_data(data->window);
    GFile *vroot;

    vroot = bedit_file_browser_store_get_virtual_root(store);

    if (!vroot) {
        return;
    }

    g_object_set(data->message, "location", vroot, NULL);

    bedit_message_bus_send_message_sync(wdata->bus, data->message);

    g_object_unref(vroot);
}

static void store_begin_loading(
    BeditFileBrowserStore *store, GtkTreeIter *iter, MessageCacheData *data
) {
    GtkTreePath *path;
    WindowData *wdata = get_window_data(data->window);

    path = gtk_tree_model_get_path(GTK_TREE_MODEL(store), iter);

    set_item_message(wdata, iter, path, data->message);

    bedit_message_bus_send_message_sync(wdata->bus, data->message);
    gtk_tree_path_free(path);
}

static void store_end_loading(
    BeditFileBrowserStore *store, GtkTreeIter *iter, MessageCacheData *data
) {
    GtkTreePath *path;
    WindowData *wdata = get_window_data(data->window);

    path = gtk_tree_model_get_path(GTK_TREE_MODEL(store), iter);

    set_item_message(wdata, iter, path, data->message);

    bedit_message_bus_send_message_sync(wdata->bus, data->message);
    gtk_tree_path_free(path);
}

static void register_signals(
    BeditWindow *window, BeditFileBrowserWidget *widget
) {
    BeditMessageBus *bus = bedit_window_get_message_bus(window);
    BeditFileBrowserStore *store;

    BeditMessage *message;
    WindowData *data;

    /* Register signals */
    bedit_message_bus_register(
        bus, BEDIT_TYPE_FILE_BROWSER_MESSAGE_ID_LOCATION,
        MESSAGE_OBJECT_PATH, "root_changed"
    );

    bedit_message_bus_register(
        bus, BEDIT_TYPE_FILE_BROWSER_MESSAGE_ID_LOCATION,
        MESSAGE_OBJECT_PATH, "begin_loading"
    );

    bedit_message_bus_register(
        bus, BEDIT_TYPE_FILE_BROWSER_MESSAGE_ID_LOCATION,
        MESSAGE_OBJECT_PATH, "end_loading"
    );

    bedit_message_bus_register(
        bus, BEDIT_TYPE_FILE_BROWSER_MESSAGE_ID_LOCATION,
        MESSAGE_OBJECT_PATH, "inserted"
    );

    bedit_message_bus_register(
        bus, BEDIT_TYPE_FILE_BROWSER_MESSAGE_ID_LOCATION,
        MESSAGE_OBJECT_PATH, "deleted"
    );

    store = bedit_file_browser_widget_get_browser_store(widget);

    message = g_object_new(
        BEDIT_TYPE_FILE_BROWSER_MESSAGE_ID_LOCATION,
        "object-path", MESSAGE_OBJECT_PATH, "method", "inserted", NULL
    );

    data = get_window_data(window);

    data->row_inserted_id = g_signal_connect_data(
        store, "row-inserted",
        G_CALLBACK(store_row_inserted), message_cache_data_new(window, message),
        (GClosureNotify)message_cache_data_free, 0
    );

    message = g_object_new(
        BEDIT_TYPE_FILE_BROWSER_MESSAGE_ID_LOCATION,
        "object-path", MESSAGE_OBJECT_PATH, "method", "deleted", NULL
    );

    data->before_row_deleted_id = g_signal_connect_data(
        store, "before-row-deleted",
        G_CALLBACK(store_before_row_deleted),
        message_cache_data_new(window, message),
        (GClosureNotify)message_cache_data_free, 0
    );

    message = g_object_new(
        BEDIT_TYPE_FILE_BROWSER_MESSAGE_ID_LOCATION, "object-path",
        MESSAGE_OBJECT_PATH, "method", "root_changed", NULL
    );

    data->root_changed_id = g_signal_connect_data(
        store, "notify::virtual-root",
        G_CALLBACK(store_virtual_root_changed),
        message_cache_data_new(window, message),
        (GClosureNotify)message_cache_data_free, 0
    );

    message = g_object_new(
        BEDIT_TYPE_FILE_BROWSER_MESSAGE_ID_LOCATION,
        "object-path", MESSAGE_OBJECT_PATH, "method", "begin_loading", NULL
    );

    data->begin_loading_id = g_signal_connect_data(
        store, "begin_loading",
        G_CALLBACK(store_begin_loading),
        message_cache_data_new(window, message),
        (GClosureNotify)message_cache_data_free, 0
    );

    message = g_object_new(
        BEDIT_TYPE_FILE_BROWSER_MESSAGE_ID_LOCATION,
        "object-path", MESSAGE_OBJECT_PATH, "method", "end_loading", NULL
    );

    data->end_loading_id = g_signal_connect_data(
        store, "end_loading",
        G_CALLBACK(store_end_loading),
        message_cache_data_new(window, message),
        (GClosureNotify)message_cache_data_free, 0
    );
}

static void message_unregistered(
    BeditMessageBus *bus, const gchar *object_path, const gchar *method,
    BeditWindow *window
) {
    gchar *identifier;
    FilterData *data;
    WindowData *wdata;

    wdata = get_window_data(window);

    identifier = bedit_message_type_identifier(object_path, method);

    data = g_hash_table_lookup(wdata->filters, identifier);

    if (data) {
        bedit_file_browser_widget_remove_filter(wdata->widget, data->id);
    }

    g_free(identifier);
}

void bedit_file_browser_messages_register(
    BeditWindow *window, BeditFileBrowserWidget *widget
) {
    window_data_new(window, widget);

    register_methods(window, widget);
    register_signals(window, widget);

    g_signal_connect(
        bedit_window_get_message_bus(window), "unregistered",
        G_CALLBACK(message_unregistered), window
    );
}

static void cleanup_signals(BeditWindow *window) {
    WindowData *data = get_window_data(window);
    BeditFileBrowserStore *store;

    store = bedit_file_browser_widget_get_browser_store(data->widget);

    g_signal_handler_disconnect(store, data->row_inserted_id);
    g_signal_handler_disconnect(store, data->before_row_deleted_id);
    g_signal_handler_disconnect(store, data->root_changed_id);
    g_signal_handler_disconnect(store, data->begin_loading_id);
    g_signal_handler_disconnect(store, data->end_loading_id);

    g_signal_handlers_disconnect_by_func(
        data->bus, message_unregistered, window
    );
}

void bedit_file_browser_messages_unregister(BeditWindow *window) {
    BeditMessageBus *bus = bedit_window_get_message_bus(window);
    WindowData *data = get_window_data(window);

    cleanup_signals(window);

    BUS_DISCONNECT(bus, get_root, data);
    BUS_DISCONNECT(bus, set_root, data);
    BUS_DISCONNECT(bus, set_emblem, data);
    BUS_DISCONNECT(bus, set_markup, data);
    BUS_DISCONNECT(bus, add_filter, window);
    BUS_DISCONNECT(bus, remove_filter, data);

    BUS_DISCONNECT(bus, up, data);
    BUS_DISCONNECT(bus, history_back, data);
    BUS_DISCONNECT(bus, history_forward, data);

    BUS_DISCONNECT(bus, refresh, data);

    BUS_DISCONNECT(bus, set_show_hidden, data);
    BUS_DISCONNECT(bus, set_show_binary, data);

    BUS_DISCONNECT(bus, show_bookmarks, data);
    BUS_DISCONNECT(bus, show_files, data);

    BUS_DISCONNECT(bus, get_view, data);

    bedit_message_bus_unregister_all(bus, MESSAGE_OBJECT_PATH);

    window_data_free(window);
}

