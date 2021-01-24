/*
 * bedit-file-browser-location.c
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

#include "bedit-file-browser-location.h"

#include <bedit/bedit-utils.h>
#include <gdk/gdkkeysyms.h>
#include <glib.h>
#include <glib/gi18n-lib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "bedit-file-browser-bookmarks-store.h"
#include "bedit-file-browser-store.h"
#include "bedit-file-browser-utils.h"

#include <gtk/gtk.h>

enum {
    BOOKMARKS_ID,
    SEPARATOR_CUSTOM_ID,
    SEPARATOR_ID,
    PATH_ID,
    N_TYPE_IDS
};

enum {
    COLUMN_ICON,
    COLUMN_ICON_NAME,
    COLUMN_NAME,
    COLUMN_FILE,
    COLUMN_ID,
    N_COLUMNS
};

struct _BeditFileBrowserLocation {
    GtkMenuButton parent_instance;

    BeditFileBrowserBookmarksStore *bookmarks_store;
    BeditFileBrowserStore *file_store;

    GtkPopover *locations_popover;
    GtkTreeView *locations_treeview;
    GtkTreeViewColumn *treeview_icon_column;
    GtkCellRenderer *treeview_icon_renderer;
    GtkTreeSelection *locations_treeview_selection;
    GtkImage *locations_button_arrow;
    GtkCellView *locations_cellview;
    GtkListStore *locations_model;

    gulong virtual_root_changed_id;
};

enum {
    PROP_0,
    PROP_BOOKMARKS_STORE,
    PROP_FILE_STORE,
    LAST_PROP,
};

static GParamSpec *properties[LAST_PROP];

G_DEFINE_TYPE(
    BeditFileBrowserLocation, bedit_file_browser_location, GTK_TYPE_MENU_BUTTON
)

static void on_locations_treeview_selection_changed(
    GtkTreeSelection *treeview_selection, BeditFileBrowserLocation *widget
);
static void on_virtual_root_changed(
    BeditFileBrowserStore *model, GParamSpec *param,
    BeditFileBrowserLocation *widget
);

static void bedit_file_browser_location_get_property(
    GObject *object, guint prop_id, GValue *value, GParamSpec *pspec
) {
    BeditFileBrowserLocation *widget = BEDIT_FILE_BROWSER_LOCATION(object);

    switch (prop_id) {
    case PROP_BOOKMARKS_STORE:
        g_value_take_object(
            value, bedit_file_browser_location_get_bookmarks_store(widget)
        );
        break;

    case PROP_FILE_STORE:
        g_value_take_object(
            value, bedit_file_browser_location_get_file_store(widget)
        );
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void bedit_file_browser_location_set_property(
    GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec
) {
    BeditFileBrowserLocation *obj = BEDIT_FILE_BROWSER_LOCATION(object);

    switch (prop_id) {
    case PROP_BOOKMARKS_STORE:
        bedit_file_browser_location_set_bookmarks_store(
            obj, BEDIT_FILE_BROWSER_BOOKMARKS_STORE(g_value_dup_object(value))
        );
        break;

    case PROP_FILE_STORE:
        bedit_file_browser_location_set_file_store(
            obj, BEDIT_FILE_BROWSER_STORE(g_value_dup_object(value))
        );
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void bedit_file_browser_location_class_init(
    BeditFileBrowserLocationClass *klass
) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    object_class->get_property = bedit_file_browser_location_get_property;
    object_class->set_property = bedit_file_browser_location_set_property;

    properties[PROP_BOOKMARKS_STORE] = g_param_spec_object(
        "bookmarks-store", "Bookmarks Store",
        "Object that tracks bookmark state",
        BEDIT_TYPE_FILE_BROWSER_BOOKMARKS_STORE,
        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS
    );

    properties[PROP_FILE_STORE] = g_param_spec_object(
        "file-store", "File Store",
        "Object that tracks the current view of the filesystem",
        BEDIT_TYPE_FILE_BROWSER_STORE,
        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS
    );

    g_object_class_install_properties(object_class, LAST_PROP, properties);

    gtk_widget_class_set_template_from_resource(
        widget_class,
        "/com/bwhmather/bedit/ui/"
        "bedit-file-browser-location.ui"
    );

    gtk_widget_class_bind_template_child(
        widget_class, BeditFileBrowserLocation, locations_popover
    );

    gtk_widget_class_bind_template_child(
        widget_class, BeditFileBrowserLocation, locations_treeview
    );

    gtk_widget_class_bind_template_child(
        widget_class, BeditFileBrowserLocation, treeview_icon_column
    );

    gtk_widget_class_bind_template_child(
        widget_class, BeditFileBrowserLocation, treeview_icon_renderer
    );

    gtk_widget_class_bind_template_child(
        widget_class, BeditFileBrowserLocation, locations_treeview_selection
    );

    gtk_widget_class_bind_template_child(
        widget_class, BeditFileBrowserLocation, locations_button_arrow
    );

    gtk_widget_class_bind_template_child(
        widget_class, BeditFileBrowserLocation, locations_cellview
    );

    gtk_widget_class_bind_template_child(
        widget_class, BeditFileBrowserLocation, locations_model
    );
}

static gboolean locations_find_by_id(
    BeditFileBrowserLocation *widget, guint id, GtkTreeIter *iter
) {
    GtkTreeModel *model = GTK_TREE_MODEL(widget->locations_model);
    guint checkid;

    if (iter == NULL) {
        return FALSE;
    }

    if (gtk_tree_model_get_iter_first(model, iter)) {
        do {
            gtk_tree_model_get(model, iter, COLUMN_ID, &checkid, -1);

            if (checkid == id) {
                return TRUE;
            }
        } while (gtk_tree_model_iter_next(model, iter));
    }

    return FALSE;
}

static gboolean separator_func(
    GtkTreeModel *model, GtkTreeIter *iter, gpointer data
) {
    guint id;

    gtk_tree_model_get(model, iter, COLUMN_ID, &id, -1);

    return (id == SEPARATOR_ID);
}



static void insert_path_item(
    BeditFileBrowserLocation *widget, GFile *file, GtkTreeIter *after,
    GtkTreeIter *iter
) {
    gchar *unescape = NULL;
    gchar *icon_name = NULL;
    GdkPixbuf *icon = NULL;

    unescape = bedit_file_browser_utils_file_basename(file);
    icon_name = bedit_file_browser_utils_symbolic_icon_name_from_file(file);

    gtk_list_store_insert_after(widget->locations_model, iter, after);

    gtk_list_store_set(
        widget->locations_model, iter,
        COLUMN_ICON, icon,
        COLUMN_ICON_NAME, icon_name,
        COLUMN_NAME, unescape,
        COLUMN_FILE, file,
        COLUMN_ID, PATH_ID,
        -1
    );

    if (icon) {
        g_object_unref(icon);
    }

    g_free(icon_name);
    g_free(unescape);
}

static void insert_separator_item(BeditFileBrowserLocation *widget) {
    GtkTreeIter iter;

    gtk_list_store_insert(widget->locations_model, &iter, 1);
    gtk_list_store_set(
        widget->locations_model, &iter,
        COLUMN_ICON, NULL,
        COLUMN_ICON_NAME, NULL,
        COLUMN_NAME, NULL,
        COLUMN_ID, SEPARATOR_ID,
        -1
    );
}

static void insert_location_path(BeditFileBrowserLocation *widget) {
    GFile *root;
    GFile *virtual_root;
    GFile *current = NULL;
    GFile *tmp;
    GtkTreeIter separator;
    GtkTreeIter iter;

    g_return_if_fail(BEDIT_IS_FILE_BROWSER_STORE(widget->file_store));

    root = bedit_file_browser_store_get_root(widget->file_store);
    virtual_root = bedit_file_browser_store_get_virtual_root(widget->file_store);

    g_return_if_fail(G_IS_FILE(root));
    g_return_if_fail(G_IS_FILE(virtual_root));

    current = virtual_root;
    locations_find_by_id(widget, SEPARATOR_ID, &separator);

    while (current != NULL) {
        insert_path_item(widget, current, &separator, &iter);

        if (current == virtual_root) {
            g_signal_handlers_block_by_func(
                widget->locations_treeview,
                on_locations_treeview_selection_changed, widget
            );

            gtk_tree_selection_select_iter(
                widget->locations_treeview_selection, &iter
            );

            g_signal_handlers_unblock_by_func(
                widget->locations_treeview,
                on_locations_treeview_selection_changed, widget
            );
        }

        if (
            g_file_equal(current, root) ||
            !g_file_has_parent(current, NULL)
        ) {
            if (current != virtual_root) {
                g_object_unref(current);
            }
            break;
        }

        tmp = g_file_get_parent(current);

        if (current != virtual_root) {
            g_object_unref(current);
        }

        current = tmp;
    }
}

static void remove_path_items(BeditFileBrowserLocation *widget) {
    GtkTreeIter iter;

    while (locations_find_by_id(widget, PATH_ID, &iter)) {
        gtk_list_store_remove(widget->locations_model, &iter);
    }
}

static void check_current_item(
    BeditFileBrowserLocation *widget, gboolean show_path
) {
    GtkTreeIter separator;
    gboolean has_sep;

    remove_path_items(widget);
    has_sep = locations_find_by_id(widget, SEPARATOR_ID, &separator);

    if (show_path) {
        if (!has_sep) {
            insert_separator_item(widget);
        }

        insert_location_path(widget);
    } else if (has_sep) {
        gtk_list_store_remove(widget->locations_model, &separator);
    }
}

static void on_locations_treeview_row_activated(
    GtkTreeView *locations_treeview, GtkTreePath *path,
    GtkTreeViewColumn *column, BeditFileBrowserLocation *widget
) {
    GtkTreeIter iter;
    guint id = G_MAXUINT;
    GFile *file;

    if (gtk_tree_model_get_iter(
        GTK_TREE_MODEL(widget->locations_model), &iter, path
    )) {
        gtk_tree_model_get(
            GTK_TREE_MODEL(widget->locations_model), &iter,
            COLUMN_ID, &id,
            -1
        );
    }

    if (id == PATH_ID) {
        gtk_tree_model_get(
            GTK_TREE_MODEL(widget->locations_model), &iter,
            COLUMN_FILE, &file,
            -1
        );

        bedit_file_browser_store_set_virtual_root_from_location(
            widget->file_store, file
        );

        g_object_unref(file);
        gtk_cell_view_set_displayed_row(
            GTK_CELL_VIEW(widget->locations_cellview), path
        );
    }

    gtk_toggle_button_set_active(
        GTK_TOGGLE_BUTTON(widget), FALSE
    );
}

static void locations_icon_renderer_cb(
    GtkTreeViewColumn *tree_column, GtkCellRenderer *cell,
    GtkTreeModel *tree_model, GtkTreeIter *iter, BeditFileBrowserLocation *widget
) {
    GdkPixbuf *pixbuf;
    gchar *icon_name;

    gtk_tree_model_get(
        tree_model, iter,
        BEDIT_FILE_BROWSER_BOOKMARKS_STORE_COLUMN_ICON_NAME, &icon_name,
        BEDIT_FILE_BROWSER_BOOKMARKS_STORE_COLUMN_ICON, &pixbuf,
        -1
    );

    if (icon_name != NULL) {
        g_object_set(cell, "icon-name", icon_name, NULL);
    } else {
        g_object_set(cell, "pixbuf", pixbuf, NULL);
    }

    g_clear_object(&pixbuf);
    g_free(icon_name);
}

static void bedit_file_browser_location_init(BeditFileBrowserLocation *widget) {
    gtk_widget_init_template(GTK_WIDGET(widget));

    widget->bookmarks_store = NULL;
    widget->file_store = NULL;

    gtk_tree_view_set_row_separator_func(
        widget->locations_treeview, separator_func, widget, NULL
    );
    gtk_tree_selection_set_mode(
        widget->locations_treeview_selection, GTK_SELECTION_SINGLE
    );
    gtk_tree_view_column_set_cell_data_func(
        widget->treeview_icon_column, widget->treeview_icon_renderer,
        (GtkTreeCellDataFunc)locations_icon_renderer_cb, widget, NULL
    );

    g_signal_connect(
        widget->locations_treeview_selection, "changed",
        G_CALLBACK(on_locations_treeview_selection_changed), widget
    );
    g_signal_connect(
        widget->locations_treeview, "row-activated",
        G_CALLBACK(on_locations_treeview_row_activated), widget
    );
}

/**
 * bedit_file_browser_location_new:
 *
 * Creates a new #BeditFileBrowserLocation.
 *
 * Return value: the new #BeditFileBrowserLocation object
 **/
