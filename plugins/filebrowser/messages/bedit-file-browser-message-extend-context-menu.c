
/*
 * bedit-file-browser-message-extend-context-menu.c
 * This file is part of bedit
 *
 * Copyright (C) 2014 - Paolo Borelli
 * Copyright (C) 2014 - Jesse van den Kieboom
 *
 * bedit is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * bedit is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with bedit; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#include "config.h"

#include <bedit/bedit-menu-extension.h>
#include "bedit-file-browser-message-extend-context-menu.h"

enum {
    PROP_0,

    PROP_EXTENSION,
};

struct _BeditFileBrowserMessageExtendContextMenuPrivate {
    BeditMenuExtension *extension;
};

G_DEFINE_TYPE_EXTENDED(
    BeditFileBrowserMessageExtendContextMenu,
    bedit_file_browser_message_extend_context_menu, BEDIT_TYPE_MESSAGE, 0,
    G_ADD_PRIVATE(BeditFileBrowserMessageExtendContextMenu))

static void bedit_file_browser_message_extend_context_menu_finalize(
    GObject *obj) {
    BeditFileBrowserMessageExtendContextMenu *msg =
        BEDIT_FILE_BROWSER_MESSAGE_EXTEND_CONTEXT_MENU(obj);

    if (msg->priv->extension) {
        g_object_unref(msg->priv->extension);
    }

    G_OBJECT_CLASS(bedit_file_browser_message_extend_context_menu_parent_class)
        ->finalize(obj);
}

static void bedit_file_browser_message_extend_context_menu_get_property(
    GObject *obj, guint prop_id, GValue *value, GParamSpec *pspec) {
    BeditFileBrowserMessageExtendContextMenu *msg;

    msg = BEDIT_FILE_BROWSER_MESSAGE_EXTEND_CONTEXT_MENU(obj);

    switch (prop_id) {
    case PROP_EXTENSION:
        g_value_set_object(value, msg->priv->extension);
        break;
    }
}

static void bedit_file_browser_message_extend_context_menu_set_property(
    GObject *obj, guint prop_id, GValue const *value, GParamSpec *pspec) {
    BeditFileBrowserMessageExtendContextMenu *msg;

    msg = BEDIT_FILE_BROWSER_MESSAGE_EXTEND_CONTEXT_MENU(obj);

    switch (prop_id) {
    case PROP_EXTENSION: {
        if (msg->priv->extension) {
            g_object_unref(msg->priv->extension);
        }
        msg->priv->extension = g_value_dup_object(value);
        break;
    }
    }
}

static void bedit_file_browser_message_extend_context_menu_class_init(
    BeditFileBrowserMessageExtendContextMenuClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize =
        bedit_file_browser_message_extend_context_menu_finalize;

    object_class->get_property =
        bedit_file_browser_message_extend_context_menu_get_property;
    object_class->set_property =
        bedit_file_browser_message_extend_context_menu_set_property;

    g_object_class_install_property(
        object_class, PROP_EXTENSION,
        g_param_spec_object(
            "extension", "Extension", "Extension", BEDIT_TYPE_MENU_EXTENSION,
            G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));
}

static void bedit_file_browser_message_extend_context_menu_init(
    BeditFileBrowserMessageExtendContextMenu *message) {
    message->priv =
        bedit_file_browser_message_extend_context_menu_get_instance_private(
            message);
}