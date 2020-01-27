
/*
 * bedit-file-browser-message-id-location.c
 * This file is part of bedit
 *
 * Copyright (C) 2013 - Garrett Regier
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

#include "bedit-file-browser-message-id-location.h"
#include "gio/gio.h"

enum {
    PROP_0,

    PROP_ID,
    PROP_NAME,
    PROP_LOCATION,
    PROP_IS_DIRECTORY,
};

struct _BeditFileBrowserMessageIdLocationPrivate {
    gchar *id;
    gchar *name;
    GFile *location;
    gboolean is_directory;
};

G_DEFINE_TYPE_EXTENDED(
    BeditFileBrowserMessageIdLocation, bedit_file_browser_message_id_location,
    BEDIT_TYPE_MESSAGE, 0, G_ADD_PRIVATE(BeditFileBrowserMessageIdLocation))

static void bedit_file_browser_message_id_location_finalize(GObject *obj) {
    BeditFileBrowserMessageIdLocation *msg =
        BEDIT_FILE_BROWSER_MESSAGE_ID_LOCATION(obj);

    g_free(msg->priv->id);
    g_free(msg->priv->name);
    if (msg->priv->location) {
        g_object_unref(msg->priv->location);
    }

    G_OBJECT_CLASS(bedit_file_browser_message_id_location_parent_class)
        ->finalize(obj);
}

static void bedit_file_browser_message_id_location_get_property(
    GObject *obj, guint prop_id, GValue *value, GParamSpec *pspec) {
    BeditFileBrowserMessageIdLocation *msg;

    msg = BEDIT_FILE_BROWSER_MESSAGE_ID_LOCATION(obj);

    switch (prop_id) {
    case PROP_ID:
        g_value_set_string(value, msg->priv->id);
        break;
    case PROP_NAME:
        g_value_set_string(value, msg->priv->name);
        break;
    case PROP_LOCATION:
        g_value_set_object(value, msg->priv->location);
        break;
    case PROP_IS_DIRECTORY:
        g_value_set_boolean(value, msg->priv->is_directory);
        break;
    }
}

static void bedit_file_browser_message_id_location_set_property(
    GObject *obj, guint prop_id, GValue const *value, GParamSpec *pspec) {
    BeditFileBrowserMessageIdLocation *msg;

    msg = BEDIT_FILE_BROWSER_MESSAGE_ID_LOCATION(obj);

    switch (prop_id) {
    case PROP_ID: {
        g_free(msg->priv->id);
        msg->priv->id = g_value_dup_string(value);
        break;
    }
    case PROP_NAME: {
        g_free(msg->priv->name);
        msg->priv->name = g_value_dup_string(value);
        break;
    }
    case PROP_LOCATION: {
        if (msg->priv->location) {
            g_object_unref(msg->priv->location);
        }
        msg->priv->location = g_value_dup_object(value);
        break;
    }
    case PROP_IS_DIRECTORY:
        msg->priv->is_directory = g_value_get_boolean(value);
        break;
    }
}

static void bedit_file_browser_message_id_location_class_init(
    BeditFileBrowserMessageIdLocationClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = bedit_file_browser_message_id_location_finalize;

    object_class->get_property =
        bedit_file_browser_message_id_location_get_property;
    object_class->set_property =
        bedit_file_browser_message_id_location_set_property;

    g_object_class_install_property(
        object_class, PROP_ID,
        g_param_spec_string(
            "id", "Id", "Id", NULL,
            G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(
        object_class, PROP_NAME,
        g_param_spec_string(
            "name", "Name", "Name", NULL,
            G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(
        object_class, PROP_LOCATION,
        g_param_spec_object(
            "location", "Location", "Location", G_TYPE_FILE,
            G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(
        object_class, PROP_IS_DIRECTORY,
        g_param_spec_boolean(
            "is-directory", "Is Directory", "Is Directory", FALSE,
            G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));
}

static void bedit_file_browser_message_id_location_init(
    BeditFileBrowserMessageIdLocation *message) {
    message->priv =
        bedit_file_browser_message_id_location_get_instance_private(message);
}
