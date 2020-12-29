/*
 * bedit-file-browser-widget.c
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * Based on gedit-file-browser-widget.c from Gedit.
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

#include <bedit/bedit-utils.h>
#include <gdk/gdkkeysyms.h>
#include <glib.h>
#include <glib/gi18n-lib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "bedit-file-browser-bookmarks-store.h"
#include "bedit-file-browser-enum-types.h"
#include "bedit-file-browser-error.h"
#include "bedit-file-browser-store.h"
#include "bedit-file-browser-utils.h"
#include "bedit-file-browser-view.h"
#include "bedit-file-browser-location.h"
#include "bedit-file-browser-widget.h"
#include "bedit-file-browser-search-view.h"

#define LOCATION_DATA_KEY "bedit-file-browser-widget-location"

enum {
    BOOKMARKS_ID,
    SEPARATOR_CUSTOM_ID,
    SEPARATOR_ID,
    PATH_ID,
    NUM_DEFAULT_IDS
};

enum {
    COLUMN_ICON,
    COLUMN_ICON_NAME,
    COLUMN_NAME,
    COLUMN_FILE,
    COLUMN_ID,
    N_COLUMNS
};

/* Properties */
enum {
    PROP_0,

    PROP_VIRTUAL_ROOT,
    PROP_SEARCH_ENABLED,
};

/* Signals */
enum {
    FILE_ACTIVATED,
    DIRECTORY_ACTIVATED,
    ERROR,
    CONFIRM_DELETE,
    CONFIRM_NO_TRASH,
    OPEN_IN_TERMINAL,
    SET_ACTIVE_ROOT,
    NUM_SIGNALS
};

static guint signals[NUM_SIGNALS] = {0};

typedef struct _SignalNode {
    GObject *object;
    gulong id;
} SignalNode;

typedef struct {
    gulong id;
    BeditFileBrowserWidgetFilterFunc func;
    gpointer user_data;
    GDestroyNotify destroy_notify;
} FilterFunc;

typedef struct {
    gchar *name;
    gchar *icon_name;
    GdkPixbuf *icon;
} NameIcon;

struct _BeditFileBrowserWidgetPrivate {
    BeditFileBrowserView *tree_view;
    BeditFileBrowserSearchView *search_view;

    BeditFileBrowserStore *file_store;
    BeditFileBrowserBookmarksStore *bookmarks_store;

    GMenuModel *dir_menu;
    GMenuModel *bookmarks_menu;

    BeditFileBrowserLocation *location;
    GtkEntry *search_entry;

    GtkStack *toolbar_stack;
    GtkStack *view_stack;

    GSimpleActionGroup *action_group;

    GSList *signal_pool;

    GCancellable *cancellable;

    GdkCursor *busy_cursor;

    guint search_enabled : 1;
};

static void on_model_set(
    GtkTreeView *tree_view, GParamSpec *arg1, BeditFileBrowserWidget *widget
);
static void on_tree_view_error(
    BeditFileBrowserView *tree_view, guint code, gchar *message,
    BeditFileBrowserWidget *obj
);
static void on_file_store_error(
    BeditFileBrowserStore *store, guint code, gchar *message,
    BeditFileBrowserWidget *obj
);
static gboolean on_file_store_no_trash(
    BeditFileBrowserStore *store, GList *files, BeditFileBrowserWidget *obj
);
static gboolean on_tree_view_popup_menu(
    BeditFileBrowserView *tree_view, BeditFileBrowserWidget *obj
);

static gboolean on_view_stack_key_press_event(
    GtkStack *view_stack, GdkEventKey *event,
    BeditFileBrowserWidget *obj
);

static gboolean on_search_entry_key_press_event(
    GtkEntry *search_entry, GdkEventKey *event,
    BeditFileBrowserWidget *obj
);

static gboolean on_tree_view_button_press_event(
    BeditFileBrowserView *tree_view, GdkEventButton *event,
    BeditFileBrowserWidget *obj
);
static gboolean on_tree_view_key_press_event(
    BeditFileBrowserView *tree_view, GdkEventKey *event,
    BeditFileBrowserWidget *obj
);
static void on_selection_changed(
    GtkTreeSelection *selection, BeditFileBrowserWidget *obj
);

static void on_virtual_root_changed(
    BeditFileBrowserStore *model, GParamSpec *param,
    BeditFileBrowserWidget *obj
);
static void on_filter_mode_changed(
    BeditFileBrowserStore *model, GParamSpec *param,
    BeditFileBrowserWidget *obj
);
static void up_activated(
    GSimpleAction *action, GVariant *parameter, gpointer user_data
);
static void home_activated(
    GSimpleAction *action, GVariant *parameter, gpointer user_data
);
static void new_folder_activated(
    GSimpleAction *action, GVariant *parameter, gpointer user_data
);
static void open_activated(
    GSimpleAction *action, GVariant *parameter, gpointer user_data
);
static void new_file_activated(
    GSimpleAction *action, GVariant *parameter, gpointer user_data
);
static void rename_activated(
    GSimpleAction *action, GVariant *parameter, gpointer user_data
);
static void delete_activated(
    GSimpleAction *action, GVariant *parameter, gpointer user_data
);
static void move_to_trash_activated(
    GSimpleAction *action, GVariant *parameter, gpointer user_data
);
static void refresh_view_activated(
    GSimpleAction *action, GVariant *parameter, gpointer user_data
);
static void view_folder_activated(
    GSimpleAction *action, GVariant *parameter, gpointer user_data
);
static void change_show_hidden_state(
    GSimpleAction *action, GVariant *state, gpointer user_data
);
static void change_show_binary_state(
    GSimpleAction *action, GVariant *state, gpointer user_data
);
static void open_in_terminal_activated(
    GSimpleAction *action, GVariant *parameter, gpointer user_data
);
static void set_active_root_activated(
    GSimpleAction *action, GVariant *parameter, gpointer user_data
);

G_DEFINE_DYNAMIC_TYPE_EXTENDED(
    BeditFileBrowserWidget, bedit_file_browser_widget, GTK_TYPE_GRID, 0,
    G_ADD_PRIVATE_DYNAMIC(BeditFileBrowserWidget)
)

static void cancel_async_operation(BeditFileBrowserWidget *widget) {
    if (!widget->priv->cancellable) {
        return;
    }

    g_cancellable_cancel(widget->priv->cancellable);
    g_object_unref(widget->priv->cancellable);

    widget->priv->cancellable = NULL;
}

static void clear_signals(BeditFileBrowserWidget *obj) {
    GSList *item = obj->priv->signal_pool;
    SignalNode *node;

    while (item != NULL) {
        node = (SignalNode *)(item->data);
        item = g_slist_delete_link(item, item);

        g_signal_handler_disconnect(node->object, node->id);
        g_slice_free(SignalNode, node);
    }

    obj->priv->signal_pool = NULL;
}

static void bedit_file_browser_widget_dispose(GObject *object) {
    BeditFileBrowserWidget *obj = BEDIT_FILE_BROWSER_WIDGET(object);
    BeditFileBrowserWidgetPrivate *priv = obj->priv;

    clear_signals(obj);
    g_clear_object(&priv->file_store);
    g_clear_object(&priv->bookmarks_store);

    cancel_async_operation(obj);

    g_clear_object(&priv->busy_cursor);
    g_clear_object(&priv->dir_menu);
    g_clear_object(&priv->bookmarks_menu);

    G_OBJECT_CLASS(bedit_file_browser_widget_parent_class)->dispose(object);
}

