/*
 * bedit-utils.c
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * Based on gedit-utils.c from Gedit.
 *
 * Copyright (C) 1998-1999 - Alex Roberts, Evan Lawrence
 * Copyright (C) 2000-2007 - Paolo Maggi
 * Copyright (C) 2000 - Jacob Leach
 * Copyright (C) 2000-2002 - Chema Celorio
 * Copyright (C) 2001 - Carlos Perelló Marín
 * Copyright (C) 2002 - Hema Seetharamaiah, James Willcox, Kjartan Maraas
 * Copyright (C) 2004 - Vinay
 * Copyright (C) 2004-2014 - Paolo Borelli
 * Copyright (C) 2005 - Ray Strode
 * Copyright (C) 2006 - Frank Arnold
 * Copyright (C) 2006-2010 - Steve Frécinaux
 * Copyright (C) 2007-2014 - Jesse van den Kieboom
 * Copyright (C) 2008 - Joe M Smith
 * Copyright (C) 2009-2010 - Javier Jardón
 * Copyright (C) 2009-2013 - Ignacio Casal Quinteiro
 * Copyright (C) 2010 - Philip Withnall
 * Copyright (C) 2010-2013 - Garrett Regier
 * Copyright (C) 2013 - Alexander Larsson, Nelson Benítez León
 * Copyright (C) 2013-2019 - Sébastien Wilmet
 * Copyright (C) 2014 - Boris Egorov, Robert Roth
 * Copyright (C) 2014-2018 - Sebastien Lafargue
 * Copyright (C) 2016 - Piotr Drąg
 * Copyright (C) 2019 - Jordi Mas, Michael Catanzaro
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

#include "bedit-utils.h"

#include <glib/gi18n.h>
#include <string.h>

#include "bedit-debug.h"

static void widget_get_origin(GtkWidget *widget, gint *x, gint *y) {
    GdkWindow *window;

    window = gtk_widget_get_window(widget);
    gdk_window_get_origin(window, x, y);
}

/**
 * bedit_utils_menu_position_under_widget:
 * @menu:
 * @x:
 * @y:
 * @push_in:
 * @user_data:
 *
 * Deprecated: 3.36: Use gtk_menu_popup_at_widget() instead.
 */
void bedit_utils_menu_position_under_widget(
    GtkMenu *menu, gint *x, gint *y, gboolean *push_in, gpointer user_data
) {
    GtkWidget *widget;
    GtkRequisition requisition;
    GtkAllocation allocation;

    widget = GTK_WIDGET(user_data);
    widget_get_origin(widget, x, y);

    gtk_widget_get_preferred_size(GTK_WIDGET(menu), &requisition, NULL);

    gtk_widget_get_allocation(widget, &allocation);

    if (gtk_widget_get_direction(widget) == GTK_TEXT_DIR_RTL) {
        *x += allocation.x + allocation.width - requisition.width;
    } else {
        *x += allocation.x;
    }

    *y += allocation.y + allocation.height;

    *push_in = TRUE;
}

gboolean bedit_utils_menu_position_under_tree_view(
    GtkTreeView *tree_view, GdkRectangle *rect
) {
    GtkTreeSelection *selection;
    GtkTreeModel *model;
    gint count_rows;
    GList *rows;
    gint widget_x, widget_y;

    model = gtk_tree_view_get_model(tree_view);
    g_return_val_if_fail(model != NULL, FALSE);

    selection = gtk_tree_view_get_selection(tree_view);
    g_return_val_if_fail(selection != NULL, FALSE);

    count_rows = gtk_tree_selection_count_selected_rows(selection);
    if (count_rows != 1) {
        return FALSE;
    }

    rows = gtk_tree_selection_get_selected_rows(selection, &model);
    gtk_tree_view_get_cell_area(
        tree_view, (GtkTreePath *)(rows->data),
        gtk_tree_view_get_column(tree_view, 0), rect
    );

    gtk_tree_view_convert_bin_window_to_widget_coords(
        tree_view, rect->x, rect->y, &widget_x, &widget_y
    );
    rect->x = widget_x;
    rect->y = widget_y;

    g_list_free_full(rows, (GDestroyNotify)gtk_tree_path_free);
    return TRUE;
}

