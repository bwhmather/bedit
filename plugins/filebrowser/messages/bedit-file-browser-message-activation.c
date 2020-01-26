
/*
 * bedit-file-browser-message-activation.c
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

#include "bedit-file-browser-message-activation.h"

enum {
    PROP_0,

    PROP_ACTIVE,
};

struct _BeditFileBrowserMessageActivationPrivate {
    gboolean active;
};

G_DEFINE_TYPE_EXTENDED(
    BeditFileBrowserMessageActivation, bedit_file_browser_message_activation,
    GEDIT_TYPE_MESSAGE, 0, G_ADD_PRIVATE(BeditFileBrowserMessageActivation))

static void bedit_file_browser_message_activation_get_property(
    GObject *obj, guint prop_id, GValue *value, GParamSpec *pspec) {
    BeditFileBrowserMessageActivation *msg;

    msg = GEDIT_FILE_BROWSER_MESSAGE_ACTIVATION(obj);

    switch (prop_id) {
    case PROP_ACTIVE:
        g_value_set_boolean(value, msg->priv->active);
        break;
    }
}

static void bedit_file_browser_message_activation_set_property(
    GObject *obj, guint prop_id, GValue const *value, GParamSpec *pspec) {
    BeditFileBrowserMessageActivation *msg;

    msg = GEDIT_FILE_BROWSER_MESSAGE_ACTIVATION(obj);

    switch (prop_id) {
    case PROP_ACTIVE:
        msg->priv->active = g_value_get_boolean(value);
        break;
    }
}

static void bedit_file_browser_message_activation_class_init(
    BeditFileBrowserMessageActivationClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->get_property =
        bedit_file_browser_message_activation_get_property;
    object_class->set_property =
        bedit_file_browser_message_activation_set_property;

    g_object_class_install_property(
        object_class, PROP_ACTIVE,
        g_param_spec_boolean(
            "active", "Active", "Active", FALSE,
            G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));
}

static void bedit_file_browser_message_activation_init(
    BeditFileBrowserMessageActivation *message) {
    message->priv =
        bedit_file_browser_message_activation_get_instance_private(message);
}
