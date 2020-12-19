#include "config.h"

#include "bedit-file-browser-search-view.h"

#include <gmodule.h>
#include <gtk/gtk.h>


typedef struct _Search Search;

struct _Search {
    gchar *pattern;
    GSequence *matches;

    GHashTable *dir_cache;

    guint pending_dirs;
    GCancellable *cancellable;
};

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







void bedit_file_browser_search(
    GFile *root, gchar const *query,
    GHashMap *dir_cache,
    GCancellable *cancellable,
    GCallback *callback,
    gpointer argument,
) {


}


bedit_file_browser_search_unref(BeditFileBrowserSearch)







static void bedit_file_browser_dir_next_load_cb(
    GFileEnumerator *enumerator, GAsyncResult *result,
    Search *search
) {
    BeditFileBrowserDir *dir;
    GError *error = NULL;
    GList *files;

    g_return_if_fail(G_IS_FILE_ENUMERATOR(enumerator));

    files = g_file_enumerator_next_files_finish(
        enumerator, result, &error
    );

    if (g_cancellable_is_cancelled(search->cancellable)) {
        g_object_unref(enumerator);
        g_list_free(files);
        if (error != NULL) g_error_free(error);
        search_unref(search);

        return;
    }
    // Note that search does not hold a strong reference to the directory, so
    // it is not safe to dereference this pointer until after the cancellable
    // has been checked.
    dir = context->dir;
    g_return_if_fail(BEDIT_IS_FILE_BROWSER_DIR(dir));

    if (error != NULL) {
        // TODO
        // bedit_file_browser_dir_set_error(dir, error);
        // bedit_file_browser_dir_unload(dir);

        g_error_free(error);
        g_object_unref(enumerator);

        check_finished(search);

        return;
    }

    if (files == NULL) {
        // We've reached the end of the enumerator and can close everything up.
        // TODO mark as finished.
        g_object_unref(enumerator);
        check_finished(search);

        return;
    }

    for (GList *cursor = files; cursor != NULL; cursor = cursor->next) {
        GFile *file;
        GFileInfo *info;
        gchar const *name;

        info = cursor->data;
        name = g_file_info_get_name(info);
        file = g_file_get_child(dir->file, name);

        bedit_file_browser_dir_add_child(dir, file);
    }

    g_list_free(files);

    g_file_enumerator_next_files_async(
        enumerator, DIRECTORY_LOAD_ITEMS_PER_CALLBACK, G_PRIORITY_DEFAULT,
        context->cancellable,
        (GAsyncReadyCallback)bedit_file_browser_dir_next_load_cb,
        context
    );
}

static void bedit_file_browser_dir_begin_load_cb(
    GFile *file, GAsyncResult *result, Search *context
) {
    BeditFileBrowserDir *dir;
    GError *error = NULL;
    GFileEnumerator *enumerator;

    g_return_if_fail(G_IS_FILE(file));

    enumerator = g_file_enumerate_children_finish(file, result, &error);

    if (g_cancellable_is_cancelled(context->cancellable)) {
        if (enumerator != NULL) g_object_unref(enumerator);
        if (error) g_error_free(error);

        return;
    }
    // Note that context does not hold a strong reference to the directory, so
    // it is not safe to dereference this pointer until after the cancellable
    // has been checked.
    dir = context->dir;
    g_return_if_fail(BEDIT_IS_FILE_BROWSER_DIR(dir));

    if (error != NULL) {
        // TODO
        // bedit_file_browser_dir_set_error(dir, error);
        // bedit_file_browser_dir_unload(dir);

        g_error_free(error);
        async_context_unref(context);

        return;
    }

    g_return_if_fail(G_IS_FILE_ENUMERATOR(enumerator));

    g_file_enumerator_next_files_async(
        enumerator, DIRECTORY_LOAD_ITEMS_PER_CALLBACK, G_PRIORITY_DEFAULT,
        context->cancellable,
        (GAsyncReadyCallback)bedit_file_browser_dir_next_load_cb,
        context
    );
}

void bedit_file_browser_dir_refresh(BeditFileBrowserDir *dir) {
    g_return_if_fail(BEDIT_IS_FILE_BROWSER_DIR(dir));

    if (dir->context != NULL) {
        g_cancellable_cancel(dir->context->cancellable);
        async_context_unref(dir->context);
    }
    dir->context = async_context_new(dir);

    // Set loading to true.

    // Free the children.
    while (TRUE) {
        GHashTableIter child_iter;
        GFile *child;

        g_hash_table_iter_init(&child_iter, dir->children);
        if (!g_hash_table_iter_next(&child_iter, (gpointer) &child, NULL)) {
            break;
        }

        bedit_file_browser_dir_remove_child(dir, child);
    }

    g_file_enumerate_children_async(
        dir->file, STANDARD_ATTRIBUTE_TYPES, G_FILE_QUERY_INFO_NONE,
        G_PRIORITY_DEFAULT, dir->context->cancellable,
        (GAsyncReadyCallback)bedit_file_browser_dir_begin_load_cb,
        async_context_ref(dir->context)
    );
}


































static void bedit_file_browser_search_view_refresh(
    BeditFileBrowserSearchView *view
) {
    g_return_if_fail(BEDIT_IS_FILE_BROWSER_SEARCH_VIEW(view));

    if (!bedit_file_browser_search_view_get_enabled(view)) {
        return;
    }

    if (view->cancellable != NULL) {
        g_cancellable_cancel(view->cancellable);
        g_clear_object(&view->cancellable);
    }
    view->cancellable = g_cancellable_new();

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
        g_weak_ref(view)
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
