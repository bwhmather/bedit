#include "config.h"

#include "bedit-file-browser-search-view.h"

#include <gmodule.h>
#include <gtk/gtk.h>

#include "bedit/bedit-debug.h"


#define DIRECTORY_LOAD_ITEMS_PER_CALLBACK 128
#define STANDARD_ATTRIBUTE_TYPES                                            \
    G_FILE_ATTRIBUTE_STANDARD_TYPE ","                                      \
    G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN ","                                 \
    G_FILE_ATTRIBUTE_STANDARD_IS_BACKUP ","                                 \
    G_FILE_ATTRIBUTE_STANDARD_NAME ","                                      \
    G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME ","                              \
    G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","                              \
    G_FILE_ATTRIBUTE_STANDARD_SYMBOLIC_ICON

struct _BeditFileBrowserSearchView {
    GtkBin parent_instance;

    GtkTreeView *tree_view;
    GtkListStore *list_store;

    GHashTable *dir_cache;

    GFile *virtual_root;
    gchar *query;

    GCancellable *cancellable;

    guint enabled : 1;
};

G_DEFINE_DYNAMIC_TYPE(
    BeditFileBrowserSearchView, bedit_file_browser_search_view, GTK_TYPE_BIN
)

enum {
    PROP_0,
    PROP_VIRTUAL_ROOT,
    PROP_QUERY,
    PROP_ENABLED,
    LAST_PROP,
};

typedef enum {
    COLUMN_ICON_NAME = 0,
    COLUMN_MARKUP,
    COLUMN_LOCATION,
    N_COLUMNS,
} BeditFileBrowserSearchStoreColumn;

static void bedit_file_browser_search_view_refresh(
    BeditFileBrowserSearchView *view
);

