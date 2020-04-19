/*
 * bedit-message-bus.h
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * Based on gedit-message-bus.h from Gedit.
 *
 * Copyright (C) 2008-2011 - Jesse van den Kieboom
 * Copyright (C) 2010 - Garrett Regier, Steve Frécinaux
 * Copyright (C) 2016 - Sébastien Wilmet
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

#ifndef BEDIT_MESSAGE_BUS_H
#define BEDIT_MESSAGE_BUS_H

#include <bedit/bedit-message.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define BEDIT_TYPE_MESSAGE_BUS (bedit_message_bus_get_type())
#define BEDIT_MESSAGE_BUS(obj)                                              \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), BEDIT_TYPE_MESSAGE_BUS, BeditMessageBus))
#define BEDIT_MESSAGE_BUS_CONST(obj)                                        \
    (G_TYPE_CHECK_INSTANCE_CAST(                                            \
        (obj), BEDIT_TYPE_MESSAGE_BUS, BeditMessageBus const                \
    ))
#define BEDIT_MESSAGE_BUS_CLASS(klass)                                      \
    (G_TYPE_CHECK_CLASS_CAST(                                               \
        (klass), BEDIT_TYPE_MESSAGE_BUS, BeditMessageBusClass               \
    ))
#define BEDIT_IS_MESSAGE_BUS(obj)                                           \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), BEDIT_TYPE_MESSAGE_BUS))
#define BEDIT_IS_MESSAGE_BUS_CLASS(klass)                                   \
    (G_TYPE_CHECK_CLASS_TYPE((klass), BEDIT_TYPE_MESSAGE_BUS))
#define BEDIT_MESSAGE_BUS_GET_CLASS(obj)                                    \
    (G_TYPE_INSTANCE_GET_CLASS(                                             \
        (obj), BEDIT_TYPE_MESSAGE_BUS, BeditMessageBusClass                 \
    ))

typedef struct _BeditMessageBus BeditMessageBus;
typedef struct _BeditMessageBusClass BeditMessageBusClass;
typedef struct _BeditMessageBusPrivate BeditMessageBusPrivate;

struct _BeditMessageBus {
    GObject parent;

    BeditMessageBusPrivate *priv;
};

struct _BeditMessageBusClass {
    GObjectClass parent_class;

    void (*dispatch)(BeditMessageBus *bus, BeditMessage *message);
    void (*registered)(
        BeditMessageBus *bus, const gchar *object_path, const gchar *method
    );
    void (*unregistered)(
        BeditMessageBus *bus, const gchar *object_path, const gchar *method
    );
};

typedef void (*BeditMessageCallback)(
    BeditMessageBus *bus, BeditMessage *message, gpointer user_data
);

typedef void (*BeditMessageBusForeach)(
    gchar const *object_path, gchar const *method, gpointer user_data
);

GType bedit_message_bus_get_type(void) G_GNUC_CONST;

BeditMessageBus *bedit_message_bus_get_default(void);
BeditMessageBus *bedit_message_bus_new(void);

GType bedit_message_bus_lookup(
    BeditMessageBus *bus, const gchar *object_path, const gchar *method
);

void bedit_message_bus_register(
    BeditMessageBus *bus, GType message_type,
    const gchar *object_path, const gchar *method
);

void bedit_message_bus_unregister(
    BeditMessageBus *bus, const gchar *object_path, const gchar *method
);

void bedit_message_bus_unregister_all(
    BeditMessageBus *bus, const gchar *object_path
);

gboolean bedit_message_bus_is_registered(
    BeditMessageBus *bus, const gchar *object_path, const gchar *method
);

void bedit_message_bus_foreach(
    BeditMessageBus *bus, BeditMessageBusForeach func, gpointer user_data
);

guint bedit_message_bus_connect(
    BeditMessageBus *bus, const gchar *object_path, const gchar *method,
    BeditMessageCallback callback, gpointer user_data,
    GDestroyNotify destroy_data
);

void bedit_message_bus_disconnect(BeditMessageBus *bus, guint id);

void bedit_message_bus_disconnect_by_func(
    BeditMessageBus *bus, const gchar *object_path, const gchar *method,
    BeditMessageCallback callback, gpointer user_data
);

void bedit_message_bus_block(BeditMessageBus *bus, guint id);
void bedit_message_bus_block_by_func(
    BeditMessageBus *bus, const gchar *object_path, const gchar *method,
    BeditMessageCallback callback, gpointer user_data
);

void bedit_message_bus_unblock(BeditMessageBus *bus, guint id);
void bedit_message_bus_unblock_by_func(
    BeditMessageBus *bus, const gchar *object_path, const gchar *method,
    BeditMessageCallback callback, gpointer user_data
);

void bedit_message_bus_send_message(
    BeditMessageBus *bus, BeditMessage *message
);
void bedit_message_bus_send_message_sync(
    BeditMessageBus *bus, BeditMessage *message
);

void bedit_message_bus_send(
    BeditMessageBus *bus, const gchar *object_path, const gchar *method,
    const gchar *first_property, ...
) G_GNUC_NULL_TERMINATED;

BeditMessage *bedit_message_bus_send_sync(
    BeditMessageBus *bus, const gchar *object_path, const gchar *method,
    const gchar *first_property, ...
) G_GNUC_NULL_TERMINATED;

G_END_DECLS

#endif /* BEDIT_MESSAGE_BUS_H */

