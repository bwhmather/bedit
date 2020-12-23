#include "config.h"

#include "bedit-file-browser-search-view.h"

#include <gmodule.h>
#include <gtk/gtk.h>


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
    BeditFileBrowserSearchView *widget, GFile *virtual_root
) {
    gboolean updated = FALSE;

    g_return_if_fail(BEDIT_IS_FILE_BROWSER_SEARCH_VIEW(widget));
    g_return_if_fail(G_IS_FILE(virtual_root));

    if (widget->virtual_root != NULL) {
        updated = !g_file_equal(virtual_root, widget->virtual_root);
        g_object_unref(widget->virtual_root);
    } else {
        updated = virtual_root != NULL;
    }

    widget->virtual_root = virtual_root;

    if (updated) {
        g_object_notify(G_OBJECT(widget), "virtual-root");
    }
}

GFile *bedit_file_browser_search_view_get_virtual_root(
    BeditFileBrowserSearchView *widget
) {
    g_return_val_if_fail(BEDIT_IS_FILE_BROWSER_SEARCH_VIEW(widget), NULL);

    if (widget->virtual_root != NULL) {
        g_object_ref(widget->virtual_root);
    }

    return widget->virtual_root;
}

void bedit_file_browser_search_view_set_query(
    BeditFileBrowserSearchView *view, const gchar *query
) {
    g_return_if_fail(BEDIT_IS_FILE_BROWSER_SEARCH_VIEW(view));

    if (!g_strcmp0(query, view->query)) {
        view->query = g_strdup(query);
        g_object_notify(G_OBJECT(view), "query");
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
    }
}

gboolean bedit_file_browser_search_view_get_enabled(
    BeditFileBrowserSearchView *view
) {
    return view->enabled ? TRUE : FALSE;
}



















typedef struct _Search Search;

struct _Search {
    GFile *root;
    gchar *pattern;

    GSequence *matches;

    GCancellable *cancellable;

    // A map from directories to their maximum path depth relative to the root.
    // The root starts at depth zero.  If, as a result of following `..` path
    // segments, a directory is reached more than once, lower depths will be
    // replaced.
    GHashTable *dir_depth;

    // A map from directories to lists of `GFileInfo` objects representing their
    // children.  `dir_scratch` is used as a temporary store for directories
    // that are currently being reloaded.  Once loading has finished they will
    // be moved to `dir_cache`.  Keys and values are both owned, but values will
    // not be unref'ed when removed.
    GHashTable *dir_scratch;

    // A map from directories to lists of `GFileInfo` objects representing their
    // children.  `dir_cache` is reused between searches.  Keys and values are
    // both owned.
    GHashTable *dir_cache;

    // A map from `GFile`s to `GFileInfo` objects.  Both the files and the file
    // info objects are owned.  Will be populated for every traversed directory
    // and every file in `matches`.   Not shared between searches.
    GHashTable *fileinfo_cache;

    BeditFileBrowserSearchView *view;
};













void bedit_file_browser_search(
    GFile *root, gchar const *query,
    GHashMap *dir_cache,
    GCancellable *cancellable,
    GCallback *callback,
    gpointer argument,
) {

}


BeditFileBrowserSearchResult *bedit_file_browser_result_finish(


) {



}





bedit_file_browser_search_unref(BeditFileBrowserSearch)




static void process_dir_real(Search *search, GFile *root) {
    guint depth;
    GList *children;

    g_return_if_fail(g_hash_table_contains(search->dir_depth, root));
    g_return_if_fail(g_hash_table_contains(search->dir_cache, root));

    depth = GPOINTER_TO_INT(g_hash_table_get(search->dir_depth, root));
    children = g_hash_table_get(search->dir_cache, root);


    for (GList *cursor = children; cursor != NULL; cursor = cursor->next) {
        GFileInfo *child_info = cursor->value;

        if depth == len(pattern):
            if match(pattern, depth, child_info->name):
                matches.add(file(child_info));

        else:
            if is_dir(child_info) and match(pattern, depth, child_info->name):
                bedit_file_browser_search_process_dir();
    }
}


static void bedit_file_browser_search_try_finish(Search *search) {
    if (g_hash_table_size(search->dir_scratch)) {
        // There are still directories that need to be processed.
        return;
    }

    if (!g_cancellable_is_cancelled(search->cancellable)) {
        BeditFileBrowserSearchView *view = search->view;
        GtkListStore *list_store = view->list_store;


        // Sort matches.

        // Copy matches to list model.
        gtk_list_store_clear(list_store);
        for file in matches:
            parent = file->parent
            name = file->name

            fileinfo = search->dir_cache[parent][name]


        // Set list view to active.
    }

    // Free search object
}




