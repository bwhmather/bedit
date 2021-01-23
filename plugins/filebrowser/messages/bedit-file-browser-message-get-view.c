/*
 * bedit-file-browser-message-get-view.c
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

#include "bedit-file-browser-message-get-view.h"
#include "plugins/filebrowser/bedit-file-browser-folder-view.h"

enum {
    PROP_0,

    PROP_VIEW,
};

struct _BeditFileBrowserMessageGetViewPrivate {
    BeditFileBrowserFolderView *view;
};

G_DEFINE_TYPE_EXTENDED(
    BeditFileBrowserMessageGetView, bedit_file_browser_message_get_view,
    BEDIT_TYPE_MESSAGE, 0,
    G_ADD_PRIVATE(BeditFileBrowserMessageGetView)
)

static void bedit_file_browser_message_get_view_finalize(GObject *obj) {
    BeditFileBrowserMessageGetView *msg =
        BEDIT_FILE_BROWSER_MESSAGE_GET_VIEW(obj);

    if (msg->priv->view) {
        g_object_unref(msg->priv->view);
    }

    G_OBJECT_CLASS(bedit_file_browser_message_get_view_parent_class)
        ->finalize(obj);
}

static void bedit_file_browser_message_get_view_get_property(
    GObject *obj, guint prop_id, GValue *value, GParamSpec *pspec
) {
    BeditFileBrowserMessageGetView *msg;

    msg = BEDIT_FILE_BROWSER_MESSAGE_GET_VIEW(obj);

    switch (prop_id) {
    case PROP_VIEW:
        g_value_set_object(value, msg->priv->view);
        break;
    }
}

static void bedit_file_browser_message_get_view_set_property(
    GObject *obj, guint prop_id, GValue const *value, GParamSpec *pspec
) {
    BeditFileBrowserMessageGetView *msg;

    msg = BEDIT_FILE_BROWSER_MESSAGE_GET_VIEW(obj);

    switch (prop_id) {
    case PROP_VIEW:
        if (msg->priv->view) {
            g_object_unref(msg->priv->view);
        }
        msg->priv->view = g_value_dup_object(value);
        break;
    }
}

static void bedit_file_browser_message_get_view_class_init(
    BeditFileBrowserMessageGetViewClass *klass
) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = bedit_file_browser_message_get_view_finalize;

    object_class->get_property =
        bedit_file_browser_message_get_view_get_property;
    object_class->set_property =
        bedit_file_browser_message_get_view_set_property;

    g_object_class_install_property(
        object_class, PROP_VIEW,
        g_param_spec_object(
            "view", "View", "View", BEDIT_TYPE_FILE_BROWSER_FOLDER_VIEW,
            G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS
        )
    );
}

static void bedit_file_browser_message_get_view_init(
    BeditFileBrowserMessageGetView *message
) {
    message->priv =
        bedit_file_browser_message_get_view_get_instance_private(message);
}