static void bedit_file_browser_widget_get_property(
    GObject *object, guint prop_id, GValue *value, GParamSpec *pspec
) {
    BeditFileBrowserWidget *obj = BEDIT_FILE_BROWSER_WIDGET(object);

    switch (prop_id) {
    case PROP_VIRTUAL_ROOT:
        g_value_take_object(
            value, bedit_file_browser_widget_get_virtual_root(obj)
        );
        break;

    case PROP_SEARCH_ENABLED:
        g_value_set_boolean(
            value, bedit_file_browser_widget_get_search_enabled(obj)
        );
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void bedit_file_browser_widget_set_property(
    GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec
) {
    BeditFileBrowserWidget *obj = BEDIT_FILE_BROWSER_WIDGET(object);

    switch (prop_id) {
    case PROP_VIRTUAL_ROOT:
        bedit_file_browser_widget_set_virtual_root(
            obj, G_FILE(g_value_dup_object(value))
        );
        break;

    case PROP_SEARCH_ENABLED:
        bedit_file_browser_widget_set_search_enabled(
            obj, g_value_get_boolean(value)
        );
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void bedit_file_browser_widget_class_init(
    BeditFileBrowserWidgetClass *klass
) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    object_class->dispose = bedit_file_browser_widget_dispose;

    object_class->get_property = bedit_file_browser_widget_get_property;
    object_class->set_property = bedit_file_browser_widget_set_property;

    g_object_class_install_property(
        object_class, PROP_VIRTUAL_ROOT,
        g_param_spec_object(
            "virtual-root", "Virtual Root",
            "The location in the filesystem that widget is currently showing",
            G_TYPE_FILE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS
        )
    );

    g_object_class_install_property(
        object_class, PROP_SEARCH_ENABLED,
        g_param_spec_boolean(
            "search-enabled", "Search Enabled",
            "True if the widget is in search mode.  False otherwise",
            FALSE,
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
            G_PARAM_EXPLICIT_NOTIFY
        )
    );

    signals[FILE_ACTIVATED] = g_signal_new(
        "file-activated", G_OBJECT_CLASS_TYPE(object_class),
        G_SIGNAL_RUN_LAST,
        G_STRUCT_OFFSET(BeditFileBrowserWidgetClass, file_activated),
        NULL, NULL, NULL,
        G_TYPE_NONE, 1,
        G_TYPE_FILE
    );
    signals[DIRECTORY_ACTIVATED] = g_signal_new(
        "directory-activated", G_OBJECT_CLASS_TYPE(object_class),
        G_SIGNAL_RUN_LAST,
        G_STRUCT_OFFSET(BeditFileBrowserWidgetClass, directory_activated),
        NULL, NULL, NULL,
        G_TYPE_NONE, 1,
        G_TYPE_FILE
    );

    signals[ERROR] = g_signal_new(
        "error", G_OBJECT_CLASS_TYPE(object_class), G_SIGNAL_RUN_LAST,
        G_STRUCT_OFFSET(BeditFileBrowserWidgetClass, error),
        NULL, NULL, NULL,
        G_TYPE_NONE, 2,
        G_TYPE_UINT, G_TYPE_STRING
    );

    signals[CONFIRM_DELETE] = g_signal_new(
        "confirm-delete", G_OBJECT_CLASS_TYPE(object_class), G_SIGNAL_RUN_LAST,
        G_STRUCT_OFFSET(BeditFileBrowserWidgetClass, confirm_delete),
        g_signal_accumulator_true_handled, NULL, NULL,
        G_TYPE_BOOLEAN, 2,
        G_TYPE_OBJECT, G_TYPE_POINTER
    );

    signals[CONFIRM_NO_TRASH] = g_signal_new(
        "confirm-no-trash", G_OBJECT_CLASS_TYPE(object_class),
        G_SIGNAL_RUN_LAST,
        G_STRUCT_OFFSET(BeditFileBrowserWidgetClass, confirm_no_trash),
        g_signal_accumulator_true_handled, NULL, NULL,
        G_TYPE_BOOLEAN, 1,
        G_TYPE_POINTER
    );

    signals[OPEN_IN_TERMINAL] = g_signal_new(
        "open-in-terminal", G_OBJECT_CLASS_TYPE(object_class),
        G_SIGNAL_RUN_LAST,
        G_STRUCT_OFFSET(BeditFileBrowserWidgetClass, open_in_terminal),
        NULL, NULL, NULL,
        G_TYPE_NONE, 1,
        G_TYPE_FILE
    );

    signals[SET_ACTIVE_ROOT] = g_signal_new(
        "set-active-root", G_OBJECT_CLASS_TYPE(object_class), G_SIGNAL_RUN_LAST,
        G_STRUCT_OFFSET(BeditFileBrowserWidgetClass, set_active_root),
        NULL, NULL, NULL,
        G_TYPE_NONE, 0
    );

    gtk_widget_class_set_template_from_resource(
        widget_class,
        "/com/bwhmather/bedit/plugins/file-browser/ui/"
        "bedit-file-browser-widget.ui"
    );

    gtk_widget_class_bind_template_child_private(
        widget_class, BeditFileBrowserWidget, location
    );

    gtk_widget_class_bind_template_child_private(
        widget_class, BeditFileBrowserWidget, search_entry
    );

    gtk_widget_class_bind_template_child_private(
        widget_class, BeditFileBrowserWidget, tree_view
    );

    gtk_widget_class_bind_template_child_private(
        widget_class, BeditFileBrowserWidget, search_view
    );

    gtk_widget_class_bind_template_child_private(
        widget_class, BeditFileBrowserWidget, toolbar_stack
    );

    gtk_widget_class_bind_template_child_private(
        widget_class, BeditFileBrowserWidget, view_stack
    );
}

static void bedit_file_browser_widget_class_finalize(
    BeditFileBrowserWidgetClass *klass
) {}

static void add_signal(
    BeditFileBrowserWidget *obj, gpointer object, gulong id
) {
    SignalNode *node = g_slice_new(SignalNode);

    node->object = G_OBJECT(object);
    node->id = id;

    obj->priv->signal_pool = g_slist_prepend(obj->priv->signal_pool, node);
}

static void on_begin_loading(
    BeditFileBrowserStore *model, GtkTreeIter *iter,
    BeditFileBrowserWidget *obj
) {
    if (!GDK_IS_WINDOW(gtk_widget_get_window(
        GTK_WIDGET(obj->priv->tree_view)
    ))) {
        return;
    }

    gdk_window_set_cursor(
        gtk_widget_get_window(GTK_WIDGET(obj)), obj->priv->busy_cursor
    );
}

static void on_end_loading(
    BeditFileBrowserStore *model, GtkTreeIter *iter,
    BeditFileBrowserWidget *obj
) {
    if (!GDK_IS_WINDOW(gtk_widget_get_window(
        GTK_WIDGET(obj->priv->tree_view)
    ))) {
        return;
    }

    gdk_window_set_cursor(gtk_widget_get_window(GTK_WIDGET(obj)), NULL);
}

static GActionEntry browser_entries[] = {
    {"open", open_activated},
    {"set_active_root", set_active_root_activated},
    {"new_folder", new_folder_activated},
    {"new_file", new_file_activated},
    {"rename", rename_activated},
    {"move_to_trash", move_to_trash_activated},
    {"delete", delete_activated},
    {"refresh_view", refresh_view_activated},
    {"view_folder", view_folder_activated},
    {"open_in_terminal", open_in_terminal_activated},
    {"show_hidden", NULL, NULL, "false", change_show_hidden_state},
    {"show_binary", NULL, NULL, "false", change_show_binary_state},
    {"up", up_activated},
    {"home", home_activated}
};

static void bedit_file_browser_widget_init(BeditFileBrowserWidget *obj) {
    GtkBuilder *builder;
    GdkDisplay *display;
    GError *error = NULL;

    obj->priv = bedit_file_browser_widget_get_instance_private(obj);

    display = gtk_widget_get_display(GTK_WIDGET(obj));
    obj->priv->busy_cursor = gdk_cursor_new_from_name(display, "progress");
    obj->priv->search_enabled = FALSE;

    builder = gtk_builder_new();
    if (!gtk_builder_add_from_resource(
        builder,
        "/com/bwhmather/bedit/plugins/file-browser/ui/"
        "bedit-file-browser-menus.ui",
        &error
    )) {
        g_warning("loading menu builder file: %s", error->message);
        g_error_free(error);
    } else {
        obj->priv->dir_menu = G_MENU_MODEL(
            g_object_ref(gtk_builder_get_object(builder, "dir-menu"))
        );
        obj->priv->bookmarks_menu = G_MENU_MODEL(
            g_object_ref(gtk_builder_get_object(builder, "bookmarks-menu"))
        );
    }

    g_object_unref(builder);

    obj->priv->action_group = g_simple_action_group_new();
    g_action_map_add_action_entries(
        G_ACTION_MAP(obj->priv->action_group), browser_entries,
        G_N_ELEMENTS(browser_entries), obj
    );

    gtk_widget_insert_action_group(
        GTK_WIDGET(obj), "browser", G_ACTION_GROUP(obj->priv->action_group)
    );

    gtk_widget_init_template(GTK_WIDGET(obj));

    obj->priv->file_store = bedit_file_browser_store_new(NULL);
    obj->priv->bookmarks_store = bedit_file_browser_bookmarks_store_new();

    /* New Locations popover */
    bedit_file_browser_location_set_bookmarks_store(
        obj->priv->location, obj->priv->bookmarks_store
    );
    bedit_file_browser_location_set_file_store(
        obj->priv->location, obj->priv->file_store
    );

    g_signal_connect(
        obj->priv->search_entry, "key-press-event",
        G_CALLBACK(on_search_entry_key_press_event), obj
    );

    g_signal_connect(
        obj->priv->view_stack, "key-press-event",
        G_CALLBACK(on_view_stack_key_press_event), obj
    );

    /* tree view */
    bedit_file_browser_view_set_restore_expand_state(obj->priv->tree_view, TRUE);

    bedit_file_browser_store_set_filter_mode(
        obj->priv->file_store,
        BEDIT_FILE_BROWSER_STORE_FILTER_MODE_HIDE_HIDDEN |
        BEDIT_FILE_BROWSER_STORE_FILTER_MODE_HIDE_BINARY
    );

    g_signal_connect(
        obj->priv->tree_view, "notify::model",
        G_CALLBACK(on_model_set), obj
    );
    g_signal_connect(
        obj->priv->tree_view, "error",
        G_CALLBACK(on_tree_view_error), obj
    );
    g_signal_connect(
        obj->priv->tree_view, "popup-menu",
        G_CALLBACK(on_tree_view_popup_menu), obj
    );
    g_signal_connect(
        obj->priv->tree_view, "button-press-event",
        G_CALLBACK(on_tree_view_button_press_event), obj
    );
    g_signal_connect(
        obj->priv->tree_view, "key-press-event",
        G_CALLBACK(on_tree_view_key_press_event), obj
    );

    g_signal_connect(
        gtk_tree_view_get_selection(GTK_TREE_VIEW(obj->priv->tree_view)),
        "changed",
        G_CALLBACK(on_selection_changed), obj
    );
    g_signal_connect(
        obj->priv->file_store, "notify::filter-mode",
        G_CALLBACK(on_filter_mode_changed), obj
    );

    g_signal_connect(
        obj->priv->file_store, "notify::virtual-root",
        G_CALLBACK(on_virtual_root_changed), obj
    );

    g_signal_connect(
        obj->priv->file_store, "begin-loading",
        G_CALLBACK(on_begin_loading), obj
    );

    g_signal_connect(
        obj->priv->file_store, "end-loading",
        G_CALLBACK(on_end_loading), obj
    );

    g_signal_connect(
        obj->priv->file_store, "error",
        G_CALLBACK(on_file_store_error), obj
    );

    g_object_bind_property(
        obj->priv->search_entry, "text",
        obj->priv->search_view, "query",
        G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE
    );
}

/* Private */

static void update_sensitivity(BeditFileBrowserWidget *obj) {
    GtkTreeModel *model = gtk_tree_view_get_model(
        GTK_TREE_VIEW(obj->priv->tree_view)
    );
    GAction *action;
    gint mode;

    if (BEDIT_IS_FILE_BROWSER_STORE(model)) {
        mode = bedit_file_browser_store_get_filter_mode(
            BEDIT_FILE_BROWSER_STORE(model)
        );

        action = g_action_map_lookup_action(
            G_ACTION_MAP(obj->priv->action_group), "show_hidden"
        );

        g_action_change_state(
            action,
            g_variant_new_boolean(
                !(mode & BEDIT_FILE_BROWSER_STORE_FILTER_MODE_HIDE_HIDDEN)
            )
        );

        /* sensitivity */
        action = g_action_map_lookup_action(
            G_ACTION_MAP(obj->priv->action_group), "up"
        );
        g_simple_action_set_enabled(G_SIMPLE_ACTION(action), TRUE);
        action = g_action_map_lookup_action(
            G_ACTION_MAP(obj->priv->action_group), "home"
        );
        g_simple_action_set_enabled(G_SIMPLE_ACTION(action), TRUE);

        action = g_action_map_lookup_action(
            G_ACTION_MAP(obj->priv->action_group), "show_hidden"
        );
        g_simple_action_set_enabled(G_SIMPLE_ACTION(action), TRUE);

        action = g_action_map_lookup_action(
            G_ACTION_MAP(obj->priv->action_group), "show_binary"
        );
        g_simple_action_set_enabled(G_SIMPLE_ACTION(action), TRUE);

        g_simple_action_set_enabled(G_SIMPLE_ACTION(action), TRUE);
    } else if (BEDIT_IS_FILE_BROWSER_BOOKMARKS_STORE(model)) {
        /* Set the filter toggle to normal up state, just for visual pleasure */
        action = g_action_map_lookup_action(
            G_ACTION_MAP(obj->priv->action_group), "show_hidden"
        );

        g_action_change_state(action, g_variant_new_boolean(FALSE));

        /* sensitivity */
        action = g_action_map_lookup_action(
            G_ACTION_MAP(obj->priv->action_group), "up"
        );
        g_simple_action_set_enabled(G_SIMPLE_ACTION(action), FALSE);
        action = g_action_map_lookup_action(
            G_ACTION_MAP(obj->priv->action_group), "home"
        );
        g_simple_action_set_enabled(G_SIMPLE_ACTION(action), FALSE);

        action = g_action_map_lookup_action(
            G_ACTION_MAP(obj->priv->action_group), "show_hidden"
        );
        g_simple_action_set_enabled(G_SIMPLE_ACTION(action), FALSE);

        action = g_action_map_lookup_action(
            G_ACTION_MAP(obj->priv->action_group), "show_binary"
        );
        g_simple_action_set_enabled(G_SIMPLE_ACTION(action), FALSE);

        g_simple_action_set_enabled(G_SIMPLE_ACTION(action), FALSE);
    }

    on_selection_changed(
        gtk_tree_view_get_selection GTK_TREE_VIEW(obj->priv->tree_view), obj
    );
}

static gboolean bedit_file_browser_widget_get_first_selected(
    BeditFileBrowserWidget *obj, GtkTreeIter *iter
) {
    GtkTreeSelection *selection = gtk_tree_view_get_selection(
        GTK_TREE_VIEW(obj->priv->tree_view)
    );
    GtkTreeModel *model;
    GList *rows = gtk_tree_selection_get_selected_rows(selection, &model);
    gboolean result;

    if (!rows) {
        return FALSE;
    }

    result = gtk_tree_model_get_iter(model, iter, (GtkTreePath *)(rows->data));

    g_list_free_full(rows, (GDestroyNotify)gtk_tree_path_free);

    return result;
}

static gboolean popup_menu(
    BeditFileBrowserWidget *obj, GtkTreeView *tree_view, GdkEventButton *event,
    GtkTreeModel *model
) {
    GtkWidget *menu;
    GMenuModel *menu_model;

    if (BEDIT_IS_FILE_BROWSER_STORE(model)) {
        menu_model = obj->priv->dir_menu;
    } else if (BEDIT_IS_FILE_BROWSER_BOOKMARKS_STORE(model)) {
        menu_model = obj->priv->bookmarks_menu;
    } else {
        return FALSE;
    }

    menu = gtk_menu_new_from_model(menu_model);
    gtk_menu_attach_to_widget(GTK_MENU(menu), GTK_WIDGET(obj), NULL);

    if (event != NULL) {
        GtkTreeSelection *selection;
        selection = gtk_tree_view_get_selection(tree_view);

        if (gtk_tree_selection_count_selected_rows(selection) <= 1) {
            GtkTreePath *path;

            if (gtk_tree_view_get_path_at_pos(
                tree_view, (gint)event->x, (gint)event->y, &path,
                NULL, NULL, NULL
            )) {
                gtk_tree_selection_unselect_all(selection);
                gtk_tree_selection_select_path(selection, path);
                gtk_tree_path_free(path);
            }
        }

        gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent *)event);
    } else {
        GdkWindow *window = gtk_widget_get_window(GTK_WIDGET(tree_view));
        GdkGravity rect_gravity = GDK_GRAVITY_EAST;
        GdkGravity menu_gravity = GDK_GRAVITY_NORTH_WEST;
        GdkRectangle rect;

        if (bedit_utils_menu_position_under_tree_view(tree_view, &rect)) {
            if (
                gtk_widget_get_direction(GTK_WIDGET(tree_view)) ==
                GTK_TEXT_DIR_RTL
            ) {
                rect_gravity = GDK_GRAVITY_WEST;
                menu_gravity = GDK_GRAVITY_NORTH_EAST;
            }

            gtk_menu_popup_at_rect(
                GTK_MENU(menu), window, &rect, rect_gravity, menu_gravity,
                NULL
            );
            gtk_menu_shell_select_first(GTK_MENU_SHELL(menu), FALSE);
        }
    }

    return TRUE;
}

static void rename_selected_file(BeditFileBrowserWidget *obj) {
    GtkTreeModel *model = gtk_tree_view_get_model(
        GTK_TREE_VIEW(obj->priv->tree_view)
    );
    GtkTreeIter iter;

    if (!BEDIT_IS_FILE_BROWSER_STORE(model)) {
        return;
    }

    if (bedit_file_browser_widget_get_first_selected(obj, &iter)) {
        bedit_file_browser_view_start_rename(obj->priv->tree_view, &iter);
    }
}

static GList *get_deletable_files(BeditFileBrowserWidget *obj) {
    GtkTreeModel *model = gtk_tree_view_get_model(
        GTK_TREE_VIEW(obj->priv->tree_view)
    );
    GtkTreeSelection *selection = gtk_tree_view_get_selection(
        GTK_TREE_VIEW(obj->priv->tree_view)
    );
    GList *rows = gtk_tree_selection_get_selected_rows(selection, &model);
    GList *row;
    GList *paths = NULL;

    for (row = rows; row; row = row->next) {
        GtkTreePath *path = (GtkTreePath *)(row->data);
        GtkTreeIter iter;
        guint flags;

        if (!gtk_tree_model_get_iter(model, &iter, path)) {
            continue;
        }

        gtk_tree_model_get(
            model, &iter,
            BEDIT_FILE_BROWSER_STORE_COLUMN_FLAGS, &flags,
            -1
        );

        if (FILE_IS_DUMMY(flags)) {
            continue;
        }

        paths = g_list_append(paths, gtk_tree_path_copy(path));
    }

    g_list_free_full(rows, (GDestroyNotify)gtk_tree_path_free);

    return paths;
}

static void delete_selected_files(
    BeditFileBrowserWidget *obj, gboolean trash
) {
    GtkTreeModel *model;
    gboolean confirm;
    GList *rows;

    model = gtk_tree_view_get_model(GTK_TREE_VIEW(obj->priv->tree_view));

    if (!BEDIT_IS_FILE_BROWSER_STORE(model)) {
        return;
    }

    if (!(rows = get_deletable_files(obj))) {
        return;
    }

    if (!trash) {
        g_signal_emit(obj, signals[CONFIRM_DELETE], 0, model, rows, &confirm);

        if (!confirm) {
            return;
        }
    }

    bedit_file_browser_store_delete_all(
        BEDIT_FILE_BROWSER_STORE(model), rows, trash
    );

    g_list_free_full(rows, (GDestroyNotify)gtk_tree_path_free);

    return;
}

static gboolean on_file_store_no_trash(
    BeditFileBrowserStore *store, GList *files, BeditFileBrowserWidget *obj
) {
    gboolean confirm = FALSE;

    g_signal_emit(obj, signals[CONFIRM_NO_TRASH], 0, files, &confirm);

    return confirm;
}

static GFile *get_topmost_file(GFile *file) {
    GFile *current = g_object_ref(file);
    GFile *tmp;

    while ((tmp = g_file_get_parent(current)) != NULL) {
        g_object_unref(current);
        current = tmp;
    }

    return current;
}

static void update_filter_mode(
    BeditFileBrowserWidget *obj, GSimpleAction *action, GVariant *state,
    BeditFileBrowserStoreFilterMode mode
) {
    GtkTreeModel *model = gtk_tree_view_get_model(
        GTK_TREE_VIEW(obj->priv->tree_view)
    );

    if (BEDIT_IS_FILE_BROWSER_STORE(model)) {
        gint now = bedit_file_browser_store_get_filter_mode(
            BEDIT_FILE_BROWSER_STORE(model)
        );

        if (g_variant_get_boolean(state)) {
            now &= ~mode;
        } else {
            now |= mode;
        }

        bedit_file_browser_store_set_filter_mode(
            BEDIT_FILE_BROWSER_STORE(model), now
        );
    }

    g_simple_action_set_state(action, state);
}

/* Public */

GtkWidget *bedit_file_browser_widget_new(void) {
    BeditFileBrowserWidget *obj = g_object_new(
        BEDIT_TYPE_FILE_BROWSER_WIDGET, NULL
    );

    return GTK_WIDGET(obj);
}

void bedit_file_browser_widget_set_root_and_virtual_root(
    BeditFileBrowserWidget *obj, GFile *root, GFile *virtual_root
) {
    if (!virtual_root) {
        bedit_file_browser_store_set_root_and_virtual_root(
            obj->priv->file_store, root, root
        );
    } else {
        bedit_file_browser_store_set_root_and_virtual_root(
            obj->priv->file_store, root, virtual_root
        );
    }
}

void bedit_file_browser_widget_set_root(
    BeditFileBrowserWidget *obj, GFile *root
) {
    g_return_if_fail(BEDIT_IS_FILE_BROWSER_WIDGET(obj));

    bedit_file_browser_store_set_root_and_virtual_root(
        obj->priv->file_store, root, root
    );
}

void bedit_file_browser_widget_set_virtual_root(
    BeditFileBrowserWidget *obj, GFile *virtual_root
) {
    GFile *root;

    g_return_if_fail(BEDIT_IS_FILE_BROWSER_WIDGET(obj));
    g_return_if_fail(G_IS_FILE(virtual_root));

    root = get_topmost_file(virtual_root);
    bedit_file_browser_store_set_root_and_virtual_root(
        obj->priv->file_store, root, virtual_root
    );

    bedit_file_browser_search_view_set_virtual_root(
        obj->priv->search_view, virtual_root
    );
}

GFile *bedit_file_browser_widget_get_virtual_root(BeditFileBrowserWidget *obj) {
    g_return_val_if_fail(BEDIT_IS_FILE_BROWSER_WIDGET(obj), NULL);

    return bedit_file_browser_store_get_virtual_root(obj->priv->file_store);
}

BeditFileBrowserStore *bedit_file_browser_widget_get_browser_store(
    BeditFileBrowserWidget *obj
) {
    return obj->priv->file_store;
}

BeditFileBrowserBookmarksStore *bedit_file_browser_widget_get_bookmarks_store(
    BeditFileBrowserWidget *obj
) {
    return obj->priv->bookmarks_store;
}

BeditFileBrowserView *bedit_file_browser_widget_get_browser_view(
    BeditFileBrowserWidget *obj
) {
    return obj->priv->tree_view;
}

gboolean bedit_file_browser_widget_get_selected_directory(
    BeditFileBrowserWidget *obj, GtkTreeIter *iter
) {
    GtkTreeModel *model = gtk_tree_view_get_model(
        GTK_TREE_VIEW(obj->priv->tree_view)
    );
    GtkTreeIter parent;
    guint flags;

    if (!BEDIT_IS_FILE_BROWSER_STORE(model)) {
        return FALSE;
    }

    if (
        !bedit_file_browser_widget_get_first_selected(obj, iter) &&
        !bedit_file_browser_store_get_iter_virtual_root(
            BEDIT_FILE_BROWSER_STORE(model), iter
        )
    ) {
        return FALSE;
    }

    gtk_tree_model_get(
        model, iter,
        BEDIT_FILE_BROWSER_STORE_COLUMN_FLAGS, &flags,
        -1
    );

    if (!FILE_IS_DIR(flags)) {
        /* Get the parent, because the selection is a file */
        gtk_tree_model_iter_parent(model, &parent, iter);
        *iter = parent;
    }

    return TRUE;
}

void bedit_file_browser_widget_set_active_root_enabled(
    BeditFileBrowserWidget *widget, gboolean enabled
) {
    GAction *action;

    g_return_if_fail(BEDIT_IS_FILE_BROWSER_WIDGET(widget));

    action = g_action_map_lookup_action(
        G_ACTION_MAP(widget->priv->action_group), "set_active_root"
    );
    g_simple_action_set_enabled(G_SIMPLE_ACTION(action), enabled);
}


void bedit_file_browser_widget_set_search_enabled(
    BeditFileBrowserWidget *obj, gboolean enabled
) {
    g_return_if_fail(BEDIT_IS_FILE_BROWSER_WIDGET(obj));

    if (obj->priv->search_enabled == enabled) {
        return;
    }

    if (enabled) {
        gtk_stack_set_visible_child_full(
            obj->priv->toolbar_stack, "search",
            GTK_STACK_TRANSITION_TYPE_OVER_RIGHT
        );
        gtk_stack_set_visible_child_full(
            obj->priv->view_stack, "search",
            GTK_STACK_TRANSITION_TYPE_NONE
        );

        bedit_file_browser_search_view_set_enabled(
            obj->priv->search_view, TRUE
        );

    } else {
        gtk_stack_set_visible_child_full(
            obj->priv->toolbar_stack, "tree",
            GTK_STACK_TRANSITION_TYPE_OVER_RIGHT
        );
        gtk_stack_set_visible_child_full(
            obj->priv->view_stack, "tree",
            GTK_STACK_TRANSITION_TYPE_NONE
        );

        bedit_file_browser_search_view_set_enabled(
            obj->priv->search_view, FALSE
        );

        gtk_entry_reset_im_context(obj->priv->search_entry);
        gtk_entry_set_text(obj->priv->search_entry, "");
    }

    obj->priv->search_enabled = enabled;

    g_object_notify(G_OBJECT(obj), "search-enabled");
}

gboolean bedit_file_browser_widget_get_search_enabled(
    BeditFileBrowserWidget *obj
) {
    g_return_val_if_fail(BEDIT_IS_FILE_BROWSER_WIDGET(obj), FALSE);

    return obj->priv->search_enabled ? TRUE : FALSE;
}

static guint bedit_file_browser_widget_get_num_selected_files_or_directories(
    BeditFileBrowserWidget *obj, guint *files, guint *dirs
) {
    GtkTreeSelection *selection = gtk_tree_view_get_selection(
        GTK_TREE_VIEW(obj->priv->tree_view)
    );
    GtkTreeModel *model = gtk_tree_view_get_model(
        GTK_TREE_VIEW(obj->priv->tree_view)
    );
    GList *rows, *row;
    guint result = 0;

    rows = gtk_tree_selection_get_selected_rows(selection, &model);

    for (row = rows; row; row = row->next) {
        GtkTreePath *path = (GtkTreePath *)(row->data);
        BeditFileBrowserStoreFlag flags;
        GtkTreeIter iter;

        /* Get iter from path */
        if (!gtk_tree_model_get_iter(model, &iter, path)) {
            continue;
        }

        gtk_tree_model_get(
            model, &iter,
            BEDIT_FILE_BROWSER_STORE_COLUMN_FLAGS, &flags,
            -1
        );

        if (!FILE_IS_DUMMY(flags)) {
            if (!FILE_IS_DIR(flags)) {
                ++(*files);
            } else {
                ++(*dirs);
            }

            ++result;
        }
    }

    g_list_free_full(rows, (GDestroyNotify)gtk_tree_path_free);

    return result;
}

typedef struct {
    BeditFileBrowserWidget *widget;
    GCancellable *cancellable;
} AsyncData;

void bedit_file_browser_widget_refresh(BeditFileBrowserWidget *obj) {
    GtkTreeModel *model = gtk_tree_view_get_model(
        GTK_TREE_VIEW(obj->priv->tree_view)
    );

    bedit_file_browser_store_refresh(BEDIT_FILE_BROWSER_STORE(model));
}

BeditMenuExtension *bedit_file_browser_widget_extend_context_menu(
    BeditFileBrowserWidget *obj
) {
    guint i, n_items;
    GMenuModel *section = NULL;

    g_return_val_if_fail(BEDIT_IS_FILE_BROWSER_WIDGET(obj), NULL);

    n_items = g_menu_model_get_n_items(obj->priv->dir_menu);

    for (i = 0; i < n_items && !section; i++) {
        gchar *id = NULL;

        if (g_menu_model_get_item_attribute(
            obj->priv->dir_menu, i, "id", "s", &id
        ) && strcmp(id, "extension-section") == 0) {
            section = g_menu_model_get_item_link(
                obj->priv->dir_menu, i, G_MENU_LINK_SECTION
            );
        }

        g_free(id);
    }

    return section != NULL ? bedit_menu_extension_new(G_MENU(section)) : NULL;
}

static void file_open(
    BeditFileBrowserWidget *obj, GtkTreeModel *model, GtkTreeIter *iter
) {
    GFile *location;
    gint flags;

    gtk_tree_model_get(
        model, iter,
        BEDIT_FILE_BROWSER_STORE_COLUMN_FLAGS, &flags,
        BEDIT_FILE_BROWSER_STORE_COLUMN_LOCATION, &location,
        -1
    );

    if (!FILE_IS_DIR(flags) && !FILE_IS_DUMMY(flags)) {
        g_signal_emit(obj, signals[FILE_ACTIVATED], 0, location);
    }

    if (location) {
        g_object_unref(location);
    }
}

static gboolean directory_open(
    BeditFileBrowserWidget *obj, GtkTreeModel *model, GtkTreeIter *iter
) {
    gboolean result = FALSE;
    GError *error = NULL;
    GFile *location;
    BeditFileBrowserStoreFlag flags;

    gtk_tree_model_get(
        model, iter,
        BEDIT_FILE_BROWSER_STORE_COLUMN_FLAGS, &flags,
        BEDIT_FILE_BROWSER_STORE_COLUMN_LOCATION, &location,
        -1
    );

    if (FILE_IS_DIR(flags) && location) {
        gchar *uri = g_file_get_uri(location);
        result = TRUE;

        if (!gtk_show_uri_on_window(
            GTK_WINDOW(obj), uri, GDK_CURRENT_TIME, &error
        )) {
            g_signal_emit(
                obj, signals[ERROR], 0,
                BEDIT_FILE_BROWSER_ERROR_OPEN_DIRECTORY,
                error->message
            );

            g_error_free(error);
            error = NULL;
        }

        g_free(uri);
        g_object_unref(location);
    }

    return result;
}

static void on_tree_view_file_activated(
    BeditFileBrowserView *tree_view, GtkTreeIter *iter,
    BeditFileBrowserWidget *obj
) {
    GtkTreeModel *model;
    BeditFileBrowserStoreFlag flags;
    GFile *location;

    g_return_if_fail(BEDIT_IS_FILE_BROWSER_VIEW(tree_view));
    g_return_if_fail(BEDIT_IS_FILE_BROWSER_WIDGET(obj));

    model = gtk_tree_view_get_model(GTK_TREE_VIEW(tree_view));

    gtk_tree_model_get(
        model, iter,
        BEDIT_FILE_BROWSER_STORE_COLUMN_FLAGS, &flags,
        BEDIT_FILE_BROWSER_STORE_COLUMN_LOCATION, &location,
        -1
    );

    g_return_if_fail(!FILE_IS_DIR(flags));
    g_return_if_fail(!FILE_IS_DUMMY(flags));
    g_return_if_fail(G_IS_FILE(location));

    g_signal_emit(obj, signals[FILE_ACTIVATED], 0, location);

    g_object_unref(location);
}

static void on_tree_view_directory_activated(
    BeditFileBrowserView *tree_view, GtkTreeIter *iter,
    BeditFileBrowserWidget *obj
) {
    GtkTreeModel *model;
    BeditFileBrowserStoreFlag flags;
    GFile *location;

    model = gtk_tree_view_get_model(GTK_TREE_VIEW(tree_view));

    gtk_tree_model_get(
        model, iter,
        BEDIT_FILE_BROWSER_STORE_COLUMN_FLAGS, &flags,
        BEDIT_FILE_BROWSER_STORE_COLUMN_LOCATION, &location,
        -1
    );

    g_return_if_fail(FILE_IS_DIR(flags));
    g_return_if_fail(G_IS_FILE(location));

    g_signal_emit(obj, signals[DIRECTORY_ACTIVATED], 0, location);

    g_object_unref(location);
}

static gboolean virtual_root_is_root(
    BeditFileBrowserWidget *obj, BeditFileBrowserStore *model
) {
    GtkTreeIter root;
    GtkTreeIter virtual_root;

    if (!bedit_file_browser_store_get_iter_root(model, &root)) {
        return TRUE;
    }

    if (!bedit_file_browser_store_get_iter_virtual_root(model, &virtual_root)) {
        return TRUE;
    }

    return bedit_file_browser_store_iter_equal(model, &root, &virtual_root);
}

static void on_virtual_root_changed(
    BeditFileBrowserStore *model, GParamSpec *param,
    BeditFileBrowserWidget *obj
) {
    GtkTreeIter iter;

    if (
        gtk_tree_view_get_model(GTK_TREE_VIEW(obj->priv->tree_view)) !=
        GTK_TREE_MODEL(obj->priv->file_store)
    ) {
        bedit_file_browser_view_set_model(
            obj->priv->tree_view, GTK_TREE_MODEL(obj->priv->file_store)
        );
    }

    if (bedit_file_browser_store_get_iter_virtual_root(model, &iter)) {
        GFile *location;
        GtkTreeIter root;

        gtk_tree_model_get(
            GTK_TREE_MODEL(model), &iter,
            BEDIT_FILE_BROWSER_STORE_COLUMN_LOCATION, &location,
            -1
        );

        if (bedit_file_browser_store_get_iter_root(model, &root)) {
            GAction *action;

            action = g_action_map_lookup_action(
                G_ACTION_MAP(obj->priv->action_group), "up"
            );
            g_simple_action_set_enabled(
                G_SIMPLE_ACTION(action), !virtual_root_is_root(obj, model)
            );
        }

        if (location) {
            g_object_unref(location);
        }
    } else {
        g_message("NO!");
    }

    bedit_file_browser_search_view_set_virtual_root(
        obj->priv->search_view, bedit_file_browser_store_get_virtual_root(model)
    );

    g_object_notify(G_OBJECT(obj), "virtual-root");

}

static void on_model_set(
    GtkTreeView *tree_view, GParamSpec *arg1, BeditFileBrowserWidget *widget
) {
    GtkTreeModel *model;

    g_return_if_fail(GTK_IS_TREE_VIEW(tree_view));
    g_return_if_fail(BEDIT_IS_FILE_BROWSER_WIDGET(widget));

    model = gtk_tree_view_get_model(tree_view);

    clear_signals(widget);

    if (model != NULL) {
        cancel_async_operation(widget);

        add_signal(
            widget, tree_view,
            g_signal_connect(
                tree_view, "file-activated",
                G_CALLBACK(on_tree_view_file_activated), widget
            )
        );
        add_signal(
            widget, tree_view,
            g_signal_connect(
                tree_view, "directory-activated",
                G_CALLBACK(on_tree_view_directory_activated), widget
            )
        );

        add_signal(
            widget, model,
            g_signal_connect(
                model, "no-trash", G_CALLBACK(on_file_store_no_trash), widget
            )
        );
    }

    update_sensitivity(widget);
}

static void on_file_store_error(
    BeditFileBrowserStore *store, guint code, gchar *message,
    BeditFileBrowserWidget *obj
) {
    g_signal_emit(obj, signals[ERROR], 0, code, message);
}

static void on_tree_view_error(
    BeditFileBrowserView *tree_view, guint code, gchar *message,
    BeditFileBrowserWidget *obj
) {
    g_signal_emit(obj, signals[ERROR], 0, code, message);
}

static gboolean on_tree_view_popup_menu(
    BeditFileBrowserView *tree_view, BeditFileBrowserWidget *obj
) {
    return popup_menu(
        obj, GTK_TREE_VIEW(tree_view), NULL,
        gtk_tree_view_get_model(GTK_TREE_VIEW(tree_view))
    );
}

static gboolean on_tree_view_button_press_event(
    BeditFileBrowserView *tree_view, GdkEventButton *event,
    BeditFileBrowserWidget *obj
) {
    if (
        event->type == GDK_BUTTON_PRESS &&
        event->button == GDK_BUTTON_SECONDARY
    ) {
        return popup_menu(
            obj, GTK_TREE_VIEW(tree_view), event,
            gtk_tree_view_get_model(GTK_TREE_VIEW(tree_view))
        );
    }

    return FALSE;
}

static gboolean on_tree_view_key_press_event(
    BeditFileBrowserView *tree_view, GdkEventKey *event,
    BeditFileBrowserWidget *obj
) {
    GtkTreeModel *model;
    guint modifiers;

    if (
        (event->state & (
            ~GDK_CONTROL_MASK & ~GDK_MOD1_MASK
        )) == (event->state | GDK_SHIFT_MASK)  &&
        event->keyval == GDK_KEY_BackSpace
    ) {
        GAction *action = g_action_map_lookup_action(
            G_ACTION_MAP(obj->priv->action_group), "up"
        );

        if (action != NULL) {
            g_action_activate(action, NULL);
            return TRUE;
        }
    }

    model = gtk_tree_view_get_model(GTK_TREE_VIEW(tree_view));
    if (!BEDIT_IS_FILE_BROWSER_STORE(model)) {
        return FALSE;
    }

    modifiers = gtk_accelerator_get_default_mod_mask();

    if (event->keyval == GDK_KEY_Delete || event->keyval == GDK_KEY_KP_Delete) {
        if ((event->state & modifiers) == GDK_SHIFT_MASK) {
            delete_selected_files(obj, FALSE);
            return TRUE;
        } else if ((event->state & modifiers) == 0) {
            delete_selected_files(obj, TRUE);
            return TRUE;
        }
    }

    if ((event->keyval == GDK_KEY_F2) && (event->state & modifiers) == 0) {
        rename_selected_file(obj);
        return TRUE;
    }

    return FALSE;
}

static gboolean on_view_stack_key_press_event(
    GtkStack *view_stack, GdkEventKey *event,
    BeditFileBrowserWidget *obj
) {
    const gchar *name;

    g_return_val_if_fail(GTK_IS_STACK(view_stack), FALSE);
    g_return_val_if_fail(BEDIT_IS_FILE_BROWSER_WIDGET(obj), FALSE);

    g_return_val_if_fail(view_stack == obj->priv->view_stack, FALSE);

    name = gtk_stack_get_visible_child_name(obj->priv->view_stack);
    if (g_strcmp0(name, "search")) {
        if (!gtk_entry_im_context_filter_keypress(
            obj->priv->search_entry, event)
        ) {
            return FALSE;
        }
        bedit_file_browser_widget_set_search_enabled(obj, TRUE);

        return TRUE;
    } else {
        gtk_widget_realize(GTK_WIDGET(obj->priv->search_entry));
        gtk_widget_event(
            GTK_WIDGET(obj->priv->search_entry), (GdkEvent *) event
        );
        return TRUE;
    }
}

static gboolean on_search_entry_key_press_event(
    GtkEntry *search_entry, GdkEventKey *event,
    BeditFileBrowserWidget *obj
) {
    g_return_val_if_fail(GTK_IS_ENTRY(search_entry), FALSE);
    g_return_val_if_fail(BEDIT_IS_FILE_BROWSER_WIDGET(obj), FALSE);

    // TODO This is ignored in GTK3 because the popover holds a grab and
    // intercepts.  Should just work in GTK4.
    if (event->keyval == GDK_KEY_Escape) {
        bedit_file_browser_widget_set_search_enabled(obj, FALSE);
        return TRUE;
    }

    return FALSE;
}

static void on_selection_changed(
    GtkTreeSelection *selection, BeditFileBrowserWidget *obj
) {
    GtkTreeModel *model = gtk_tree_view_get_model(
        GTK_TREE_VIEW(obj->priv->tree_view)
    );
    GAction *action;
    guint selected = 0;
    guint files = 0;
    guint dirs = 0;

    if (BEDIT_IS_FILE_BROWSER_STORE(model)) {
        selected =
            bedit_file_browser_widget_get_num_selected_files_or_directories(
                obj, &files, &dirs
            );
    }

    action = g_action_map_lookup_action(
        G_ACTION_MAP(obj->priv->action_group), "move_to_trash"
    );
    g_simple_action_set_enabled(G_SIMPLE_ACTION(action), selected > 0);

    action = g_action_map_lookup_action(
        G_ACTION_MAP(obj->priv->action_group), "delete"
    );
    g_simple_action_set_enabled(G_SIMPLE_ACTION(action), selected > 0);

    action = g_action_map_lookup_action(
        G_ACTION_MAP(obj->priv->action_group), "open"
    );
    g_simple_action_set_enabled(
        G_SIMPLE_ACTION(action), (selected > 0) && (selected == files)
    );

    action = g_action_map_lookup_action(
        G_ACTION_MAP(obj->priv->action_group), "rename"
    );
    g_simple_action_set_enabled(G_SIMPLE_ACTION(action), selected == 1);

    action = g_action_map_lookup_action(
        G_ACTION_MAP(obj->priv->action_group), "open_in_terminal"
    );
    g_simple_action_set_enabled(G_SIMPLE_ACTION(action), selected == 1);

    action = g_action_map_lookup_action(
        G_ACTION_MAP(obj->priv->action_group), "new_folder"
    );
    g_simple_action_set_enabled(G_SIMPLE_ACTION(action), selected <= 1);

    action = g_action_map_lookup_action(
        G_ACTION_MAP(obj->priv->action_group), "new_file"
    );
    g_simple_action_set_enabled(G_SIMPLE_ACTION(action), selected <= 1);
}

static void on_filter_mode_changed(
    BeditFileBrowserStore *model, GParamSpec *param,
    BeditFileBrowserWidget *obj
) {
    gint mode = bedit_file_browser_store_get_filter_mode(model);
    GAction *action;
    GVariant *variant;
    gboolean active;

    action = g_action_map_lookup_action(
        G_ACTION_MAP(obj->priv->action_group), "show_hidden"
    );
    active = !(mode & BEDIT_FILE_BROWSER_STORE_FILTER_MODE_HIDE_HIDDEN);
    variant = g_action_get_state(action);

    if (active != g_variant_get_boolean(variant)) {
        g_action_change_state(action, g_variant_new_boolean(active));
    }

    g_variant_unref(variant);

    action = g_action_map_lookup_action(
        G_ACTION_MAP(obj->priv->action_group), "show_binary"
    );
    active = !(mode & BEDIT_FILE_BROWSER_STORE_FILTER_MODE_HIDE_BINARY);
    variant = g_action_get_state(action);

    if (active != g_variant_get_boolean(variant)) {
        g_action_change_state(action, g_variant_new_boolean(active));
    }
    g_variant_unref(variant);
}

static void up_activated(
    GSimpleAction *action, GVariant *parameter, gpointer user_data
) {
    GFile *old_vroot, *new_vroot;
    BeditFileBrowserWidget *widget;
    GtkTreeView *view;
    BeditFileBrowserStore *store;

    widget = BEDIT_FILE_BROWSER_WIDGET(user_data);
    view = GTK_TREE_VIEW(widget->priv->tree_view);
    store = BEDIT_FILE_BROWSER_STORE(gtk_tree_view_get_model(view));

    old_vroot = bedit_file_browser_store_get_virtual_root(store);

    bedit_file_browser_store_set_virtual_root_up(store);

    new_vroot = bedit_file_browser_store_get_virtual_root(store);

    if (new_vroot != old_vroot && old_vroot != NULL) {
        GtkTreeIter iter;
        gboolean has_next;

        has_next = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter);

        while (has_next) {
            GFile *location;

            gtk_tree_model_get(
                GTK_TREE_MODEL(store), &iter,
                BEDIT_FILE_BROWSER_STORE_COLUMN_LOCATION, &location,
                -1
            );

            if (g_file_equal(location, old_vroot)) {
                GtkTreeSelection *selection;

                selection = gtk_tree_view_get_selection(view);
                gtk_tree_selection_select_iter(selection, &iter);

                break;
            }

            g_object_unref(location);

            has_next = gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter);
        }
    }

    g_object_unref(old_vroot);
    g_object_unref(new_vroot);
}

