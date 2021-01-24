/*
 * bedit-file-browser-filter-view.c
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
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

#include "bedit-file-browser-filter-view.h"

#include <gmodule.h>
#include <gtk/gtk.h>

#include "bedit/bedit-debug.h"

#include "bedit-file-browser-filter-match.h"
#include "bedit-file-browser-filter-dir-enumerator.h"
#include "bedit-file-browser-filter-root-dir-enumerator.h"
#include "bedit-file-browser-filter-child-dir-enumerator.h"
#include "bedit-file-browser-filter-parent-dir-enumerator.h"
#include "bedit-file-browser-filter-file-enumerator.h"

struct _BeditFileBrowserFilterView {
    GtkBin parent_instance;

    GtkTreeView *tree_view;

    GFile *virtual_root;
    gchar *query;

    GCancellable *cancellable;

    guint enabled : 1;
};

G_DEFINE_TYPE(
    BeditFileBrowserFilterView, bedit_file_browser_filter_view, GTK_TYPE_BIN
)

enum {
    PROP_0,
    PROP_VIRTUAL_ROOT,
    PROP_QUERY,
    PROP_ENABLED,
    LAST_PROP,
};

enum {
    SIGNAL_FILE_ACTIVATED,
    SIGNAL_DIRECTORY_ACTIVATED,
    LAST_SIGNAL,
};

static guint signals[LAST_SIGNAL];

typedef enum {
    COLUMN_ICON = 0,
    COLUMN_MARKUP,
    COLUMN_LOCATION,
    COLUMN_FILE_INFO,
    N_COLUMNS,
} BeditFileBrowserFilterStoreColumn;

static void bedit_file_browser_filter_view_activate_selected(
    BeditFileBrowserFilterView *view
);

static void bedit_file_browser_filter_view_on_row_activated(
    GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column,
    BeditFileBrowserFilterView *view
);

static void bedit_file_browser_filter_view_refresh(
    BeditFileBrowserFilterView *view
);

static gboolean bedit_file_browser_filter_view_on_key_press(
    GtkWidget *widget, GdkEventKey *event
);

static void bedit_file_browser_filter_view_get_property(
    GObject *object, guint prop_id, GValue *value, GParamSpec *pspec
) {
    BeditFileBrowserFilterView *view = BEDIT_FILE_BROWSER_FILTER_VIEW(object);

    switch (prop_id) {
    case PROP_VIRTUAL_ROOT:
        g_value_take_object(
            value, bedit_file_browser_filter_view_get_virtual_root(view)
        );
        break;

    case PROP_QUERY:
        g_value_take_string(
            value, bedit_file_browser_filter_view_get_query(view)
        );
        break;

    case PROP_ENABLED:
        g_value_set_boolean(
            value, bedit_file_browser_filter_view_get_enabled(view)
        );
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void bedit_file_browser_filter_view_set_property(
    GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec
) {
    BeditFileBrowserFilterView *view = BEDIT_FILE_BROWSER_FILTER_VIEW(object);

    switch (prop_id) {
    case PROP_VIRTUAL_ROOT:
        bedit_file_browser_filter_view_set_virtual_root(
            view, G_FILE(g_value_dup_object(value))
        );
        break;

    case PROP_QUERY:
        bedit_file_browser_filter_view_set_query(
            view, g_value_get_string(value)
        );
        break;

    case PROP_ENABLED:
        bedit_file_browser_filter_view_set_enabled(
            view, g_value_get_boolean(value)
        );
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void bedit_file_browser_filter_view_grab_focus(GtkWidget *widget) {
    BeditFileBrowserFilterView *view;

    view = BEDIT_FILE_BROWSER_FILTER_VIEW(widget);
    gtk_widget_grab_focus(GTK_WIDGET(view->tree_view));
}

static void bedit_file_browser_filter_view_class_init(
    BeditFileBrowserFilterViewClass *klass
) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    object_class->get_property = bedit_file_browser_filter_view_get_property;
    object_class->set_property = bedit_file_browser_filter_view_set_property;

    g_object_class_install_property(
        object_class, PROP_VIRTUAL_ROOT,
        g_param_spec_object(
            "virtual-root", "Virtual Root",
            "The location in the filesystem that widget is currently showing",
            G_TYPE_FILE,
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
            G_PARAM_EXPLICIT_NOTIFY
        )
    );

    g_object_class_install_property(
        object_class, PROP_QUERY,
        g_param_spec_string(
            "query", "Query",
            "Query string to use to match files under the current virtual root",
            "",
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
            G_PARAM_EXPLICIT_NOTIFY
        )
    );

    g_object_class_install_property(
        object_class, PROP_ENABLED,
        g_param_spec_boolean(
            "enabled", "Enabled",
            "Set to false to clear the cache and tell view to ignore all input",
            FALSE,
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
            G_PARAM_EXPLICIT_NOTIFY
        )
    );

    signals[SIGNAL_FILE_ACTIVATED] = g_signal_new(
        "file-activated", G_TYPE_FROM_CLASS(klass),
        G_SIGNAL_RUN_FIRST,
        0,
        NULL, NULL,
        NULL,
        G_TYPE_NONE, 1, G_TYPE_FILE
    );
    signals[SIGNAL_DIRECTORY_ACTIVATED] = g_signal_new(
        "directory-activated", G_TYPE_FROM_CLASS(klass),
        G_SIGNAL_RUN_FIRST,
        0,
        NULL, NULL,
        NULL,
        G_TYPE_NONE, 1, G_TYPE_FILE
    );

    widget_class->grab_focus = bedit_file_browser_filter_view_grab_focus;
    widget_class->key_press_event = bedit_file_browser_filter_view_on_key_press;

    gtk_widget_class_set_template_from_resource(
        widget_class,
        "/com/bwhmather/bedit/ui/"
        "bedit-file-browser-filter-view.ui"
    );

    gtk_widget_class_bind_template_child(
        widget_class, BeditFileBrowserFilterView, tree_view
    );
}

static void bedit_file_browser_filter_view_init(
    BeditFileBrowserFilterView *view
) {
    view->cancellable = NULL;

    gtk_widget_init_template(GTK_WIDGET(view));

    g_signal_connect(
        view->tree_view, "row-activated",
        G_CALLBACK(bedit_file_browser_filter_view_on_row_activated), view
    );
}

/**
 * bedit_file_browser_filter_view_new:
 *
 * Creates a new #BeditFileBrowserFilterView.
 *
 * Return value: the new #BeditFileBrowserFilterView object
 **/