static void bedit_file_browser_search_view_get_property(
    GObject *object, guint prop_id, GValue *value, GParamSpec *pspec
) {
    BeditFileBrowserSearchView *view = BEDIT_FILE_BROWSER_SEARCH_VIEW(object);

    switch (prop_id) {
    case PROP_VIRTUAL_ROOT:
        g_value_take_object(
            value, bedit_file_browser_search_view_get_virtual_root(view)
        );
        break;

    case PROP_QUERY:
        g_value_take_string(
            value, bedit_file_browser_search_view_get_query(view)
        );
        break;

    case PROP_ENABLED:
        g_value_set_boolean(
            value, bedit_file_browser_search_view_get_enabled(view)
        );
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void bedit_file_browser_search_view_set_property(
    GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec
) {
    BeditFileBrowserSearchView *view = BEDIT_FILE_BROWSER_SEARCH_VIEW(object);

    switch (prop_id) {
    case PROP_VIRTUAL_ROOT:
        bedit_file_browser_search_view_set_virtual_root(
            view, G_FILE(g_value_dup_object(value))
        );
        break;

    case PROP_QUERY:
        bedit_file_browser_search_view_set_query(
            view, g_value_get_string(value)
        );
        break;

    case PROP_ENABLED:
        bedit_file_browser_search_view_set_enabled(
            view, g_value_get_boolean(value)
        );
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void bedit_file_browser_search_view_class_init(
    BeditFileBrowserSearchViewClass *klass
) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    object_class->get_property = bedit_file_browser_search_view_get_property;
    object_class->set_property = bedit_file_browser_search_view_set_property;

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

    gtk_widget_class_set_template_from_resource(
        widget_class,
        "/com/bwhmather/bedit/plugins/file-browser/ui/"
        "bedit-file-browser-search-view.ui"
    );

    gtk_widget_class_bind_template_child(
        widget_class, BeditFileBrowserSearchView, tree_view
    );
}

static void bedit_file_browser_search_view_class_finalize(
    BeditFileBrowserSearchViewClass *klass
) {}



static void bedit_file_browser_search_view_init(
    BeditFileBrowserSearchView *view
) {
    view->cancellable = NULL;

    gtk_widget_init_template(GTK_WIDGET(view));

    view->list_store = gtk_list_store_new(
        N_COLUMNS,
        G_TYPE_STRING,  // Icon name.
        G_TYPE_STRING,  // Markup.
        G_TYPE_FILE  // Location.
    );

    gtk_list_store_insert_with_values(
        view->list_store, NULL, -1,
        COLUMN_ICON_NAME, g_strdup("folder-symbolic"),
        COLUMN_MARKUP, g_strdup("Hello, World!"),
        COLUMN_LOCATION, NULL,
        -1
    );
    gtk_list_store_insert_with_values(
        view->list_store, NULL, -1,
        COLUMN_ICON_NAME, g_strdup("folder-symbolic"),
        COLUMN_MARKUP, g_strdup("Hello, World!"),
        COLUMN_LOCATION, NULL,
        -1
    );

    gtk_tree_view_set_model(view->tree_view, GTK_TREE_MODEL(view->list_store));

}

/**
 * bedit_file_browser_search_view_new:
 *
 * Creates a new #BeditFileBrowserSearchView.
 *
 * Return value: the new #BeditFileBrowserSearchView object
 **/
BeditFileBrowserSearchView *bedit_file_browser_search_view_new(void) {
    return g_object_new(BEDIT_TYPE_FILE_BROWSER_SEARCH_VIEW, NULL);
}

void bedit_file_browser_search_view_set_virtual_root(
    BeditFileBrowserSearchView *view, GFile *virtual_root
) {
    gboolean updated = FALSE;

    g_return_if_fail(BEDIT_IS_FILE_BROWSER_SEARCH_VIEW(view));
    g_return_if_fail(G_IS_FILE(virtual_root));

    if (view->virtual_root != NULL) {
        updated = !g_file_equal(virtual_root, view->virtual_root);
        g_object_unref(view->virtual_root);
    } else {
        updated = virtual_root != NULL;
    }

    view->virtual_root = virtual_root;

    if (updated) {
        g_object_notify(G_OBJECT(view), "virtual-root");
        bedit_file_browser_search_view_refresh(view);
    }
}

GFile *bedit_file_browser_search_view_get_virtual_root(
    BeditFileBrowserSearchView *view
) {
    g_return_val_if_fail(BEDIT_IS_FILE_BROWSER_SEARCH_VIEW(view), NULL);

    if (view->virtual_root != NULL) {
        g_object_ref(view->virtual_root);
    }

    return view->virtual_root;
}

void bedit_file_browser_search_view_set_query(
    BeditFileBrowserSearchView *view, const gchar *query
) {
    g_return_if_fail(BEDIT_IS_FILE_BROWSER_SEARCH_VIEW(view));

    bedit_debug_message(DEBUG_PLUGINS, "query: %s", query);

    if (g_strcmp0(query, view->query)) {
        bedit_debug(DEBUG_PLUGINS);
        view->query = g_strdup(query);
        g_object_notify(G_OBJECT(view), "query");
        bedit_file_browser_search_view_refresh(view);
    }
}

gchar *bedit_file_browser_search_view_get_query(
    BeditFileBrowserSearchView *view
) {
    g_return_val_if_fail(BEDIT_IS_FILE_BROWSER_SEARCH_VIEW(view), NULL);

    return view->query;
}

void bedit_file_browser_search_view_set_enabled(
    BeditFileBrowserSearchView *view, gboolean enabled
) {
    g_return_if_fail(BEDIT_IS_FILE_BROWSER_SEARCH_VIEW(view));

    if (view->enabled != enabled) {
        view->enabled = enabled;
        g_object_notify(G_OBJECT(view), "enabled");
        if (enabled) {
            bedit_file_browser_search_view_refresh(view);
        } else if (view->cancellable != NULL) {
            g_cancellable_cancel(view->cancellable);
            g_clear_object(&view->cancellable);
        }
    }
}

gboolean bedit_file_browser_search_view_get_enabled(
    BeditFileBrowserSearchView *view
) {
    return view->enabled ? TRUE : FALSE;
}














typedef struct _Search Search;

struct _Search {
    gchar *query;

    GtkListStore *matches;

    gchar *query_segment;

    // A stack of GFile objects describing the directories.
    GSList *file_stack;

    // A stack of pointers into one of the `GList`s of `GFileInfo` objects in
    // `dir_cache`.
    GSList *cursor_stack;


    // A map from directories to lists of `GFileInfo` objects representing their
    // children.  `dir_cache` is reused between searches.  Keys and values are
    // both owned.
    GHashTable *dir_cache;

    GCancellable *cancellable;
    BeditFileBrowserSearchView *view;
};


static void bedit_file_browser_search_free(Search *search);
static gboolean bedit_file_browser_search_match_segment(
    gchar const *segment, gchar const *name
);
static gboolean bedit_file_browser_search_is_last_segment(gchar const *segment);
static void bedit_file_browser_search_push(
    Search *search, GFile *file, GList *children
);
static void bedit_file_browser_search_pop(Search *search);
static void bedit_file_browser_search_iterate(Search *search);
static void bedit_file_browser_search_reload_top_next_cb(
    GFileEnumerator *enumerator, GAsyncResult *result,
    Search *search
);
static void bedit_file_browser_search_reload_top_begin_cb(
    GFile *file, GAsyncResult *result, Search *search
);
void bedit_file_browser_search_reload_top(Search *search);


static gboolean bedit_file_browser_search_match_segment(
    gchar const *segment, gchar const *name
) {
    // TODO
    return TRUE;
}

static gboolean bedit_file_browser_search_is_last_segment(
    gchar const *segment
) {
    while (*segment != '/' && *segment != '\0') {
        segment++;
    }
    return *segment == '\0';
}

static void bedit_file_browser_search_free(Search *search) {
    bedit_debug(DEBUG_PLUGINS);

    g_free(search->query);
    g_clear_object(&search->matches);

    g_slist_free_full(search->file_stack, g_object_unref);
    g_slist_free(search->cursor_stack);

    g_hash_table_unref(search->dir_cache);
    search->dir_cache = NULL;

    g_clear_object(&search->cancellable);

    search->view = NULL;

    g_free(search);
};

static void bedit_file_browser_search_push(
    Search *search, GFile *file, GList *children
) {
    bedit_debug(DEBUG_PLUGINS);

    g_return_if_fail(
        !bedit_file_browser_search_is_last_segment(search->query_segment)
    );

    search->file_stack = g_slist_prepend(search->file_stack, g_object_ref(file));
    search->cursor_stack = g_slist_prepend(search->cursor_stack, children);

    while (*search->query_segment != '/') {
        search->query_segment++;
    }
}

static void bedit_file_browser_search_pop(Search *search) {
    GSList *file_stack_head;
    GSList *cursor_stack_head;

    bedit_debug(DEBUG_PLUGINS);

    file_stack_head = search->file_stack;
    g_return_if_fail(file_stack_head != NULL);
    search->file_stack = file_stack_head->next;
    g_clear_object(&file_stack_head->data);
    file_stack_head->next = NULL;
    g_slist_free_1(file_stack_head);

    cursor_stack_head = search->cursor_stack;
    g_return_if_fail(cursor_stack_head != NULL);
    search->cursor_stack = cursor_stack_head->next;
    cursor_stack_head->data = NULL;
    cursor_stack_head->next = NULL;
    g_slist_free_1(cursor_stack_head);

    while (search->query_segment > search->query && *search->query_segment != '/') {
        search->query_segment--;
    }
}

static void bedit_file_browser_search_iterate(Search *search) {
    bedit_debug(DEBUG_PLUGINS);

    if (g_cancellable_is_cancelled(search->cancellable)) {
        bedit_file_browser_search_free(search);
        return;
    }

    while (search->cursor_stack != NULL) {
        GFileInfo *fileinfo;
        GList *cursor;

        bedit_debug_message(DEBUG_PLUGINS, "iter");

        if (search->cursor_stack->data == NULL) {
            bedit_file_browser_search_pop(search);
            continue;
        }

        // Advance the cursor.  The list is owned by the directory cache, so no
        // need to free anything.
        cursor = (GList *) search->cursor_stack->data;
        search->cursor_stack->data = cursor->next;

        fileinfo = G_FILE_INFO(cursor->data);
        g_return_if_fail(G_IS_FILE_INFO(fileinfo));

        if (bedit_file_browser_search_is_last_segment(search->query_segment)) {
            gchar const *name;

            name = g_file_info_get_name(fileinfo);
            bedit_debug_message(DEBUG_PLUGINS, "last: %s", name);

            if (bedit_file_browser_search_match_segment(
                search->query_segment, name
            )) {
                GFile *file;

                file = g_file_get_child(search->file_stack->data, name);

                gtk_list_store_insert_with_values(
                    search->matches, NULL, -1,
                    COLUMN_ICON_NAME, g_strdup("folder-symbolic"),  // TODO g_file_info_get_icon
                    COLUMN_MARKUP, g_file_info_get_display_name(fileinfo),
                    COLUMN_LOCATION, file,
                    -1
                );

                g_object_unref(file);
            }
        } else {
            gchar const *name;

            if (g_file_info_get_file_type(fileinfo) != G_FILE_TYPE_DIRECTORY) {
                continue;
            }

            name = g_file_info_get_name(fileinfo);

            bedit_debug_message(DEBUG_PLUGINS, "rest: %s", name);

            if (bedit_file_browser_search_match_segment(
                search->query_segment, name
            )) {
                GFile *file = g_file_get_child(search->file_stack->data, name);

                bedit_file_browser_search_push(
                    search, file, g_hash_table_lookup(search->dir_cache, file)
                );

                if (!g_hash_table_contains(search->dir_cache, file))  {
                    bedit_file_browser_search_reload_top(search);
                    g_object_unref(file);
                    // `bedit_file_browser_reload_top` will eventually call back
                    // into this function in a different iteration of the glib
                    // main loop, and we can continue.
                    return;
                }
                g_object_unref(file);
            }
        }
    }

    gtk_tree_view_set_model(
        search->view->tree_view, GTK_TREE_MODEL(search->matches)
    );

    bedit_debug_message(DEBUG_PLUGINS, "DONE!");

    bedit_file_browser_search_free(search);
}

gint bedit_file_browser_search_compare_fileinfo(
    gconstpointer a, gconstpointer b
) {
    GFileInfo *fileinfo_a;
    GFileInfo *fileinfo_b;

    fileinfo_a = G_FILE_INFO(a);
    fileinfo_b = G_FILE_INFO(b);

    return g_utf8_collate(
        g_file_info_get_name(fileinfo_a),
        g_file_info_get_name(fileinfo_b)
    );
}

static void bedit_file_browser_search_reload_top_next_cb(
    GFileEnumerator *enumerator, GAsyncResult *result,
    Search *search
) {
    GError *error = NULL;
    GFile *root;
    GList *new_files;
    GList *existing_files;

    bedit_debug(DEBUG_PLUGINS);

    g_return_if_fail(G_IS_FILE_ENUMERATOR(enumerator));

    root = g_file_enumerator_get_container(enumerator);
    g_return_if_fail(g_file_equal(root, search->file_stack->data));

    new_files = g_file_enumerator_next_files_finish(
        enumerator, result, &error
    );
    existing_files = search->cursor_stack->data;

    if (error != NULL) {
        bedit_debug_message(DEBUG_PLUGINS, "ERROR");
        // TODO log error
        g_error_free(error);
        g_object_unref(enumerator);

        g_list_free_full(new_files, g_object_unref);
        g_list_free_full(existing_files, g_object_unref);

        bedit_file_browser_search_pop(search);
        bedit_file_browser_search_iterate(search);

        return;
    }

    if (new_files != NULL) {
        // We haven't finished iterating.  Store the new children at the
        // beginning of the scratch list (only O(n), not O(n^2) to build the
        // full list) and fetch the next batch.
        search->cursor_stack->data = g_list_concat(new_files, existing_files);

        g_file_enumerator_next_files_async(
            enumerator, DIRECTORY_LOAD_ITEMS_PER_CALLBACK, G_PRIORITY_DEFAULT,
            search->cancellable,
            (GAsyncReadyCallback)bedit_file_browser_search_reload_top_next_cb,
            search
        );

        return;
    }

    search->cursor_stack->data = g_list_sort(
        existing_files, bedit_file_browser_search_compare_fileinfo
    );
    g_hash_table_replace(
        search->dir_cache, g_object_ref(root), search->cursor_stack->data
    );

    g_object_unref(enumerator);

    bedit_file_browser_search_iterate(search);
}

static void bedit_file_browser_search_reload_top_begin_cb(
    GFile *file, GAsyncResult *result, Search *search
) {
    GError *error = NULL;
    GFileEnumerator *enumerator;

    bedit_debug(DEBUG_PLUGINS);

    g_return_if_fail(G_IS_FILE(file));

    enumerator = g_file_enumerate_children_finish(file, result, &error);

    if (error != NULL) {
        // TODO log error.
        g_error_free(error);

        bedit_file_browser_search_pop(search);
        bedit_file_browser_search_iterate(search);

        return;
    }

    g_return_if_fail(G_IS_FILE_ENUMERATOR(enumerator));

    g_file_enumerator_next_files_async(
        enumerator, DIRECTORY_LOAD_ITEMS_PER_CALLBACK, G_PRIORITY_DEFAULT,
        search->cancellable,
        (GAsyncReadyCallback)bedit_file_browser_search_reload_top_next_cb,
        search
    );
}

void bedit_file_browser_search_reload_top(Search *search) {
    bedit_debug(DEBUG_PLUGINS);

    g_return_if_fail(search->file_stack != NULL);
    g_return_if_fail(search->cursor_stack != NULL);

    search->cursor_stack->data = NULL;

    g_file_enumerate_children_async(
        search->file_stack->data,
        STANDARD_ATTRIBUTE_TYPES, G_FILE_QUERY_INFO_NONE,
        G_PRIORITY_DEFAULT, search->cancellable,
        (GAsyncReadyCallback)bedit_file_browser_search_reload_top_begin_cb,
        search
    );
}


static void bedit_file_browser_search_view_free_fileinfo_list(GList *files) {
    g_list_free_full(files, g_object_unref);
}

static void bedit_file_browser_search_view_refresh(
    BeditFileBrowserSearchView *view
) {
    Search *search;

    g_return_if_fail(BEDIT_IS_FILE_BROWSER_SEARCH_VIEW(view));

    if (!bedit_file_browser_search_view_get_enabled(view)) {
        bedit_debug_message(DEBUG_PLUGINS, "refresh skipped");
        return;
    }

    if (bedit_file_browser_search_view_get_query(view) == NULL) {
        bedit_debug_message(DEBUG_PLUGINS, "refresh skipped");
        return;
    }

    if (bedit_file_browser_search_view_get_virtual_root(view) == NULL) {
        bedit_debug_message(DEBUG_PLUGINS, "refresh skipped");
        return;
    }

    bedit_debug_message(DEBUG_PLUGINS, "refresh");

    // Cancel previous search.
    if (view->cancellable != NULL) {
        g_cancellable_cancel(view->cancellable);
        g_clear_object(&view->cancellable);
    }
    view->cancellable = g_cancellable_new();

    // Create the cache, if it doesn't already exist.
    if (view->dir_cache == NULL) {
        view->dir_cache = g_hash_table_new_full(
            g_file_hash, (GEqualFunc) g_file_equal,
            g_object_unref, (GDestroyNotify) bedit_file_browser_search_view_free_fileinfo_list
        );
    }

    search = g_new(Search, 1);
    g_return_if_fail(search != NULL);

    search->query = g_strdup(view->query);
    search->query_segment = search->query;

    // TODO this is also created on startup.
    search->matches = gtk_list_store_new(
        N_COLUMNS,
        G_TYPE_STRING,  // Icon name.
        G_TYPE_STRING,  // Markup.
        G_TYPE_FILE  // Location.
    );
    g_return_if_fail(GTK_IS_LIST_STORE(search->matches));

    search->file_stack = NULL;
    search->cursor_stack = NULL;

    search->dir_cache = g_hash_table_ref(view->dir_cache);

    search->cancellable = g_object_ref(view->cancellable);
    search->view = view;

    // We should use push, but we don't want to advance the query segment,
    search->file_stack = g_slist_prepend(
        search->file_stack,
        g_object_ref(view->virtual_root)
    );
    search->cursor_stack = g_slist_prepend(
        search->cursor_stack,
        g_hash_table_lookup(search->dir_cache, view->virtual_root)
    );

    if (!g_hash_table_contains(search->dir_cache, view->virtual_root))  {
        bedit_file_browser_search_reload_top(search);
    } else {
        g_return_if_fail(g_hash_table_lookup(search->dir_cache, view->virtual_root) != NULL);
        g_return_if_fail(search->cursor_stack->data != NULL);
        bedit_file_browser_search_iterate(search);
    }
}

void _bedit_file_browser_search_view_register_type(GTypeModule *type_module) {
    bedit_file_browser_search_view_register_type(type_module);
}