static void home_activated(
    GSimpleAction *action, GVariant *parameter, gpointer user_data
) {
    BeditFileBrowserWidget *widget = BEDIT_FILE_BROWSER_WIDGET(user_data);
    GtkTreeModel *model = gtk_tree_view_get_model(
        GTK_TREE_VIEW(widget->priv->tree_view)
    );
    GFile *home_location;

    if (!BEDIT_IS_FILE_BROWSER_STORE(model)) {
        return;
    }

    home_location = g_file_new_for_path(g_get_home_dir());
    bedit_file_browser_widget_set_virtual_root(widget, home_location);

    g_object_unref(home_location);
}

static void new_folder_activated(
    GSimpleAction *action, GVariant *parameter, gpointer user_data
) {
    BeditFileBrowserWidget *widget = BEDIT_FILE_BROWSER_WIDGET(user_data);
    GtkTreeModel *model = gtk_tree_view_get_model(
        GTK_TREE_VIEW(widget->priv->tree_view)
    );
    GtkTreeIter parent;
    GtkTreeIter iter;

    if (!BEDIT_IS_FILE_BROWSER_STORE(model)) {
        return;
    }

    if (!bedit_file_browser_widget_get_selected_directory(widget, &parent)) {
        return;
    }

    if (bedit_file_browser_store_new_directory(
        BEDIT_FILE_BROWSER_STORE(model), &parent, &iter
    )) {
        bedit_file_browser_view_start_rename(widget->priv->tree_view, &iter);
    }
}