BeditFileBrowserFilterView *bedit_file_browser_filter_view_new(void) {
    return g_object_new(BEDIT_TYPE_FILE_BROWSER_FILTER_VIEW, NULL);
}

void bedit_file_browser_filter_view_set_virtual_root(
    BeditFileBrowserFilterView *view, GFile *virtual_root
) {
    gboolean updated = FALSE;

    g_return_if_fail(BEDIT_IS_FILE_BROWSER_FILTER_VIEW(view));
    g_return_if_fail(G_IS_FILE(virtual_root));

    if (view->virtual_root != NULL) {
        updated = !g_file_equal(virtual_root, view->virtual_root);
        g_object_unref(view->virtual_root);
    } else {
        updated = virtual_root != NULL;
    }

    view->virtual_root = g_object_ref(virtual_root);

    if (updated) {
        g_object_notify(G_OBJECT(view), "virtual-root");
        bedit_file_browser_filter_view_refresh(view);
    }
}

GFile *bedit_file_browser_filter_view_get_virtual_root(
    BeditFileBrowserFilterView *view
) {
    g_return_val_if_fail(BEDIT_IS_FILE_BROWSER_FILTER_VIEW(view), NULL);

    if (view->virtual_root != NULL) {
        g_object_ref(view->virtual_root);
    }

    return view->virtual_root;
}

void bedit_file_browser_filter_view_set_query(
    BeditFileBrowserFilterView *view, const gchar *query
) {
    g_return_if_fail(BEDIT_IS_FILE_BROWSER_FILTER_VIEW(view));

    bedit_debug_message(DEBUG_PLUGINS, "query: %s", query);

    if (g_strcmp0(query, view->query)) {
        bedit_debug(DEBUG_PLUGINS);
        view->query = g_strdup(query);
        g_object_notify(G_OBJECT(view), "query");
        bedit_file_browser_filter_view_refresh(view);
    }
}

gchar *bedit_file_browser_filter_view_get_query(
    BeditFileBrowserFilterView *view
) {
    g_return_val_if_fail(BEDIT_IS_FILE_BROWSER_FILTER_VIEW(view), NULL);

    return view->query;
}