/**
 * bedit_utils_set_atk_name_description:
 * @widget: The Gtk widget for which name/description to be set
 * @name: Atk name string
 * @description: Atk description string
 *
 * This function sets up name and description
 * for a specified gtk widget.
 */
void bedit_utils_set_atk_name_description(
    GtkWidget *widget, const gchar *name, const gchar *description
) {
    AtkObject *aobj;

    aobj = gtk_widget_get_accessible(widget);

    if (!(GTK_IS_ACCESSIBLE(aobj))) {
        return;
    }

    if (name) {
        atk_object_set_name(aobj, name);
    }

    if (description) {
        atk_object_set_description(aobj, description);
    }
}

/**
 * bedit_utils_set_atk_relation:
 * @obj1: specified widget.
 * @obj2: specified widget.
 * @rel_type: the type of relation to set up.
 *
 * This function establishes atk relation
 * between 2 specified widgets.
 *
 * Deprecated: 3.36: This function is no longer used by bedit. If you need it,
 * copy it.
 */
void bedit_utils_set_atk_relation(
    GtkWidget *obj1, GtkWidget *obj2, AtkRelationType rel_type
) {
    AtkObject *atk_obj1, *atk_obj2;
    AtkRelationSet *relation_set;
    AtkObject *targets[1];
    AtkRelation *relation;

    atk_obj1 = gtk_widget_get_accessible(obj1);
    atk_obj2 = gtk_widget_get_accessible(obj2);

    if (!(GTK_IS_ACCESSIBLE(atk_obj1)) || !(GTK_IS_ACCESSIBLE(atk_obj2))) {
        return;
    }

    relation_set = atk_object_ref_relation_set(atk_obj1);
    targets[0] = atk_obj2;

    relation = atk_relation_new(targets, 1, rel_type);
    atk_relation_set_add(relation_set, relation);

    g_object_unref(G_OBJECT(relation));
}

void bedit_warning(GtkWindow *parent, const gchar *format, ...) {
    va_list args;
    gchar *str;
    GtkWidget *dialog;
    GtkWindowGroup *wg = NULL;

    g_return_if_fail(format != NULL);

    if (parent != NULL) {
        wg = gtk_window_get_group(parent);
    }

    va_start(args, format);
    str = g_strdup_vprintf(format, args);
    va_end(args);

    dialog = gtk_message_dialog_new_with_markup(
        parent, GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "%s", str
    );

    g_free(str);

    if (wg != NULL) {
        gtk_window_group_add_window(wg, GTK_WINDOW(dialog));
    }

    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);

    g_signal_connect(
        G_OBJECT(dialog), "response", G_CALLBACK(gtk_widget_destroy), NULL
    );

    gtk_widget_show(dialog);
}

static gchar *str_truncate(
    const gchar *string, guint truncate_length, gboolean middle
) {
    GString *truncated;
    guint length;
    guint n_chars;
    guint num_left_chars;
    guint right_offset;
    guint delimiter_length;
    const gchar *delimiter = "\342\200\246"; /* The character: … */

    g_return_val_if_fail(string != NULL, NULL);

    length = strlen(string);

    g_return_val_if_fail(g_utf8_validate(string, length, NULL), NULL);

    /* It doesnt make sense to truncate strings to less than
     * the size of the delimiter plus 2 characters (one on each
     * side)
     */
    delimiter_length = g_utf8_strlen(delimiter, -1);
    if (truncate_length < (delimiter_length + 2)) {
        return g_strdup(string);
    }

    n_chars = g_utf8_strlen(string, length);

    /* Make sure the string is not already small enough. */
    if (n_chars <= truncate_length) {
        return g_strdup(string);
    }

    /* Find the 'middle' where the truncation will occur. */
    if (middle) {
        num_left_chars = (truncate_length - delimiter_length) / 2;
        right_offset =
            n_chars - truncate_length + num_left_chars + delimiter_length;

        truncated = g_string_new_len(
            string, g_utf8_offset_to_pointer(string, num_left_chars) - string
        );
        g_string_append(truncated, delimiter);
        g_string_append(
            truncated, g_utf8_offset_to_pointer(string, right_offset)
        );
    } else {
        num_left_chars = truncate_length - delimiter_length;
        truncated = g_string_new_len(
            string, g_utf8_offset_to_pointer(string, num_left_chars) - string
        );
        g_string_append(truncated, delimiter);
    }

    return g_string_free(truncated, FALSE);
}