static void open_activated(
    GSimpleAction *action, GVariant *parameter, gpointer user_data
) {
    BeditFileBrowserWidget *widget = BEDIT_FILE_BROWSER_WIDGET(user_data);
    GtkTreeModel *model = gtk_tree_view_get_model(
        GTK_TREE_VIEW(widget->priv->tree_view)
    );
    GtkTreeSelection *selection = gtk_tree_view_get_selection(
        GTK_TREE_VIEW(widget->priv->tree_view)
    );
    GList *rows;
    GList *row;

    if (!BEDIT_IS_FILE_BROWSER_STORE(model)) {
        return;
    }

    rows = gtk_tree_selection_get_selected_rows(selection, &model);
    for (row = rows; row; row = row->next) {
        GtkTreePath *path = (GtkTreePath *)(row->data);
        GtkTreeIter iter;

        if (gtk_tree_model_get_iter(model, &iter, path)) {
            file_open(widget, model, &iter);
        }

        gtk_tree_path_free(path);
    }

    g_list_free(rows);
}

static void new_file_activated(
    GSimpleAction *action, GVariant *parameter, gpointer user_data
) {
    BeditFileBrowserWidget *widget = BEDIT_FILE_BROWSER_WIDGET(user_data);
    GtkTreeModel *model = gtk_tree_view_get_model(
        GTK_TREE_VIEW(widget->priv->tree_view)
    );
    GtkTreeIter parent;
    GtkTreeIter iter;

    if (!BEDIT_IS_FILE_BROWSER_STORE(model)) {
        return;
    }

    if (!bedit_file_browser_widget_get_selected_directory(widget, &parent)) {
        return;
    }

    if (bedit_file_browser_store_new_file(
        BEDIT_FILE_BROWSER_STORE(model), &parent, &iter
    )) {
        bedit_file_browser_view_start_rename(widget->priv->tree_view, &iter);
    }
}