void bedit_file_browser_filter_view_set_enabled(
    BeditFileBrowserFilterView *view, gboolean enabled
) {
    g_return_if_fail(BEDIT_IS_FILE_BROWSER_FILTER_VIEW(view));

    if (view->enabled != enabled) {
        view->enabled = enabled;
        g_object_notify(G_OBJECT(view), "enabled");
        if (enabled) {
            bedit_file_browser_filter_view_refresh(view);
        } else if (view->cancellable != NULL) {
            g_cancellable_cancel(view->cancellable);
            g_clear_object(&view->cancellable);
        }
    }
}

gboolean bedit_file_browser_filter_view_get_enabled(
    BeditFileBrowserFilterView *view
) {
    return view->enabled ? TRUE : FALSE;
}

static void bedit_file_browser_filter_view_activate_selected(
    BeditFileBrowserFilterView *view
) {
    GtkTreeModel *model;
    GtkTreeSelection *selection;
    GList *rows;
    GFile *directory = NULL;

    g_return_if_fail(BEDIT_IS_FILE_BROWSER_FILTER_VIEW(view));

    model = gtk_tree_view_get_model(view->tree_view);

    selection = gtk_tree_view_get_selection(view->tree_view);
    rows = gtk_tree_selection_get_selected_rows(selection, &model);

    for (GList *row = rows; row != NULL; row = row->next) {
        GtkTreePath *path;
        GtkTreeIter iter;
        GFile *file;
        GFileInfo *fileinfo;
        GFileType filetype;

        path = (GtkTreePath *) row->data;

        if (!gtk_tree_model_get_iter(model, &iter, path)) {
            continue;
        }

        gtk_tree_model_get(
            model, &iter,
            COLUMN_LOCATION, &file,
            COLUMN_FILE_INFO, &fileinfo,
            -1
        );

        g_return_if_fail(G_IS_FILE(file));
        g_return_if_fail(G_IS_FILE_INFO(fileinfo));

        filetype = g_file_info_get_file_type(fileinfo);

        if (filetype == G_FILE_TYPE_DIRECTORY && directory == NULL) {
            directory = g_object_ref(file);
        } else {
            g_signal_emit(view, signals[SIGNAL_FILE_ACTIVATED], 0, file);
        }

        g_object_unref(file);
        g_object_unref(fileinfo);
    }

    if (directory != NULL) {
        g_signal_emit(view, signals[SIGNAL_DIRECTORY_ACTIVATED], 0, directory);
        g_object_unref(directory);
    }

    g_list_free_full(rows, (GDestroyNotify) gtk_tree_path_free);
}

static void bedit_file_browser_filter_view_on_row_activated(
    GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column,
    BeditFileBrowserFilterView *view
) {
    GtkTreeSelection *selection;

    g_return_if_fail(GTK_IS_TREE_VIEW(tree_view));
    g_return_if_fail(BEDIT_IS_FILE_BROWSER_FILTER_VIEW(view));

    /* Make sure the activated row is the only one selected */
    selection = gtk_tree_view_get_selection(tree_view);
    gtk_tree_selection_unselect_all(selection);
    gtk_tree_selection_select_path(selection, path);

    bedit_file_browser_filter_view_activate_selected(view);
}

static gboolean bedit_file_browser_filter_view_on_key_press(
    GtkWidget *widget, GdkEventKey *event
) {
    BeditFileBrowserFilterView *view = BEDIT_FILE_BROWSER_FILTER_VIEW(widget);

    switch (event->keyval) {
    case GDK_KEY_Return:
    case GDK_KEY_KP_Enter:
        bedit_file_browser_filter_view_activate_selected(view);
        return TRUE;

    default:
        return GTK_WIDGET_CLASS(
            bedit_file_browser_filter_view_parent_class
        )->key_press_event(widget, event);
    }
}

