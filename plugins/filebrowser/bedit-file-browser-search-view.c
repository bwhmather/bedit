/*
 * bedit-file-browser-search-view.c
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
} BeditFileBrowserSearchStoreColumn;

static void bedit_file_browser_search_view_activate_selected(
    BeditFileBrowserSearchView *view
);

static void bedit_file_browser_search_view_on_row_activated(
    GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column,
    BeditFileBrowserSearchView *view
);

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

    g_signal_connect(
        view->tree_view, "row-activated",
        G_CALLBACK(bedit_file_browser_search_view_on_row_activated), view
    );
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

    view->virtual_root = g_object_ref(virtual_root);

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

static void bedit_file_browser_search_view_activate_selected(
    BeditFileBrowserSearchView *view
) {
    GtkTreeModel *model;
    GtkTreeSelection *selection;
    GList *rows;
    GFile *directory = NULL;

    g_return_if_fail(BEDIT_IS_FILE_BROWSER_SEARCH_VIEW(view));

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

static void bedit_file_browser_search_view_on_row_activated(
    GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column,
    BeditFileBrowserSearchView *view
) {
    GtkTreeSelection *selection;

    g_return_if_fail(GTK_IS_TREE_VIEW(tree_view));
    g_return_if_fail(BEDIT_IS_FILE_BROWSER_SEARCH_VIEW(view));

    /* Make sure the activated row is the only one selected */
    selection = gtk_tree_view_get_selection(tree_view);
    gtk_tree_selection_unselect_all(selection);
    gtk_tree_selection_select_path(selection, path);

    bedit_file_browser_search_view_activate_selected(view);
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

    GSList *markup_stack;

    // Temporary buffer used to build markup strings representing filenames.
    GString *markup_buffer;

    // A map from directories to lists of `GFileInfo` objects representing their
    // children.  `dir_cache` is reused between searches.  Keys and values are
    // both owned.
    GHashTable *dir_cache;

    GCancellable *cancellable;
    BeditFileBrowserSearchView *view;
};