/**
 * bedit_utils_str_middle_truncate:
 * @string:
 * @truncate_length:
 *
 * Returns:
 */
gchar *bedit_utils_str_middle_truncate(
    const gchar *string, guint truncate_length
) {
    return str_truncate(string, truncate_length, TRUE);
}

/**
 * bedit_utils_str_end_truncate:
 * @string:
 * @truncate_length:
 *
 * Returns:
 */
gchar *bedit_utils_str_end_truncate(
    const gchar *string, guint truncate_length
) {
    return str_truncate(string, truncate_length, FALSE);
}

/**
 * bedit_utils_make_valid_utf8:
 * @name:
 *
 * Returns:
 * Deprecated: 3.36: Use g_utf8_make_valid() instead.
 */
gchar *bedit_utils_make_valid_utf8(const char *name) {
    return g_utf8_make_valid(name, -1);
}

static gchar *uri_get_dirname(const gchar *uri) {
    gchar *res;
    gchar *str;

    g_return_val_if_fail(uri != NULL, NULL);

    /* CHECK: does it work with uri chaining? - Paolo */
    str = g_path_get_dirname(uri);
    g_return_val_if_fail(str != NULL, g_strdup("."));

    if ((strlen(str) == 1) && (*str == '.')) {
        g_free(str);

        return NULL;
    }

    res = bedit_utils_replace_home_dir_with_tilde(str);

    g_free(str);

    return res;
}

/**
 * bedit_utils_location_get_dirname_for_display:
 * @location: the location
 *
 * Returns a string suitable to be displayed in the UI indicating
 * the name of the directory where the file is located.
 * For remote files it may also contain the hostname etc.
 * For local files it tries to replace the home dir with ~.
 *
 * Returns: (transfer full): a string to display the dirname
 */
gchar *bedit_utils_location_get_dirname_for_display(GFile *location) {
    gchar *uri;
    gchar *res;
    GMount *mount;

    g_return_val_if_fail(location != NULL, NULL);

    /* we use the parse name, that is either the local path
     * or an uri but which is utf8 safe */
    uri = g_file_get_parse_name(location);

    /* FIXME: this is sync... is it a problem? */
    mount = g_file_find_enclosing_mount(location, NULL, NULL);
    if (mount != NULL) {
        gchar *mount_name;
        gchar *path = NULL;
        gchar *dirname;

        mount_name = g_mount_get_name(mount);
        g_object_unref(mount);

        /* obtain the "path" part of the uri */
        bedit_utils_decode_uri(uri, NULL, NULL, NULL, NULL, &path);

        if (path == NULL) {
            dirname = uri_get_dirname(uri);
        } else {
            dirname = uri_get_dirname(path);
        }

        if (dirname == NULL || strcmp(dirname, ".") == 0) {
            res = mount_name;
        } else {
            res = g_strdup_printf("%s %s", mount_name, dirname);
            g_free(mount_name);
        }

        g_free(path);
        g_free(dirname);
    } else {
        /* fallback for local files or uris without mounts */
        res = uri_get_dirname(uri);
    }

    g_free(uri);

    return res;
}

/**
 * bedit_utils_replace_home_dir_with_tilde:
 * @uri:
 *
 * Returns:
 */
gchar *bedit_utils_replace_home_dir_with_tilde(const gchar *filename) {
    gchar *tmp;
    gchar *home;

    g_return_val_if_fail(filename != NULL, NULL);

    /* Note that g_get_home_dir returns a const string */
    tmp = (gchar *)g_get_home_dir();

    if (tmp == NULL) {
        return g_strdup(filename);
    }

    home = g_filename_to_utf8(tmp, -1, NULL, NULL, NULL);
    if (home == NULL) {
        return g_strdup(filename);
    }

    if (g_str_equal(filename, home)) {
        g_free(home);
        return g_strdup("~");
    }

    tmp = home;
    home = g_strdup_printf("%s/", tmp);
    g_free(tmp);

    if (g_str_has_prefix(filename, home)) {
        gchar *res = g_strdup_printf("~/%s", filename + strlen(home));
        g_free(home);
        return res;
    }

    g_free(home);
    return g_strdup(filename);
}