static void bedit_file_browser_filter_view_refresh(
    BeditFileBrowserFilterView *view
) {
    GError *error = NULL;
    gchar *query;
    gchar *query_cursor;
    gchar const *prefix;
    GFile *virtual_root;
    BeditFileBrowserFilterDirEnumerator *dir_enumerator;
    BeditFileBrowserFilterFileEnumerator *file_enumerator;
    GtkTreeStore *tree_store;
    GtkTreePath *path;
    GtkTreeSelection *selection;

    g_return_if_fail(BEDIT_IS_FILE_BROWSER_FILTER_VIEW(view));

    if (!bedit_file_browser_filter_view_get_enabled(view)) {
        bedit_debug_message(DEBUG_PLUGINS, "refresh skipped");
        return;
    }

    query = bedit_file_browser_filter_view_get_query(view);
    if (query == NULL) {
        bedit_debug_message(DEBUG_PLUGINS, "refresh skipped");
        return;
    }

    virtual_root = bedit_file_browser_filter_view_get_virtual_root(view);
    if (virtual_root == NULL) {
        bedit_debug_message(DEBUG_PLUGINS, "refresh skipped");
        return;
    }

    bedit_debug_message(DEBUG_PLUGINS, "refresh");

    // Cancel previous filter.
    // TODO make filter run in background thread.
    if (view->cancellable != NULL) {
        g_cancellable_cancel(view->cancellable);
        g_clear_object(&view->cancellable);
    }
    view->cancellable = g_cancellable_new();

    if (g_str_has_prefix(query, "~/")) {
        virtual_root = g_file_new_for_path(g_get_home_dir());
        query_cursor = query + 2;
        prefix = "~/";
    } else if (g_str_has_prefix(query, "./")) {
        virtual_root = g_object_ref(virtual_root);
        query_cursor = query + 2;
        prefix = "./";
    } else if (g_str_has_prefix(query, "/")) {
        virtual_root = g_file_new_for_path("/");
        query_cursor = query + 1;
        prefix = "/";
    } else {
        virtual_root = g_object_ref(virtual_root);
        query_cursor = query;
        prefix = "";
    }

    dir_enumerator = BEDIT_FILE_BROWSER_FILTER_DIR_ENUMERATOR(
        bedit_file_browser_filter_root_dir_enumerator_new(virtual_root, prefix)
    );
    g_return_if_fail(
        BEDIT_IS_FILE_BROWSER_FILTER_DIR_ENUMERATOR(dir_enumerator)
    );

    while (*query_cursor != '\0') {
        gchar *end;
        gchar *query_segment;
        BeditFileBrowserFilterDirEnumerator *new_enumerator;

        end = strstr(query_cursor, "/");
        if (end == NULL) {
            break;
        }

        query_segment = g_strndup(query_cursor, end - query_cursor);

        if (g_strcmp0(query_segment, "..")) {
            new_enumerator = BEDIT_FILE_BROWSER_FILTER_DIR_ENUMERATOR(
                bedit_file_browser_filter_child_dir_enumerator_new(
                    dir_enumerator, query_segment
                )
            );
        } else {
            new_enumerator = BEDIT_FILE_BROWSER_FILTER_DIR_ENUMERATOR(
                bedit_file_browser_filter_parent_dir_enumerator_new(
                    dir_enumerator
                )
            );
        }

        g_clear_object(&dir_enumerator);
        dir_enumerator = (
            new_enumerator
        );
        g_free(query_segment);

        query_cursor = end + 1;
    }

    file_enumerator = bedit_file_browser_filter_file_enumerator_new(
        dir_enumerator, query_cursor
    );
    g_return_if_fail(
        BEDIT_IS_FILE_BROWSER_FILTER_FILE_ENUMERATOR(file_enumerator)
    );
    g_clear_object(&dir_enumerator);

    tree_store = gtk_tree_store_new(
        N_COLUMNS,
        G_TYPE_ICON,  // Icon name.
        G_TYPE_STRING,  // Markup.
        G_TYPE_FILE,  // Location.
        G_TYPE_FILE_INFO  // Metadata.
    );

    while (TRUE) {
        GFile *match_file;
        GFileInfo *match_info;
        gchar *match_markup;

        if (!bedit_file_browser_filter_file_enumerator_iterate(
            file_enumerator,
            &match_file,
            &match_info,
            &match_markup,
            view->cancellable, &error
        )) {
            break;
        };

        gtk_tree_store_insert_with_values(
            tree_store, NULL, NULL, -1,
            COLUMN_ICON, g_file_info_get_symbolic_icon(match_info),
            COLUMN_MARKUP, match_markup,
            COLUMN_LOCATION, match_file,
            COLUMN_FILE_INFO, match_info,
            -1
        );

        g_object_unref(match_file);
        g_object_unref(match_info);
        g_free(match_markup);
    }

    g_clear_object(&file_enumerator);

    gtk_tree_view_set_model(view->tree_view, GTK_TREE_MODEL(tree_store));
    g_clear_object(&tree_store);

    path = gtk_tree_path_new_first();
    selection = gtk_tree_view_get_selection(view->tree_view);
    gtk_tree_selection_unselect_all(selection);
    gtk_tree_selection_select_path(selection, path);
    gtk_tree_path_free(path);
    g_object_unref(virtual_root);
}