static void bedit_file_browser_search_free(Search *search);
static gboolean bedit_file_browser_search_match_segment(
    gchar const *segment, gchar const *name, GString *markup
);
static gboolean bedit_file_browser_search_is_last_segment(gchar const *segment);
static void bedit_file_browser_search_push(
    Search *search, GFile *file, GList *children, gchar *markup
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

static void bedit_file_browser_search_append_escaped(
    GString *str, gunichar chr
) {
    // Stolen from `g_markup_escape_text`, with some minor reformatting.
    switch (chr) {
    case '&':
        g_string_append(str, "&amp;");
        break;

    case '<':
        g_string_append(str, "&lt;");
        break;

    case '>':
        g_string_append(str, "&gt;");
        break;

    case '\'':
        g_string_append(str, "&apos;");
        break;

    case '"':
        g_string_append(str, "&quot;");
        break;

    default:
        if (
            (0x1 <= chr && chr <= 0x8) ||
            (0xb <= chr && chr  <= 0xc) ||
            (0xe <= chr && chr <= 0x1f) ||
            (0x7f <= chr && chr <= 0x84) ||
            (0x86 <= chr && chr <= 0x9f)
        ) {
            g_string_append_printf(str, "&#x%x;", chr);
        } else {
            g_string_append_unichar(str, chr);
        }
        break;
    }
}

static gboolean bedit_file_browser_search_match_segment(
    gchar const *segment, gchar const *name, GString *markup
) {
    gboolean bold = FALSE;
    gchar const *query_cursor;
    gchar const *name_cursor;
    gchar const *chunk_start;

    g_string_truncate(markup, 0);

    query_cursor = segment;
    name_cursor = name;
    chunk_start = name;

    while (*chunk_start != '\0') {
        gchar const *chunk_end;
        gchar const *candidate_start;
        gchar const *best_start;
        size_t best_length;

        // Find end of chunk.
        chunk_end = g_utf8_find_next_char(chunk_start, NULL);
        while (TRUE) {
            gunichar start_char = g_utf8_get_char(chunk_start);
            gunichar end_char = g_utf8_get_char(chunk_end);

            if (end_char == '\0') {
                break;
            }

            if (g_unichar_isalpha(start_char)) {
                // Chunk is made up of an upper or lower case letter followed by
                // any number of lower case letters.
                if (!g_unichar_islower(end_char)) {
                    break;
                }
            } else if (g_unichar_isdigit(start_char)) {
                // Chunk is a sequence of digits
                if (!g_unichar_isdigit(end_char)) {
                    break;
                }
            } else {
                // Chunk is a single, non-alphanumeric character.
                break;
            }

            chunk_end = g_utf8_find_next_char(chunk_end, NULL);
        }

        g_return_val_if_fail(chunk_end > chunk_start, FALSE);  // TODO debugging only.

        // Find longest match.
        best_start = chunk_start;
        best_length = 0;

        candidate_start = chunk_start;
        while (candidate_start < chunk_end) {
            gchar const *candidate_name_cursor;
            gchar const *candidate_query_cursor;
            gint candidate_length;

            candidate_name_cursor = candidate_start;
            candidate_query_cursor = query_cursor;
            candidate_length = 0;

            while (candidate_start < chunk_end) {
                gunichar name_char = g_utf8_get_char(candidate_name_cursor);
                gunichar query_char = g_utf8_get_char(candidate_query_cursor);

                if (query_char == '\0' || query_char == '/') {
                    break;
                }

                if (
                    g_unichar_tolower(name_char) !=
                    g_unichar_tolower(query_char)
                ) {
                    break;
                }

                candidate_name_cursor = g_utf8_find_next_char(
                    candidate_name_cursor, NULL
                );
                candidate_query_cursor = g_utf8_find_next_char(
                    candidate_query_cursor, NULL
                );
                candidate_length++;
            }

            if (candidate_length > best_length) {
                best_start = candidate_start;
                best_length = candidate_length;
            }

            candidate_start = g_utf8_find_next_char(candidate_start, NULL);
        }

        while (name_cursor < chunk_end) {
            if (
                name_cursor >= best_start &&
                name_cursor < (best_start + best_length)
            ) {
                if (!bold) {
                    g_string_append(markup, "<b>");
                    bold = TRUE;
                }
            } else {
                if (bold) {
                    g_string_append(markup, "</b>");
                    bold = FALSE;
                }
            }

            bedit_file_browser_search_append_escaped(
                markup, g_utf8_get_char(name_cursor)
            );

            name_cursor = g_utf8_find_next_char(name_cursor, NULL);
        }

        query_cursor += best_length;
        chunk_start = chunk_end;
    }

    if (bold) {
        g_string_append(markup, "</b>");
    }

    if (
        g_utf8_get_char(query_cursor) == '\0' ||
        g_utf8_get_char(query_cursor) == '/'
    ) {
        return TRUE;
    } else {
        return FALSE;
    }
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

    g_string_free(search->markup_buffer, TRUE);

    g_hash_table_unref(search->dir_cache);
    search->dir_cache = NULL;

    g_clear_object(&search->cancellable);

    search->view = NULL;

    g_free(search);
};

static void bedit_file_browser_search_push(
    Search *search, GFile *file, GList *children, gchar *markup
) {
    bedit_debug(DEBUG_PLUGINS);

    g_return_if_fail(
        !bedit_file_browser_search_is_last_segment(search->query_segment)
    );

    search->file_stack = g_slist_prepend(search->file_stack, g_object_ref(file));
    search->cursor_stack = g_slist_prepend(search->cursor_stack, children);

    if (search->markup_stack) {
        markup = g_strdup_printf(
            "%s/%s",
            (gchar *) search->markup_stack->data,
            markup
        );
    } else {
        markup = g_strdup(markup);
    }
    search->markup_stack = g_slist_prepend(search->markup_stack, markup);

    while (*search->query_segment != '/') {
        search->query_segment++;
    }
    search->query_segment++;
}

static void bedit_file_browser_search_pop(Search *search) {
    GSList *file_stack_head;
    GSList *cursor_stack_head;
    GSList *markup_stack_head;

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

    markup_stack_head = search->markup_stack;
    if (markup_stack_head != NULL) {
        search->markup_stack = markup_stack_head->next;
        g_free(markup_stack_head->data);
        markup_stack_head->data = NULL;
        markup_stack_head->next = NULL;
        g_slist_free_1(markup_stack_head);
    }

    search->query_segment--;
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
                search->query_segment, name, search->markup_buffer
            )) {
                GFile *file;
                gchar *markup;

                file = g_file_get_child(search->file_stack->data, name);
                if (search->markup_stack != NULL) {
                    markup = g_strdup_printf(
                        "%s/%s",
                        (gchar *) search->markup_stack->data,
                        search->markup_buffer->str
                    );
                } else {
                    markup = search->markup_buffer->str;
                }

                gtk_list_store_insert_with_values(
                    search->matches, NULL, -1,
                    COLUMN_ICON, g_file_info_get_symbolic_icon(fileinfo),
                    COLUMN_MARKUP, markup,
                    COLUMN_LOCATION, file,
                    COLUMN_FILE_INFO, fileinfo,
                    -1
                );

                if (search->markup_stack != NULL) {
                    g_free(markup);
                }
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
                search->query_segment, name, search->markup_buffer
            )) {
                GFile *file = g_file_get_child(search->file_stack->data, name);

                bedit_file_browser_search_push(
                    search, file,
                    g_hash_table_lookup(search->dir_cache, file),
                    search->markup_buffer->str
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

    search->matches = gtk_list_store_new(
        N_COLUMNS,
        G_TYPE_ICON,  // Icon name.
        G_TYPE_STRING,  // Markup.
        G_TYPE_FILE,  // Location.
        G_TYPE_FILE_INFO  // Metadata.
    );
    g_return_if_fail(GTK_IS_LIST_STORE(search->matches));

    search->file_stack = NULL;
    search->cursor_stack = NULL;
    search->markup_stack = NULL;

    search->markup_buffer = g_string_new(NULL);

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
