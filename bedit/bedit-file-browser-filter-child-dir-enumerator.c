/*
 * bedit-file-browser-filter-child-dir-enumerator.c
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

#include "bedit-file-browser-filter-child-dir-enumerator.h"

#include <gio/gio.h>
#include <glib-object.h>
#include <gmodule.h>

#include "bedit-debug.h"
#include "bedit-file-browser-filter-dir-enumerator.h"
#include "bedit-file-browser-filter-match.h"

#define STANDARD_ATTRIBUTE_TYPES                                            \
    G_FILE_ATTRIBUTE_STANDARD_TYPE ","                                      \
    G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN ","                                 \
    G_FILE_ATTRIBUTE_STANDARD_NAME

struct _BeditFileBrowserFilterChildDirEnumerator {
    GObject parent_instance;

    BeditFileBrowserFilterDirEnumerator *source;
    gchar *pattern;

    /* Sorted queue of GFile instances that match the pattern, each a child of
     * the last directory yielded by the source. */
    GList *matches;
};

enum {
    PROP_0,
    PROP_SOURCE,
    PROP_PATTERN,
    LAST_PROP,
};

static void bedit_file_browser_filter_child_dir_enumerator_iface_init(
    BeditFileBrowserFilterDirEnumeratorInterface *interface
);
static gboolean bedit_file_browser_filter_child_dir_enumerator_iterate(
    BeditFileBrowserFilterDirEnumerator *enumerator,
    GFile **dir,
    gchar **path_markup,
    GCancellable *cancellable,
    GError **error
);

G_DEFINE_TYPE_EXTENDED(
    BeditFileBrowserFilterChildDirEnumerator,
    bedit_file_browser_filter_child_dir_enumerator,
    G_TYPE_OBJECT, 0,
    G_IMPLEMENT_INTERFACE(
        BEDIT_TYPE_FILE_BROWSER_FILTER_DIR_ENUMERATOR,
        bedit_file_browser_filter_child_dir_enumerator_iface_init
    )
)

