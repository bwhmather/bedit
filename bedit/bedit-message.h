/*
 * bedit-message.h
 * This file is part of bedit
 *
 * Copyright (C) 2008, 2010 - Jesse van den Kieboom
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

#ifndef GEDIT_MESSAGE_H
#define GEDIT_MESSAGE_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GEDIT_TYPE_MESSAGE (bedit_message_get_type())
#define GEDIT_MESSAGE(obj)                                                     \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GEDIT_TYPE_MESSAGE, BeditMessage))
#define GEDIT_MESSAGE_CONST(obj)                                               \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GEDIT_TYPE_MESSAGE, BeditMessage const))
#define GEDIT_MESSAGE_CLASS(klass)                                             \
    (G_TYPE_CHECK_CLASS_CAST((klass), GEDIT_TYPE_MESSAGE, BeditMessageClass))
#define GEDIT_IS_MESSAGE(obj)                                                  \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GEDIT_TYPE_MESSAGE))
#define GEDIT_IS_MESSAGE_CLASS(klass)                                          \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GEDIT_TYPE_MESSAGE))
#define GEDIT_MESSAGE_GET_CLASS(obj)                                           \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GEDIT_TYPE_MESSAGE, BeditMessageClass))

typedef struct _BeditMessage BeditMessage;
typedef struct _BeditMessageClass BeditMessageClass;
typedef struct _BeditMessagePrivate BeditMessagePrivate;

struct _BeditMessage {
    GObject parent;

    BeditMessagePrivate *priv;
};

struct _BeditMessageClass {
    GObjectClass parent_class;
};

GType bedit_message_get_type(void) G_GNUC_CONST;

const gchar *bedit_message_get_object_path(BeditMessage *message);
const gchar *bedit_message_get_method(BeditMessage *message);

gboolean bedit_message_type_has(GType gtype, const gchar *propname);

gboolean bedit_message_type_check(
    GType gtype, const gchar *propname, GType value_type);

gboolean bedit_message_has(BeditMessage *message, const gchar *propname);

gboolean bedit_message_is_valid_object_path(const gchar *object_path);
gchar *bedit_message_type_identifier(
    const gchar *object_path, const gchar *method);

G_END_DECLS

#endif /* GEDIT_MESSAGE_H */

