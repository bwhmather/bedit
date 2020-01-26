
/*
 * bedit-file-browser-message-set-emblem.c
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

#include "bedit-file-browser-message-set-emblem.h"

enum
{
	PROP_0,

	PROP_ID,
	PROP_EMBLEM,
};

struct _BeditFileBrowserMessageSetEmblemPrivate
{
	gchar *id;
	gchar *emblem;
};

G_DEFINE_TYPE_EXTENDED (BeditFileBrowserMessageSetEmblem,
                        bedit_file_browser_message_set_emblem,
                        GEDIT_TYPE_MESSAGE,
                        0,
                        G_ADD_PRIVATE (BeditFileBrowserMessageSetEmblem))

static void
bedit_file_browser_message_set_emblem_finalize (GObject *obj)
{
	BeditFileBrowserMessageSetEmblem *msg = GEDIT_FILE_BROWSER_MESSAGE_SET_EMBLEM (obj);

	g_free (msg->priv->id);
	g_free (msg->priv->emblem);

	G_OBJECT_CLASS (bedit_file_browser_message_set_emblem_parent_class)->finalize (obj);
}

static void
bedit_file_browser_message_set_emblem_get_property (GObject    *obj,
                                                    guint       prop_id,
                                                    GValue     *value,
                                                    GParamSpec *pspec)
{
	BeditFileBrowserMessageSetEmblem *msg;

	msg = GEDIT_FILE_BROWSER_MESSAGE_SET_EMBLEM (obj);

	switch (prop_id)
	{
		case PROP_ID:
			g_value_set_string (value, msg->priv->id);
			break;
		case PROP_EMBLEM:
			g_value_set_string (value, msg->priv->emblem);
			break;
	}
}

static void
bedit_file_browser_message_set_emblem_set_property (GObject      *obj,
                                                    guint         prop_id,
                                                    GValue const *value,
                                                    GParamSpec   *pspec)
{
	BeditFileBrowserMessageSetEmblem *msg;

	msg = GEDIT_FILE_BROWSER_MESSAGE_SET_EMBLEM (obj);

	switch (prop_id)
	{
		case PROP_ID:
		{
			g_free (msg->priv->id);
			msg->priv->id = g_value_dup_string (value);
			break;
		}
		case PROP_EMBLEM:
		{
			g_free (msg->priv->emblem);
			msg->priv->emblem = g_value_dup_string (value);
			break;
		}
	}
}

static void
bedit_file_browser_message_set_emblem_class_init (BeditFileBrowserMessageSetEmblemClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	object_class->finalize = bedit_file_browser_message_set_emblem_finalize;

	object_class->get_property = bedit_file_browser_message_set_emblem_get_property;
	object_class->set_property = bedit_file_browser_message_set_emblem_set_property;

	g_object_class_install_property (object_class,
	                                 PROP_ID,
	                                 g_param_spec_string ("id",
	                                                      "Id",
	                                                      "Id",
	                                                      NULL,
	                                                      G_PARAM_READWRITE |
	                                                      G_PARAM_CONSTRUCT |
	                                                      G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (object_class,
	                                 PROP_EMBLEM,
	                                 g_param_spec_string ("emblem",
	                                                      "Emblem",
	                                                      "Emblem",
	                                                      NULL,
	                                                      G_PARAM_READWRITE |
	                                                      G_PARAM_CONSTRUCT |
	                                                      G_PARAM_STATIC_STRINGS));
}

static void
bedit_file_browser_message_set_emblem_init (BeditFileBrowserMessageSetEmblem *message)
{
	message->priv = bedit_file_browser_message_set_emblem_get_instance_private (message);
}