static void rename_activated(
    GSimpleAction *action, GVariant *parameter, gpointer user_data
) {
    BeditFileBrowserWidget *widget = BEDIT_FILE_BROWSER_WIDGET(user_data);

    rename_selected_file(widget);
}

static void delete_activated(
    GSimpleAction *action, GVariant *parameter, gpointer user_data
) {
    BeditFileBrowserWidget *widget = BEDIT_FILE_BROWSER_WIDGET(user_data);

    delete_selected_files(widget, FALSE);
}

static void move_to_trash_activated(
    GSimpleAction *action, GVariant *parameter, gpointer user_data
) {
    BeditFileBrowserWidget *widget = BEDIT_FILE_BROWSER_WIDGET(user_data);

    delete_selected_files(widget, TRUE);
}

static void refresh_view_activated(
    GSimpleAction *action, GVariant *parameter, gpointer user_data
) {
    BeditFileBrowserWidget *widget = BEDIT_FILE_BROWSER_WIDGET(user_data);

    bedit_file_browser_widget_refresh(widget);
}

static void view_folder_activated(
    GSimpleAction *action, GVariant *parameter, gpointer user_data
) {
    BeditFileBrowserWidget *widget = BEDIT_FILE_BROWSER_WIDGET(user_data);
    GtkTreeModel *model = gtk_tree_view_get_model(
        GTK_TREE_VIEW(widget->priv->tree_view)
    );
    GtkTreeSelection *selection = gtk_tree_view_get_selection(
        GTK_TREE_VIEW(widget->priv->tree_view)
    );
    GtkTreeIter iter;
    GList *rows;
    GList *row;
    gboolean directory_opened = FALSE;

    if (!BEDIT_IS_FILE_BROWSER_STORE(model)) {
        return;
    }

    rows = gtk_tree_selection_get_selected_rows(selection, &model);
    for (row = rows; row; row = row->next) {
        GtkTreePath *path = (GtkTreePath *)(row->data);

        if (gtk_tree_model_get_iter(model, &iter, path)) {
            directory_opened |= directory_open(widget, model, &iter);
        }

        gtk_tree_path_free(path);
    }

    if (
        !directory_opened &&
        bedit_file_browser_widget_get_selected_directory(widget, &iter)
    ) {
        directory_open(widget, model, &iter);
    }

    g_list_free(rows);
}