static gboolean is_valid_scheme_character(gchar c) {
    return g_ascii_isalnum(c) || c == '+' || c == '-' || c == '.';
}

static gboolean has_valid_scheme(const gchar *uri) {
    const gchar *p;

    p = uri;

    if (!is_valid_scheme_character(*p)) {
        return FALSE;
    }

    do {
        p++;
    } while (is_valid_scheme_character(*p));

    return *p == ':';
}

gboolean bedit_utils_is_valid_location(GFile *location) {
    const guchar *p;
    gchar *uri;
    gboolean is_valid;

    if (location == NULL) {
        return FALSE;
    }

    uri = g_file_get_uri(location);

    if (!has_valid_scheme(uri)) {
        g_free(uri);
        return FALSE;
    }

    is_valid = TRUE;

    /* We expect to have a fully valid set of characters */
    for (p = (const guchar *)uri; *p; p++) {
        if (*p == '%') {
            ++p;
            if (!g_ascii_isxdigit(*p)) {
                is_valid = FALSE;
                break;
            }

            ++p;
            if (!g_ascii_isxdigit(*p)) {
                is_valid = FALSE;
                break;
            }
        } else {
            if (*p <= 32 || *p >= 128) {
                is_valid = FALSE;
                break;
            }
        }
    }

    g_free(uri);

    return is_valid;
}

static gchar *make_canonical_uri_from_shell_arg(const gchar *str) {
    GFile *gfile;
    gchar *uri;

    g_return_val_if_fail(str != NULL, NULL);
    g_return_val_if_fail(*str != '\0', NULL);

    /* Note for the future:
     * FIXME: is still still relevant?
     *
     * <federico> paolo: and flame whoever tells
     * you that file:///gnome/test_files/hëllò
     * doesn't work --- that's not a valid URI
     *
     * <paolo> federico: well, another solution that
     * does not requires patch to _from_shell_args
     * is to check that the string returned by it
     * contains only ASCII chars
     * <federico> paolo: hmmmm, isn't there
     * gnome_vfs_is_uri_valid() or something?
     * <paolo>: I will use bedit_utils_is_valid_uri ()
     *
     */

    gfile = g_file_new_for_commandline_arg(str);

    if (bedit_utils_is_valid_location(gfile)) {
        uri = g_file_get_uri(gfile);
        g_object_unref(gfile);
        return uri;
    }

    g_object_unref(gfile);
    return NULL;
}

/**
 * bedit_utils_basename_for_display:
 * @location: location for which the basename should be displayed
 *
 * Returns: (transfer full): the basename of a file suitable for display to
 * users.
 */
gchar *bedit_utils_basename_for_display(GFile *location) {
    gchar *name;
    gchar *hn;
    gchar *uri;

    g_return_val_if_fail(G_IS_FILE(location), NULL);

    uri = g_file_get_uri(location);

    /* First, try to query the display name, but only on local files */
    if (g_file_has_uri_scheme(location, "file")) {
        GFileInfo *info;

        info = g_file_query_info(
            location, G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
            G_FILE_QUERY_INFO_NONE, NULL, NULL
        );

        if (info) {
            /* Simply get the display name to use as the basename */
            name = g_strdup(g_file_info_get_display_name(info));
            g_object_unref(info);
        } else {
            /* This is a local file, and therefore we will use
             * g_filename_display_basename on the local path */
            gchar *local_path;

            local_path = g_file_get_path(location);
            name = g_filename_display_basename(local_path);
            g_free(local_path);
        }
    } else if (
        g_file_has_parent(location, NULL) ||
        !bedit_utils_decode_uri(uri, NULL, NULL, &hn, NULL, NULL)
    ) {
        /* For remote files with a parent (so not just http://foo.com)
           or remote file for which the decoding of the host name fails,
           use the _parse_name and take basename of that */
        gchar *parse_name;
        gchar *base;

        parse_name = g_file_get_parse_name(location);
        base = g_filename_display_basename(parse_name);
        name = g_uri_unescape_string(base, NULL);

        g_free(base);
        g_free(parse_name);
    } else {
        /* display '/ on <host>' using the decoded host */
        gchar *hn_utf8;

        if (hn != NULL) {
            hn_utf8 = g_utf8_make_valid(hn, -1);
        } else {
            /* we should never get here */
            hn_utf8 = g_strdup("?");
        }

        /* Translators: '/ on <remote-share>' */
        name = g_strdup_printf(_("/ on %s"), hn_utf8);

        g_free(hn_utf8);
        g_free(hn);
    }

    g_free(uri);

    return name;
}

