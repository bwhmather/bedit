/*
 * bedit-trim-trailing-plugin.c
 *
 * Copyright (C) 2020 Ben Mather
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"

#include <glib/gi18n.h>
#include <libpeas-gtk/peas-gtk-configurable.h>

#include <bedit/bedit-debug.h>
#include <bedit/bedit-document.h>
#include <bedit/bedit-view-activatable.h>
#include <bedit/bedit-view.h>

#include "bedit-trim-trailing-plugin.h"

struct _BeditTrimTrailingPlugin {
    PeasExtensionBase parent_instance;

    BeditView *view;
    gulong save_handler_id;
};

enum { PROP_0, PROP_VIEW, LAST_PROP };

static void bedit_view_activatable_iface_init(
    BeditViewActivatableInterface *iface
);

static BeditDocument *bedit_trim_trailing_plugin_get_document(
    BeditTrimTrailingPlugin *plugin
);

G_DEFINE_DYNAMIC_TYPE_EXTENDED(
    BeditTrimTrailingPlugin, bedit_trim_trailing_plugin,
    PEAS_TYPE_EXTENSION_BASE, 0,
    G_IMPLEMENT_INTERFACE_DYNAMIC(
        BEDIT_TYPE_VIEW_ACTIVATABLE, bedit_view_activatable_iface_init
    )
)

static void bedit_trim_trailing_plugin_dispose(GObject *object) {
    BeditTrimTrailingPlugin *plugin;

    plugin = BEDIT_TRIM_TRAILING_PLUGIN(object);

    g_clear_object(&plugin->view);

    G_OBJECT_CLASS(bedit_trim_trailing_plugin_parent_class)->dispose(object);
}

static void bedit_trim_trailing_plugin_set_property(
    GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec
) {
    BeditTrimTrailingPlugin *plugin;

    plugin = BEDIT_TRIM_TRAILING_PLUGIN(object);

    switch (prop_id) {
    case PROP_VIEW:
        plugin->view = BEDIT_VIEW(g_value_dup_object(value));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void bedit_trim_trailing_plugin_get_property(
    GObject *object, guint prop_id, GValue *value, GParamSpec *pspec
) {
    BeditTrimTrailingPlugin *plugin;

    plugin = BEDIT_TRIM_TRAILING_PLUGIN(object);

    switch (prop_id) {
    case PROP_VIEW:
        g_value_set_object(value, plugin->view);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void trim_trailing_whitespace(GtkTextBuffer *buffer) {
    GtkTextIter iter;

    g_return_if_fail(GTK_IS_TEXT_BUFFER(buffer));

    gtk_text_buffer_begin_user_action(buffer);

    gtk_text_buffer_get_start_iter(buffer, &iter);
    while (!gtk_text_iter_is_end(&iter)) {
        GtkTextIter cursor;

        gtk_text_iter_forward_to_line_end(&iter);

        cursor = iter;

        while (!gtk_text_iter_starts_line(&cursor)) {
            GtkTextIter lookahead;
            gunichar ch;

            lookahead = cursor;
            gtk_text_iter_backward_char(&lookahead);
            ch = gtk_text_iter_get_char(&lookahead);

            if (ch != ' ' && ch != '\t') {
                break;
            }

            cursor = lookahead;
        }

        if (!gtk_text_iter_equal(&cursor, &iter)) {
            gtk_text_buffer_delete(buffer, &cursor, &iter);
        }
    }

    gtk_text_buffer_end_user_action(buffer);
}


static void document_save_cb(
    BeditDocument *document, BeditTrimTrailingPlugin *plugin
) {
    g_return_if_fail(BEDIT_IS_DOCUMENT(document));
    g_return_if_fail(BEDIT_IS_TRIM_TRAILING_PLUGIN(plugin));

    trim_trailing_whitespace(GTK_TEXT_BUFFER(document));
}

static void bedit_trim_trailing_plugin_class_init(
    BeditTrimTrailingPluginClass *klass
) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->dispose = bedit_trim_trailing_plugin_dispose;
    object_class->set_property = bedit_trim_trailing_plugin_set_property;
    object_class->get_property = bedit_trim_trailing_plugin_get_property;

    g_object_class_override_property(object_class, PROP_VIEW, "view");
}

static void bedit_trim_trailing_plugin_class_finalize(
    BeditTrimTrailingPluginClass *klass
) {}

static void bedit_trim_trailing_plugin_init(
    BeditTrimTrailingPlugin *plugin
) {}

static void bedit_trim_trailing_plugin_activate(
    BeditViewActivatable *activatable
) {
    BeditTrimTrailingPlugin *plugin;
    BeditDocument *document;

    bedit_debug(DEBUG_PLUGINS);

    plugin = BEDIT_TRIM_TRAILING_PLUGIN(activatable);

    document = bedit_trim_trailing_plugin_get_document(plugin);
    g_return_if_fail(BEDIT_IS_DOCUMENT(document));

    plugin->save_handler_id = g_signal_connect_data(
        document, "save",
        G_CALLBACK(document_save_cb), plugin,
        0, 0
    );
}

static void bedit_trim_trailing_plugin_deactivate(
    BeditViewActivatable *activatable
) {
    BeditTrimTrailingPlugin *plugin;
    BeditDocument *document;

    bedit_debug(DEBUG_PLUGINS);

    plugin = BEDIT_TRIM_TRAILING_PLUGIN(activatable);

    document = bedit_trim_trailing_plugin_get_document(plugin);
    g_return_if_fail(BEDIT_IS_DOCUMENT(document));

    g_signal_handler_disconnect(document, plugin->save_handler_id);
}

static void bedit_view_activatable_iface_init(
    BeditViewActivatableInterface *iface
) {
    iface->activate = bedit_trim_trailing_plugin_activate;
    iface->deactivate = bedit_trim_trailing_plugin_deactivate;
}

static BeditDocument *bedit_trim_trailing_plugin_get_document(
    BeditTrimTrailingPlugin *plugin
) {
    GtkTextBuffer *buffer;

    g_return_val_if_fail(BEDIT_IS_TRIM_TRAILING_PLUGIN(plugin), NULL);
    g_return_val_if_fail(BEDIT_IS_VIEW(plugin->view), NULL);
    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(plugin->view));
    g_return_val_if_fail(BEDIT_IS_DOCUMENT(buffer), NULL);
    return BEDIT_DOCUMENT(buffer);
}

G_MODULE_EXPORT void peas_register_types(PeasObjectModule *module) {
    bedit_trim_trailing_plugin_register_type(G_TYPE_MODULE(module));

    peas_object_module_register_extension_type(
        module, BEDIT_TYPE_VIEW_ACTIVATABLE, BEDIT_TYPE_TRIM_TRAILING_PLUGIN
    );
}