static void change_show_hidden_state(
    GSimpleAction *action, GVariant *state, gpointer user_data
) {
    BeditFileBrowserWidget *widget = BEDIT_FILE_BROWSER_WIDGET(user_data);

    update_filter_mode(
        widget, action, state,
        BEDIT_FILE_BROWSER_STORE_FILTER_MODE_HIDE_HIDDEN
    );
}

static void change_show_binary_state(
    GSimpleAction *action, GVariant *state, gpointer user_data
) {
    BeditFileBrowserWidget *widget = BEDIT_FILE_BROWSER_WIDGET(user_data);

    update_filter_mode(
        widget, action, state,
        BEDIT_FILE_BROWSER_STORE_FILTER_MODE_HIDE_BINARY
    );
}

static void open_in_terminal_activated(
    GSimpleAction *action, GVariant *parameter, gpointer user_data
) {
    BeditFileBrowserWidget *widget = BEDIT_FILE_BROWSER_WIDGET(user_data);
    GtkTreeIter iter;
    GFile *file;

    /* Get the current directory */
    if (!bedit_file_browser_widget_get_selected_directory(widget, &iter)) {
        return;
    }

    gtk_tree_model_get(
        GTK_TREE_MODEL(widget->priv->file_store), &iter,
        BEDIT_FILE_BROWSER_STORE_COLUMN_LOCATION, &file,
        -1
    );

    g_signal_emit(widget, signals[OPEN_IN_TERMINAL], 0, file);

    g_object_unref(file);
}

static void set_active_root_activated(
    GSimpleAction *action, GVariant *parameter, gpointer user_data
) {
    BeditFileBrowserWidget *widget = BEDIT_FILE_BROWSER_WIDGET(user_data);

    g_signal_emit(widget, signals[SET_ACTIVE_ROOT], 0);
}

void _bedit_file_browser_widget_register_type(GTypeModule *type_module) {
    bedit_file_browser_widget_register_type(type_module);
}