static void bedit_file_browser_filter_child_dir_enumerator_get_property(
    GObject *object, guint prop_id, GValue *value, GParamSpec *pspec
) {
    BeditFileBrowserFilterChildDirEnumerator *enumerator;

    enumerator = BEDIT_FILE_BROWSER_FILTER_CHILD_DIR_ENUMERATOR(object);

    switch (prop_id) {
    case PROP_SOURCE:
        g_value_take_object(value, enumerator->source);
        break;

    case PROP_PATTERN:
        g_value_take_string(value, enumerator->pattern);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void bedit_file_browser_filter_child_dir_enumerator_set_property(
    GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec
) {
    BeditFileBrowserFilterChildDirEnumerator *enumerator;

    enumerator = BEDIT_FILE_BROWSER_FILTER_CHILD_DIR_ENUMERATOR(object);

    switch (prop_id) {
    case PROP_SOURCE:
        enumerator->source = BEDIT_FILE_BROWSER_FILTER_DIR_ENUMERATOR(
            g_value_dup_object(value)
        );
        break;

    case PROP_PATTERN:
        enumerator->pattern = g_value_dup_string(value);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void bedit_file_browser_filter_child_dir_enumerator_class_init(
    BeditFileBrowserFilterChildDirEnumeratorClass *klass
) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->get_property =
        bedit_file_browser_filter_child_dir_enumerator_get_property;
    object_class->set_property =
        bedit_file_browser_filter_child_dir_enumerator_set_property;

    g_object_class_install_property(
        object_class, PROP_SOURCE,
        g_param_spec_object(
            "source", "Source directory enumerator",
            "Enumerator that yields directories to filter in",
            BEDIT_TYPE_FILE_BROWSER_FILTER_DIR_ENUMERATOR,
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY
        )
    );

    g_object_class_install_property(
        object_class, PROP_PATTERN,
        g_param_spec_string(
            "pattern", "Pattern",
            "Pattern that files must match to be included in the result",
            "",
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY
        )
    );
}

static void bedit_file_browser_filter_child_dir_enumerator_iface_init(
    BeditFileBrowserFilterDirEnumeratorInterface *interface
) {
    interface->iterate = bedit_file_browser_filter_child_dir_enumerator_iterate;
}

static void bedit_file_browser_filter_child_dir_enumerator_init(
    BeditFileBrowserFilterChildDirEnumerator *enumerator
) {
    enumerator->source = NULL;
    enumerator->pattern = NULL;

    enumerator->matches = NULL;
}

BeditFileBrowserFilterChildDirEnumerator
*bedit_file_browser_filter_child_dir_enumerator_new(
    BeditFileBrowserFilterDirEnumerator *source, gchar *pattern
) {
    BeditFileBrowserFilterChildDirEnumerator *obj;
    g_return_val_if_fail(
        BEDIT_IS_FILE_BROWSER_FILTER_DIR_ENUMERATOR(source), NULL
    );
    g_return_val_if_fail(pattern != NULL, NULL);

    obj = g_object_new(
        BEDIT_TYPE_FILE_BROWSER_FILTER_CHILD_DIR_ENUMERATOR,
        "source", source, "pattern", pattern,
        NULL
    );
    g_return_val_if_fail(
        BEDIT_IS_FILE_BROWSER_FILTER_CHILD_DIR_ENUMERATOR(obj), obj
    );
    return obj;
}

typedef struct _Match Match;

struct _Match {
    GFile *file;
    GFileInfo *info;
    gchar *markup;
    guint64 quality;
};

static gint bedit_file_browser_filter_dir_enumerator_compare_match(
    gconstpointer a, gconstpointer b
) {
    Match const *a_match, *b_match;
    GFileInfo *a_file_info, *b_file_info;
    gboolean a_is_hidden, b_is_hidden;
    gchar *a_collate_key, *b_collate_key;
    gint result;

    a_match = a;
    b_match = b;

    a_file_info = G_FILE_INFO(a_match->info);
    b_file_info = G_FILE_INFO(b_match->info);

    if (a_match->quality < b_match->quality) {
        return 1;
    }
    if (b_match->quality < a_match->quality) {
        return -1;
    }

    a_is_hidden = g_file_info_get_is_hidden(a_file_info);
    b_is_hidden = g_file_info_get_is_hidden(b_file_info);
    if (a_is_hidden && !b_is_hidden) {
        return 1;
    }
    if (b_is_hidden && !a_is_hidden) {
        return -1;
    }

    a_collate_key = g_utf8_collate_key_for_filename(
        g_file_info_get_name(a_file_info), -1
    );
    b_collate_key = g_utf8_collate_key_for_filename(
        g_file_info_get_name(b_file_info), -1
    );

    result = strcmp(
        g_file_info_get_name(a_file_info),
        g_file_info_get_name(b_file_info)
    );

    g_free(a_collate_key);
    g_free(b_collate_key);

    return result;
}

static gboolean bedit_file_browser_filter_child_dir_enumerator_iterate(
    BeditFileBrowserFilterDirEnumerator *instance,
    GFile **out_dir,
    gchar **out_markup,
    GCancellable *cancellable,
    GError **error
) {
    BeditFileBrowserFilterChildDirEnumerator *enumerator;
    Match *match;

    enumerator = BEDIT_FILE_BROWSER_FILTER_CHILD_DIR_ENUMERATOR(instance);

    if (g_cancellable_is_cancelled(cancellable)) {
        *error = g_error_new(
            G_IO_ERROR, G_IO_ERROR_CANCELLED,
            "Operation was cancelled"
        );
        return FALSE;
    }

    while (enumerator->matches == NULL) {
        GFile *dir_file;
        gchar *dir_markup;
        GFileEnumerator *child_enumerator;
        GString *markup_buffer;

        if (!bedit_file_browser_filter_dir_enumerator_iterate(
            enumerator->source,
            &dir_file,
            &dir_markup,
            cancellable, error
        )) {
            return FALSE;
        };

        child_enumerator = g_file_enumerate_children(
            dir_file, STANDARD_ATTRIBUTE_TYPES, G_FILE_QUERY_INFO_NONE,
            cancellable, error
        );
        if (child_enumerator == NULL) {
            g_free(dir_markup);
            g_object_unref(dir_file);

            if (
                g_error_matches(*error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND) ||
                g_error_matches(
                    *error, G_IO_ERROR, G_IO_ERROR_NOT_DIRECTORY
                ) ||
                g_error_matches(
                    *error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED
                )
            ) {
                // TODO log error.
                g_clear_error(error);
                continue;
            }

            return FALSE;
        }

        markup_buffer = g_string_sized_new(64);
        g_return_val_if_fail(markup_buffer != NULL, FALSE);

        while (TRUE) {
            GFileInfo *child_info = NULL;
            gchar const *child_name;
            guint64 quality;

            child_info = g_file_enumerator_next_file(
                child_enumerator, cancellable, error
            );
            if (*error) {
                // TODO cleanup.
                // TODO what happens if the folder disappears half way through
                // listing.
                return FALSE;
            }

            if (child_info == NULL) {
                break;
            }

            if (
                g_file_info_get_file_type(child_info) != G_FILE_TYPE_DIRECTORY
            ) {
                g_object_unref(child_info);
                continue;
            }

            child_name = g_file_info_get_name(child_info);

            if (bedit_file_browser_filter_match_segment(
                enumerator->pattern, child_name,
                markup_buffer, &quality
            )) {
                match = g_slice_alloc0(sizeof(Match));
                g_return_val_if_fail(match != NULL, FALSE);

                match->file = g_file_get_child(dir_file, child_name);
                match->info = g_object_ref(child_info);
                match->markup = g_strdup_printf(
                    "%s%s/",
                    (gchar *) dir_markup,
                    markup_buffer->str
                );
                match->quality = quality;

                enumerator->matches = g_list_prepend(
                    enumerator->matches, match
                );
            }

            g_object_unref(child_info);
        }

        enumerator->matches = g_list_sort(
            enumerator->matches,
            bedit_file_browser_filter_dir_enumerator_compare_match
        );

        g_string_free(markup_buffer, TRUE);

        g_object_unref(child_enumerator);

        g_free(dir_markup);
        g_object_unref(dir_file);
    }

    /* Pop next match from list of matches in the last directory. */
    match = enumerator->matches->data;
    enumerator->matches = g_list_delete_link(
        enumerator->matches, enumerator->matches
    );

    /* Unpack match into out parameters. */
    if (out_dir != NULL) {
        *out_dir = g_steal_pointer(&match->file);
    } else {
        g_clear_object(&match->file);
    }

    if (out_markup != NULL) {
        *out_markup = g_steal_pointer(&match->markup);
    } else {
        g_clear_pointer(&match->markup, g_free);
    }

    g_slice_free1(sizeof(Match), match);

    return TRUE;
}

