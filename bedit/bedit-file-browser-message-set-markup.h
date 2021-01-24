
/*
 * bedit-file-browser-message-set-markup.h
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

#ifndef BEDIT_FILE_BROWSER_MESSAGE_SET_MARKUP_H
#define BEDIT_FILE_BROWSER_MESSAGE_SET_MARKUP_H

#include <bedit/bedit-message.h>

G_BEGIN_DECLS

#define BEDIT_TYPE_FILE_BROWSER_MESSAGE_SET_MARKUP                          \
    (bedit_file_browser_message_set_markup_get_type())
#define BEDIT_FILE_BROWSER_MESSAGE_SET_MARKUP(obj)                          \
    (G_TYPE_CHECK_INSTANCE_CAST(                                            \
        (obj), BEDIT_TYPE_FILE_BROWSER_MESSAGE_SET_MARKUP,                  \
        BeditFileBrowserMessageSetMarkup                                    \
    ))
#define BEDIT_FILE_BROWSER_MESSAGE_SET_MARKUP_CONST(obj)                    \
    (G_TYPE_CHECK_INSTANCE_CAST(                                            \
        (obj), BEDIT_TYPE_FILE_BROWSER_MESSAGE_SET_MARKUP,                  \
        BeditFileBrowserMessageSetMarkup const                              \
    ))
#define BEDIT_FILE_BROWSER_MESSAGE_SET_MARKUP_CLASS(klass)                  \
    (G_TYPE_CHECK_CLASS_CAST(                                               \
        (klass), BEDIT_TYPE_FILE_BROWSER_MESSAGE_SET_MARKUP,                \
        BeditFileBrowserMessageSetMarkupClass                               \
    ))
#define BEDIT_IS_FILE_BROWSER_MESSAGE_SET_MARKUP(obj)                       \
    (G_TYPE_CHECK_INSTANCE_TYPE(                                            \
        (obj), BEDIT_TYPE_FILE_BROWSER_MESSAGE_SET_MARKUP                   \
    ))
#define BEDIT_IS_FILE_BROWSER_MESSAGE_SET_MARKUP_CLASS(klass)               \
    (G_TYPE_CHECK_CLASS_TYPE(                                               \
        (klass), BEDIT_TYPE_FILE_BROWSER_MESSAGE_SET_MARKUP                 \
    ))
#define BEDIT_FILE_BROWSER_MESSAGE_SET_MARKUP_GET_CLASS(obj)                \
    (G_TYPE_INSTANCE_GET_CLASS(                                             \
        (obj), BEDIT_TYPE_FILE_BROWSER_MESSAGE_SET_MARKUP,                  \
        BeditFileBrowserMessageSetMarkupClass                               \
    ))

typedef struct _BeditFileBrowserMessageSetMarkup
    BeditFileBrowserMessageSetMarkup;
typedef struct _BeditFileBrowserMessageSetMarkupClass
    BeditFileBrowserMessageSetMarkupClass;
typedef struct _BeditFileBrowserMessageSetMarkupPrivate
    BeditFileBrowserMessageSetMarkupPrivate;

struct _BeditFileBrowserMessageSetMarkup {
    BeditMessage parent;

    BeditFileBrowserMessageSetMarkupPrivate *priv;
};

struct _BeditFileBrowserMessageSetMarkupClass {
    BeditMessageClass parent_class;
};

GType bedit_file_browser_message_set_markup_get_type(void) G_GNUC_CONST;

G_END_DECLS

#endif /* BEDIT_FILE_BROWSER_MESSAGE_SET_MARKUP_H */

