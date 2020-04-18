/*
 * bedit-file-browser-message-extend-context-menu.h
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

#ifndef BEDIT_FILE_BROWSER_MESSAGE_EXTEND_CONTEXT_MENU_H
#define BEDIT_FILE_BROWSER_MESSAGE_EXTEND_CONTEXT_MENU_H

#include <bedit/bedit-message.h>

G_BEGIN_DECLS

#define BEDIT_TYPE_FILE_BROWSER_MESSAGE_EXTEND_CONTEXT_MENU                 \
    (bedit_file_browser_message_extend_context_menu_get_type())
#define BEDIT_FILE_BROWSER_MESSAGE_EXTEND_CONTEXT_MENU(obj)                 \
    (G_TYPE_CHECK_INSTANCE_CAST(                                            \
        (obj), BEDIT_TYPE_FILE_BROWSER_MESSAGE_EXTEND_CONTEXT_MENU,         \
        BeditFileBrowserMessageExtendContextMenu                            \
    ))
#define BEDIT_FILE_BROWSER_MESSAGE_EXTEND_CONTEXT_MENU_CONST(obj)           \
    (G_TYPE_CHECK_INSTANCE_CAST(                                            \
        (obj), BEDIT_TYPE_FILE_BROWSER_MESSAGE_EXTEND_CONTEXT_MENU,         \
        BeditFileBrowserMessageExtendContextMenu const                      \
    ))
#define BEDIT_FILE_BROWSER_MESSAGE_EXTEND_CONTEXT_MENU_CLASS(klass)         \
    (G_TYPE_CHECK_CLASS_CAST(                                               \
        (klass), BEDIT_TYPE_FILE_BROWSER_MESSAGE_EXTEND_CONTEXT_MENU,       \
        BeditFileBrowserMessageExtendContextMenuClass                       \
    ))
#define BEDIT_IS_FILE_BROWSER_MESSAGE_EXTEND_CONTEXT_MENU(obj)              \
    (G_TYPE_CHECK_INSTANCE_TYPE(                                            \
        (obj), BEDIT_TYPE_FILE_BROWSER_MESSAGE_EXTEND_CONTEXT_MENU          \
    ))
#define BEDIT_IS_FILE_BROWSER_MESSAGE_EXTEND_CONTEXT_MENU_CLASS(klass)      \
    (G_TYPE_CHECK_CLASS_TYPE(                                               \
        (klass), BEDIT_TYPE_FILE_BROWSER_MESSAGE_EXTEND_CONTEXT_MENU        \
    ))
#define BEDIT_FILE_BROWSER_MESSAGE_EXTEND_CONTEXT_MENU_GET_CLASS(obj)       \
    (G_TYPE_INSTANCE_GET_CLASS(                                             \
        (obj), BEDIT_TYPE_FILE_BROWSER_MESSAGE_EXTEND_CONTEXT_MENU,         \
        BeditFileBrowserMessageExtendContextMenuClass                       \
    ))

typedef struct _BeditFileBrowserMessageExtendContextMenu
    BeditFileBrowserMessageExtendContextMenu;
typedef struct _BeditFileBrowserMessageExtendContextMenuClass
    BeditFileBrowserMessageExtendContextMenuClass;
typedef struct _BeditFileBrowserMessageExtendContextMenuPrivate
    BeditFileBrowserMessageExtendContextMenuPrivate;

struct _BeditFileBrowserMessageExtendContextMenu {
    BeditMessage parent;

    BeditFileBrowserMessageExtendContextMenuPrivate *priv;
};

struct _BeditFileBrowserMessageExtendContextMenuClass {
    BeditMessageClass parent_class;
};

GType bedit_file_browser_message_extend_context_menu_get_type(
    void
) G_GNUC_CONST;

G_END_DECLS

#endif /* BEDIT_FILE_BROWSER_MESSAGE_EXTEND_CONTEXT_MENU_H */

