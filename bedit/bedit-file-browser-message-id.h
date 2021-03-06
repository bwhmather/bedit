
/*
 * bedit-file-browser-message-id.h
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

#ifndef BEDIT_FILE_BROWSER_MESSAGE_ID_H
#define BEDIT_FILE_BROWSER_MESSAGE_ID_H

#include "bedit-message.h"

G_BEGIN_DECLS

#define BEDIT_TYPE_FILE_BROWSER_MESSAGE_ID                                  \
    (bedit_file_browser_message_id_get_type())
#define BEDIT_FILE_BROWSER_MESSAGE_ID(obj)                                  \
    (G_TYPE_CHECK_INSTANCE_CAST(                                            \
        (obj), BEDIT_TYPE_FILE_BROWSER_MESSAGE_ID,                          \
        BeditFileBrowserMessageId                                           \
    ))
#define BEDIT_FILE_BROWSER_MESSAGE_ID_CONST(obj)                            \
    (G_TYPE_CHECK_INSTANCE_CAST(                                            \
        (obj), BEDIT_TYPE_FILE_BROWSER_MESSAGE_ID,                          \
        BeditFileBrowserMessageId const                                     \
    ))
#define BEDIT_FILE_BROWSER_MESSAGE_ID_CLASS(klass)                          \
    (G_TYPE_CHECK_CLASS_CAST(                                               \
        (klass), BEDIT_TYPE_FILE_BROWSER_MESSAGE_ID,                        \
        BeditFileBrowserMessageIdClass                                      \
    ))
#define BEDIT_IS_FILE_BROWSER_MESSAGE_ID(obj)                               \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), BEDIT_TYPE_FILE_BROWSER_MESSAGE_ID))
#define BEDIT_IS_FILE_BROWSER_MESSAGE_ID_CLASS(klass)                       \
    (G_TYPE_CHECK_CLASS_TYPE((klass), BEDIT_TYPE_FILE_BROWSER_MESSAGE_ID))
#define BEDIT_FILE_BROWSER_MESSAGE_ID_GET_CLASS(obj)                        \
    (G_TYPE_INSTANCE_GET_CLASS(                                             \
        (obj), BEDIT_TYPE_FILE_BROWSER_MESSAGE_ID,                          \
        BeditFileBrowserMessageIdClass                                      \
    ))

typedef struct _BeditFileBrowserMessageId BeditFileBrowserMessageId;
typedef struct _BeditFileBrowserMessageIdClass BeditFileBrowserMessageIdClass;
typedef struct _BeditFileBrowserMessageIdPrivate
    BeditFileBrowserMessageIdPrivate;

struct _BeditFileBrowserMessageId {
    BeditMessage parent;

    BeditFileBrowserMessageIdPrivate *priv;
};

struct _BeditFileBrowserMessageIdClass {
    BeditMessageClass parent_class;
};

GType bedit_file_browser_message_id_get_type(void) G_GNUC_CONST;

G_END_DECLS

#endif /* BEDIT_FILE_BROWSER_MESSAGE_ID_H */

