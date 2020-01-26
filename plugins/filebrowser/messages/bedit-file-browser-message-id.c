
/*
 * bedit-file-browser-message-id.c
 * This file is part of bedit
 *
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

#include "bedit-file-browser-message-id.h"

enum {
    PROP_0,

    PROP_ID,
};

struct _BeditFileBrowserMessageIdPrivate {
    guint id;
};

G_DEFINE_TYPE_EXTENDED(
    BeditFileBrowserMessageId, bedit_file_browser_message_id,
    GEDIT_TYPE_MESSAGE, 0, G_ADD_PRIVATE(BeditFileBrowserMessageId))

static void bedit_file_browser_message_id_get_property(
    GObject *obj, guint prop_id, GValue *value, GParamSpec *pspec) {
    BeditFileBrowserMessageId *msg;

    msg = GEDIT_FILE_BROWSER_MESSAGE_ID(obj);

    switch (prop_id) {
    case PROP_ID:
        g_value_set_uint(value, msg->priv->id);
        break;
    }
}

static void bedit_file_browser_message_id_set_property(
    GObject *obj, guint prop_id, GValue const *value, GParamSpec *pspec) {
    BeditFileBrowserMessageId *msg;

    msg = GEDIT_FILE_BROWSER_MESSAGE_ID(obj);

    switch (prop_id) {
    case PROP_ID:
        msg->priv->id = g_value_get_uint(value);
        break;
    }
}

static void bedit_file_browser_message_id_class_init(
    BeditFileBrowserMessageIdClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->get_property = bedit_file_browser_message_id_get_property;
    object_class->set_property = bedit_file_browser_message_id_set_property;

    g_object_class_install_property(
        object_class, PROP_ID,
        g_param_spec_uint(
            "id", "Id", "Id", 0, G_MAXUINT, 0,
            G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));
}

static void bedit_file_browser_message_id_init(
    BeditFileBrowserMessageId *message) {
    message->priv = bedit_file_browser_message_id_get_instance_private(message);
}