/**
 * bedit_utils_drop_get_uris:
 * @selection_data: the #GtkSelectionData from drag_data_received
 *
 * Create a list of valid uri's from a uri-list drop.
 *
 * Returns: (transfer full): a string array which will hold the uris or
 *           %NULL if there were no valid uris. g_strfreev should be used when
 *           the string array is no longer used
 */
gchar **bedit_utils_drop_get_uris(GtkSelectionData *selection_data) {
    gchar **uris;
    gint i;
    gint p = 0;
    gchar **uri_list;

    uris = g_uri_list_extract_uris(
        (gchar *)gtk_selection_data_get_data(selection_data)
    );
    uri_list = g_new0(gchar *, g_strv_length(uris) + 1);

    for (i = 0; uris[i] != NULL; i++) {
        gchar *uri;

        uri = make_canonical_uri_from_shell_arg(uris[i]);

        /* Silently ignore malformed URI/filename */
        if (uri != NULL) {
            uri_list[p++] = uri;
        }
    }

    if (*uri_list == NULL) {
        g_free(uri_list);
        g_strfreev(uris);
        return NULL;
    }

    g_strfreev(uris);
    return uri_list;
}

static void null_ptr(gchar **ptr) {
    if (ptr) {
        *ptr = NULL;
    }
}

/**
 * bedit_utils_decode_uri:
 * @uri: the uri to decode
 * @scheme: (out) (allow-none): return value pointer for the uri's
 * scheme (e.g. http, sftp, ...), or %NULL
 * @user: (out) (allow-none): return value pointer for the uri user info, or
 * %NULL
 * @port: (out) (allow-none): return value pointer for the uri port, or %NULL
 * @host: (out) (allow-none): return value pointer for the uri host, or %NULL
 * @path: (out) (allow-none): return value pointer for the uri path, or %NULL
 *
 * Parse and break an uri apart in its individual components like the uri
 * scheme, user info, port, host and path. The return value pointer can be
 * %NULL to ignore certain parts of the uri. If the function returns %TRUE, then
 * all return value pointers should be freed using g_free
 *
 * Return value: %TRUE if the uri could be properly decoded, %FALSE otherwise.
 */
gboolean bedit_utils_decode_uri(
    const gchar *uri, gchar **scheme, gchar **user,
    gchar **host, gchar **port,
    gchar **path
) {
    /* Largely copied from glib/gio/gdummyfile.c:_g_decode_uri. This
     * functionality should be in glib/gio, but for now we implement it
     * ourselves (see bug #546182) */
    const char *p, *in, *hier_part_start, *hier_part_end;
    char *out;
    char c;

    /* From RFC 3986 Decodes:
     * URI = scheme ":" hier-part [ "?" query ] [ "#" fragment ]
     */
    p = uri;

    null_ptr(scheme);
    null_ptr(user);
    null_ptr(port);
    null_ptr(host);
    null_ptr(path);

    /* Decode scheme:
     * scheme = ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )
     */
    if (!g_ascii_isalpha(*p)) {
        return FALSE;
    }

    while (1) {
        c = *p++;

        if (c == ':') {
            break;
        }

        if (!(g_ascii_isalnum(c) || c == '+' || c == '-' || c == '.')) {
            return FALSE;
        }
    }

    if (scheme) {
        *scheme = g_malloc(p - uri);
        out = *scheme;

        for (in = uri; in < p - 1; in++) {
            *out++ = g_ascii_tolower(*in);
        }

        *out = '\0';
    }

    hier_part_start = p;
    hier_part_end = p + strlen(p);

    if (hier_part_start[0] == '/' && hier_part_start[1] == '/') {
        const char *authority_start, *authority_end;
        const char *userinfo_start, *userinfo_end;
        const char *host_start, *host_end;
        const char *port_start;

        authority_start = hier_part_start + 2;
        /* authority is always followed by / or nothing */
        authority_end = memchr(
            authority_start, '/', hier_part_end - authority_start
        );

        if (authority_end == NULL) {
            authority_end = hier_part_end;
        }

        /* 3.2:
         * authority = [ userinfo "@" ] host [ ":" port ]
         */

        userinfo_end = memchr(
            authority_start, '@', authority_end - authority_start
        );

        if (userinfo_end) {
            userinfo_start = authority_start;

            if (user) {
                *user = g_uri_unescape_segment(
                    userinfo_start, userinfo_end, NULL
                );
            }

            if (user && *user == NULL) {
                if (scheme) {
                    g_free(*scheme);
                }

                return FALSE;
            }

            host_start = userinfo_end + 1;
        } else {
            host_start = authority_start;
        }

        port_start = memchr(host_start, ':', authority_end - host_start);

        if (port_start) {
            host_end = port_start++;

            if (port) {
                *port = g_strndup(port_start, authority_end - port_start);
            }
        } else {
            host_end = authority_end;
        }

        if (host) {
            *host = g_strndup(host_start, host_end - host_start);
        }

        hier_part_start = authority_end;
    }

    if (path) {
        *path = g_uri_unescape_segment(hier_part_start, hier_part_end, "/");
    }

    return TRUE;
}