GtkWidget *bedit_file_browser_location_new(void) {
    return GTK_WIDGET(g_object_new(BEDIT_TYPE_FILE_BROWSER_LOCATION, NULL));
}

void bedit_file_browser_location_set_bookmarks_store(
    BeditFileBrowserLocation *widget, BeditFileBrowserBookmarksStore *store
) {
    gboolean updated;

    g_return_if_fail(BEDIT_IS_FILE_BROWSER_LOCATION(widget));
    g_return_if_fail(BEDIT_IS_FILE_BROWSER_BOOKMARKS_STORE(store));

    updated = widget->bookmarks_store != store;
    widget->bookmarks_store = store;

    if (updated) {
        g_object_notify(G_OBJECT(widget), "bookmarks-store");
    }
}

BeditFileBrowserBookmarksStore *bedit_file_browser_location_get_bookmarks_store(
    BeditFileBrowserLocation *widget
) {
    g_return_val_if_fail(BEDIT_IS_FILE_BROWSER_LOCATION(widget), NULL);

    if (widget->bookmarks_store != NULL) {
        g_object_ref(widget->bookmarks_store);
    }

    return widget->bookmarks_store;
}

void bedit_file_browser_location_set_file_store(
    BeditFileBrowserLocation *widget, BeditFileBrowserStore *store
) {
    gboolean updated;

    g_return_if_fail(BEDIT_IS_FILE_BROWSER_LOCATION(widget));
    g_return_if_fail(BEDIT_IS_FILE_BROWSER_STORE(store));

    updated = widget->file_store != store;

    if (widget->file_store != NULL) {
        g_signal_handler_disconnect(
            widget->file_store, widget->virtual_root_changed_id
        );
    }

    widget->file_store = store;

    if (widget->file_store != NULL) {
        g_signal_connect(
            widget->file_store, "notify::virtual-root",
            G_CALLBACK(on_virtual_root_changed), widget
        );
    }

    if (updated) {
        g_object_notify(G_OBJECT(widget), "file-store");
    }
}