static void bedit_file_browser_search_reload_dir_next_cb(
    GFileEnumerator *enumerator, GAsyncResult *result,
    Search *search
) {
    GError *error = NULL;
    GFile *root;
    GList *new_files;
    GList *existing_files;

    g_return_if_fail(G_IS_FILE_ENUMERATOR(enumerator));

    root = g_file_enumerator_get_container(enumerator);
    new_files = g_file_enumerator_next_files_finish(
        enumerator, result, &error
    );
    existing_files = g_hash_table_get(root);

    if (error != NULL) {
        // TODO
        // bedit_file_browser_dir_set_error(dir, error);
        // bedit_file_browser_dir_unload(dir);

        g_list_free(new_files);
        g_list_free(existing_files);

        g_hash_map_remove(search->dir_scratch, root);

        g_error_free(error);
        g_object_unref(enumerator);

        bedit_file_browser_search_try_finish(search);

        return;
    }

    if (files != NULL) {
        // We haven't finished iterating.  Store the new children at the
        // beginning of the scratch list (only O(n), not O(n^2) to build the
        // full list) and fetch the next batch.
        g_hash_table_insert(
            search->dir_scratch,
            root,
            g_list_concat(new_files, existing_files)
        );

        g_file_enumerator_next_files_async(
            enumerator, DIRECTORY_LOAD_ITEMS_PER_CALLBACK, G_PRIORITY_DEFAULT,
            search->cancellable,
            (GAsyncReadyCallback)bedit_file_browser_search_reload_dir_next_cb,
            search
        );

        return;
    }

    g_hash_map_replace(
        search->dir_cache, g_object_ref(root), existing_files
    );
    g_hash_map_remove(search->dir_scratch, root);

    process_dir_real(root);

    g_object_unref(enumerator);

    bedit_file_browser_search_try_finish(search);
}

static void bedit_file_browser_search_reload_dir_begin_cb(
    GFile *file, GAsyncResult *result, Search *search
) {
    BeditFileBrowserDir *dir;
    GError *error = NULL;
    GFileEnumerator *enumerator;

    g_return_if_fail(G_IS_FILE(file));

    enumerator = g_file_enumerate_children_finish(file, result, &error);

    if (g_cancellable_is_cancelled(context->cancellable)) {
        if (enumerator != NULL) g_object_unref(enumerator);
        if (error) g_error_free(error);
        g_hash_map_remove(search->dir_scratch, file);
        bedit_file_browser_search_try_finish(search);

        return;
    }

    if (error != NULL) {
        // TODO
        // bedit_file_browser_dir_set_error(dir, error);
        // bedit_file_browser_dir_unload(dir);

        g_error_free(error);
        async_context_unref(context);
        g_hash_map_remove(search->dir_scratch, file);
        bedit_file_browser_search_try_finish(search);

        return;
    }

    g_return_if_fail(G_IS_FILE_ENUMERATOR(enumerator));

    g_file_enumerator_next_files_async(
        enumerator, DIRECTORY_LOAD_ITEMS_PER_CALLBACK, G_PRIORITY_DEFAULT,
        search->cancellable,
        (GAsyncReadyCallback)bedit_file_browser_search_reload_dir_next_cb,
        search
    );
}


void bedit_file_browser_search_reload_dir(Search *search, GFile *file) {
    if (g_hash_table_contains(search->dir_scratch, file)) {
        // This can happen if there is a `..` segment in the path and the
        // directory is matched more than once.
        return;
    }

    g_hash_table_set(search->dir_scratch, g_object_ref(file), NULL);

    g_file_enumerate_children_async(
        file, STANDARD_ATTRIBUTE_TYPES, G_FILE_QUERY_INFO_NONE,
        G_PRIORITY_DEFAULT, search->cancellable,
        (GAsyncReadyCallback)dir_begin_reload_cb,
        search
    );
}


void bedit_file_browser_search_process_dir(Search *search, GFile *file) {
    if (g_hash_table_contains(search->dir_cache, file) {
        queue(process_dir_real)
    } else {
        bedit_file_browser_search_reload_dir(search, file);
    }
}






static void on_search_completed_cb() {



}



















static void bedit_file_browser_search_view_refresh(
    BeditFileBrowserSearchView *view
) {
    g_return_if_fail(BEDIT_IS_FILE_BROWSER_SEARCH_VIEW(view));

    if (!bedit_file_browser_search_view_get_enabled(view)) {
        return;
    }

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
            g_object_unref, g_object_unref
        );
    }

    do_search(
        view->root, view->query,
        dir_cache,
        cancellable,
        on_search_completed_cb,
        view
    );

    queue_process_dir(search, virtual_root);

}










class Dir:
public:
    property loaded


private:
    monitor
    cancellable



class DirCache:






Expanding a directory adds its contents to the internal list of refs.






def is_dir(GFile):
    pass

def is_hidden(GFile):
    pass

def is_binary(GFile):
    pass



info_cache: Dict[GFile, GFileInfo]
dir_cache: Dict[GFile, List[GFileInfo]]






def process_dir():
    pending -= 1
    for file in dir.files:
        if match(file):
            matches.add(file)

        if is_dir(file) and match_partial(file):
            queue_process_dir(file)

    if pending == 0:
        display()


def queue_process_dir():
    pending += 1
    if dir in dir_cache:
        process_dir(dir)
    else:
        list_dir(callback=process_dir)






pending: Set[GFile]
































void _bedit_file_browser_search_view_register_type(GTypeModule *type_module) {
    bedit_file_browser_search_view_register_type(type_module);
}
