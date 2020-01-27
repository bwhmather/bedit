/*
 * bedit-modeline-plugin.c
 * Emacs, Kate and Vim-style modelines support for bedit.
 *
 * Copyright (C) 2005-2010 - Steve Frécinaux <code@istique.net>
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
 */

#include "config.h"

#include <glib/gi18n-lib.h>
#include <gmodule.h>
#include "bedit-modeline-plugin.h"
#include "modeline-parser.h"

#include <bedit/bedit-debug.h>
#include <bedit/bedit-view-activatable.h>
#include <bedit/bedit-view.h>

struct _BeditModelinePluginPrivate {
    BeditView *view;

    gulong document_loaded_handler_id;
    gulong document_saved_handler_id;
};

enum { PROP_0, PROP_VIEW };

static void bedit_view_activatable_iface_init(
    BeditViewActivatableInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED(
    BeditModelinePlugin, bedit_modeline_plugin, PEAS_TYPE_EXTENSION_BASE, 0,
    G_IMPLEMENT_INTERFACE_DYNAMIC(
        BEDIT_TYPE_VIEW_ACTIVATABLE, bedit_view_activatable_iface_init)
        G_ADD_PRIVATE_DYNAMIC(BeditModelinePlugin))

static void bedit_modeline_plugin_constructed(GObject *object) {
    gchar *data_dir;

    data_dir = peas_extension_base_get_data_dir(PEAS_EXTENSION_BASE(object));

    modeline_parser_init(data_dir);

    g_free(data_dir);

    G_OBJECT_CLASS(bedit_modeline_plugin_parent_class)->constructed(object);
}

static void bedit_modeline_plugin_init(BeditModelinePlugin *plugin) {
    bedit_debug_message(DEBUG_PLUGINS, "BeditModelinePlugin initializing");

    plugin->priv = bedit_modeline_plugin_get_instance_private(plugin);
}

static void bedit_modeline_plugin_dispose(GObject *object) {
    BeditModelinePlugin *plugin = BEDIT_MODELINE_PLUGIN(object);

    bedit_debug_message(DEBUG_PLUGINS, "BeditModelinePlugin disposing");

    g_clear_object(&plugin->priv->view);

    G_OBJECT_CLASS(bedit_modeline_plugin_parent_class)->dispose(object);
}

static void bedit_modeline_plugin_finalize(GObject *object) {
    bedit_debug_message(DEBUG_PLUGINS, "BeditModelinePlugin finalizing");

    modeline_parser_shutdown();

    G_OBJECT_CLASS(bedit_modeline_plugin_parent_class)->finalize(object);
}

static void bedit_modeline_plugin_set_property(
    GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) {
    BeditModelinePlugin *plugin = BEDIT_MODELINE_PLUGIN(object);

    switch (prop_id) {
    case PROP_VIEW:
        plugin->priv->view = BEDIT_VIEW(g_value_dup_object(value));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void bedit_modeline_plugin_get_property(
    GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) {
    BeditModelinePlugin *plugin = BEDIT_MODELINE_PLUGIN(object);

    switch (prop_id) {
    case PROP_VIEW:
        g_value_set_object(value, plugin->priv->view);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void on_document_loaded_or_saved(
    BeditDocument *document, GtkSourceView *view) {
    modeline_parser_apply_modeline(view);
}

static void bedit_modeline_plugin_activate(BeditViewActivatable *activatable) {
    BeditModelinePlugin *plugin;
    GtkTextBuffer *doc;

    bedit_debug(DEBUG_PLUGINS);

    plugin = BEDIT_MODELINE_PLUGIN(activatable);

    modeline_parser_apply_modeline(GTK_SOURCE_VIEW(plugin->priv->view));

    doc = gtk_text_view_get_buffer(GTK_TEXT_VIEW(plugin->priv->view));

    plugin->priv->document_loaded_handler_id = g_signal_connect(
        doc, "loaded", G_CALLBACK(on_document_loaded_or_saved),
        plugin->priv->view);
    plugin->priv->document_saved_handler_id = g_signal_connect(
        doc, "saved", G_CALLBACK(on_document_loaded_or_saved),
        plugin->priv->view);
}

static void bedit_modeline_plugin_deactivate(
    BeditViewActivatable *activatable) {
    BeditModelinePlugin *plugin;
    GtkTextBuffer *doc;

    bedit_debug(DEBUG_PLUGINS);

    plugin = BEDIT_MODELINE_PLUGIN(activatable);

    doc = gtk_text_view_get_buffer(GTK_TEXT_VIEW(plugin->priv->view));

    g_signal_handler_disconnect(doc, plugin->priv->document_loaded_handler_id);
    g_signal_handler_disconnect(doc, plugin->priv->document_saved_handler_id);
}

static void bedit_modeline_plugin_class_init(BeditModelinePluginClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->constructed = bedit_modeline_plugin_constructed;
    object_class->dispose = bedit_modeline_plugin_dispose;
    object_class->finalize = bedit_modeline_plugin_finalize;
    object_class->set_property = bedit_modeline_plugin_set_property;
    object_class->get_property = bedit_modeline_plugin_get_property;

    g_object_class_override_property(object_class, PROP_VIEW, "view");
}

static void bedit_view_activatable_iface_init(
    BeditViewActivatableInterface *iface) {
    iface->activate = bedit_modeline_plugin_activate;
    iface->deactivate = bedit_modeline_plugin_deactivate;
}

static void bedit_modeline_plugin_class_finalize(
    BeditModelinePluginClass *klass) {}

G_MODULE_EXPORT void peas_register_types(PeasObjectModule *module) {
    bedit_modeline_plugin_register_type(G_TYPE_MODULE(module));

    peas_object_module_register_extension_type(
        module, BEDIT_TYPE_VIEW_ACTIVATABLE, BEDIT_TYPE_MODELINE_PLUGIN);
}