BeditFileBrowserStore *bedit_file_browser_location_get_file_store(
    BeditFileBrowserLocation *widget
) {
    g_return_val_if_fail(BEDIT_IS_FILE_BROWSER_LOCATION(widget), NULL);

    if (widget->file_store != NULL) {
        g_object_ref(widget->file_store);
    }

    return widget->file_store;
}

static void on_locations_treeview_selection_changed(
    GtkTreeSelection *treeview_selection, BeditFileBrowserLocation *widget
) {
    GtkTreeModel *model = GTK_TREE_MODEL(widget->locations_model);
    GtkTreePath *path;
    GtkTreeIter iter;

    if (!gtk_tree_selection_get_selected(treeview_selection, &model, &iter)) {
        return;
    }

    path = gtk_tree_model_get_path(
        GTK_TREE_MODEL(widget->locations_model), &iter
    );
    gtk_cell_view_set_displayed_row(
        GTK_CELL_VIEW(widget->locations_cellview), path
    );
    gtk_tree_path_free(path);
}

static void on_virtual_root_changed(
    BeditFileBrowserStore *model, GParamSpec *param,
    BeditFileBrowserLocation *widget
) {
    GtkTreeIter iter;

    g_return_if_fail(BEDIT_IS_FILE_BROWSER_STORE(model));
    g_return_if_fail(BEDIT_IS_FILE_BROWSER_LOCATION(widget));

    if (bedit_file_browser_store_get_iter_virtual_root(model, &iter)) {
        check_current_item(widget, TRUE);
    }
}