GtkSourceCompressionType bedit_utils_get_compression_type_from_content_type(
    const gchar *content_type
) {
    if (content_type == NULL) {
        return GTK_SOURCE_COMPRESSION_TYPE_NONE;
    }

    if (g_content_type_is_a(content_type, "application/x-gzip")) {
        return GTK_SOURCE_COMPRESSION_TYPE_GZIP;
    }

    return GTK_SOURCE_COMPRESSION_TYPE_NONE;
}

/* Copied from nautilus */
static gchar *get_direct_save_filename(GdkDragContext *context) {
    guchar *prop_text;
    gint prop_len;

    if (!gdk_property_get(
        gdk_drag_context_get_source_window(context),
        gdk_atom_intern("XdndDirectSave0", FALSE),
        gdk_atom_intern("text/plain", FALSE), 0, 1024, FALSE, NULL, NULL,
        &prop_len, &prop_text
    ) && prop_text != NULL) {
        return NULL;
    }

    /* Zero-terminate the string */
    prop_text = g_realloc(prop_text, prop_len + 1);
    prop_text[prop_len] = '\0';

    /* Verify that the file name provided by the source is valid */
    if (
        *prop_text == '\0' ||
        strchr((const gchar *)prop_text, G_DIR_SEPARATOR) != NULL
    ) {
        bedit_debug_message(
            DEBUG_UTILS, "Invalid filename provided by XDS drag site"
        );
        g_free(prop_text);
        return NULL;
    }

    return (gchar *)prop_text;
}

gchar *bedit_utils_set_direct_save_filename(GdkDragContext *context) {
    gchar *uri;
    gchar *filename;

    uri = NULL;
    filename = get_direct_save_filename(context);

    if (filename != NULL) {
        gchar *tempdir;
        gchar *path;

        tempdir = g_dir_make_tmp("bedit-drop-XXXXXX", NULL);
        if (tempdir == NULL) {
            tempdir = g_strdup(g_get_tmp_dir());
        }

        path = g_build_filename(tempdir, filename, NULL);

        uri = g_filename_to_uri(path, NULL, NULL);

        /* Change the property */
        gdk_property_change(
            gdk_drag_context_get_source_window(context),
            gdk_atom_intern("XdndDirectSave0", FALSE),
            gdk_atom_intern("text/plain", FALSE), 8, GDK_PROP_MODE_REPLACE,
            (const guchar *)uri, strlen(uri)
        );

        g_free(tempdir);
        g_free(path);
        g_free(filename);
    }

    return uri;
}

const gchar *bedit_utils_newline_type_to_string(
    GtkSourceNewlineType newline_type
) {
    switch (newline_type) {
    case GTK_SOURCE_NEWLINE_TYPE_LF:
        return _("Unix/Linux");
    case GTK_SOURCE_NEWLINE_TYPE_CR:
        return _("Mac OS Classic");
    case GTK_SOURCE_NEWLINE_TYPE_CR_LF:
        return _("Windows");
    }

    return NULL;
}

