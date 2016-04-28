
/*
 * gedit-file-browser-message-set-markup.h
 * This file is part of gedit
 *
 * Copyright (C) 2013 - Garrett Regier
 * Copyright (C) 2014 - Jesse van den Kieboom
 *
 * gedit is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * gedit is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with gedit; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#ifndef GEDIT_FILE_BROWSER_MESSAGE_SET_MARKUP_H
#define GEDIT_FILE_BROWSER_MESSAGE_SET_MARKUP_H

#include <gedit/gedit-message.h>

G_BEGIN_DECLS

#define GEDIT_TYPE_FILE_BROWSER_MESSAGE_SET_MARKUP            (gedit_file_browser_message_set_markup_get_type ())
#define GEDIT_FILE_BROWSER_MESSAGE_SET_MARKUP(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj),\
                                                               GEDIT_TYPE_FILE_BROWSER_MESSAGE_SET_MARKUP,\
                                                               GeditFileBrowserMessageSetMarkup))
#define GEDIT_FILE_BROWSER_MESSAGE_SET_MARKUP_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj),\
                                                               GEDIT_TYPE_FILE_BROWSER_MESSAGE_SET_MARKUP,\
                                                               GeditFileBrowserMessageSetMarkup const))
#define GEDIT_FILE_BROWSER_MESSAGE_SET_MARKUP_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),\
                                                               GEDIT_TYPE_FILE_BROWSER_MESSAGE_SET_MARKUP,\
                                                               GeditFileBrowserMessageSetMarkupClass))
#define GEDIT_IS_FILE_BROWSER_MESSAGE_SET_MARKUP(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj),\
                                                               GEDIT_TYPE_FILE_BROWSER_MESSAGE_SET_MARKUP))
#define GEDIT_IS_FILE_BROWSER_MESSAGE_SET_MARKUP_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),\
                                                               GEDIT_TYPE_FILE_BROWSER_MESSAGE_SET_MARKUP))
#define GEDIT_FILE_BROWSER_MESSAGE_SET_MARKUP_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),\
                                                               GEDIT_TYPE_FILE_BROWSER_MESSAGE_SET_MARKUP,\
                                                               GeditFileBrowserMessageSetMarkupClass))

typedef struct _GeditFileBrowserMessageSetMarkup        GeditFileBrowserMessageSetMarkup;
typedef struct _GeditFileBrowserMessageSetMarkupClass   GeditFileBrowserMessageSetMarkupClass;
typedef struct _GeditFileBrowserMessageSetMarkupPrivate GeditFileBrowserMessageSetMarkupPrivate;

struct _GeditFileBrowserMessageSetMarkup
{
	GeditMessage parent;

	GeditFileBrowserMessageSetMarkupPrivate *priv;
};

struct _GeditFileBrowserMessageSetMarkupClass
{
	GeditMessageClass parent_class;
};

GType gedit_file_browser_message_set_markup_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* GEDIT_FILE_BROWSER_MESSAGE_SET_MARKUP_H */
