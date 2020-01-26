
/*
 * bedit-file-browser-message-add-filter.h
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

#ifndef GEDIT_FILE_BROWSER_MESSAGE_ADD_FILTER_H
#define GEDIT_FILE_BROWSER_MESSAGE_ADD_FILTER_H

#include <bedit/bedit-message.h>

G_BEGIN_DECLS

#define GEDIT_TYPE_FILE_BROWSER_MESSAGE_ADD_FILTER            (bedit_file_browser_message_add_filter_get_type ())
#define GEDIT_FILE_BROWSER_MESSAGE_ADD_FILTER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj),\
                                                               GEDIT_TYPE_FILE_BROWSER_MESSAGE_ADD_FILTER,\
                                                               BeditFileBrowserMessageAddFilter))
#define GEDIT_FILE_BROWSER_MESSAGE_ADD_FILTER_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj),\
                                                               GEDIT_TYPE_FILE_BROWSER_MESSAGE_ADD_FILTER,\
                                                               BeditFileBrowserMessageAddFilter const))
#define GEDIT_FILE_BROWSER_MESSAGE_ADD_FILTER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),\
                                                               GEDIT_TYPE_FILE_BROWSER_MESSAGE_ADD_FILTER,\
                                                               BeditFileBrowserMessageAddFilterClass))
#define GEDIT_IS_FILE_BROWSER_MESSAGE_ADD_FILTER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj),\
                                                               GEDIT_TYPE_FILE_BROWSER_MESSAGE_ADD_FILTER))
#define GEDIT_IS_FILE_BROWSER_MESSAGE_ADD_FILTER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),\
                                                               GEDIT_TYPE_FILE_BROWSER_MESSAGE_ADD_FILTER))
#define GEDIT_FILE_BROWSER_MESSAGE_ADD_FILTER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),\
                                                               GEDIT_TYPE_FILE_BROWSER_MESSAGE_ADD_FILTER,\
                                                               BeditFileBrowserMessageAddFilterClass))

typedef struct _BeditFileBrowserMessageAddFilter        BeditFileBrowserMessageAddFilter;
typedef struct _BeditFileBrowserMessageAddFilterClass   BeditFileBrowserMessageAddFilterClass;
typedef struct _BeditFileBrowserMessageAddFilterPrivate BeditFileBrowserMessageAddFilterPrivate;

struct _BeditFileBrowserMessageAddFilter
{
	BeditMessage parent;

	BeditFileBrowserMessageAddFilterPrivate *priv;
};

struct _BeditFileBrowserMessageAddFilterClass
{
	BeditMessageClass parent_class;
};

GType bedit_file_browser_message_add_filter_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* GEDIT_FILE_BROWSER_MESSAGE_ADD_FILTER_H */
