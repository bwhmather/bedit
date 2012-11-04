/*
 * gedit-open-tool-button.h
 * This file is part of gedit
 *
 * Copyright (C) 2012 - Paolo Borelli
 *
 * gedit is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * gedit is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __GEDIT_OPEN_TOOL_BUTTON_H__
#define __GEDIT_OPEN_TOOL_BUTTON_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GEDIT_TYPE_OPEN_TOOL_BUTTON		(gedit_open_tool_button_get_type ())
#define GEDIT_OPEN_TOOL_BUTTON(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GEDIT_TYPE_OPEN_TOOL_BUTTON, GeditOpenToolButton))
#define GEDIT_OPEN_TOOL_BUTTON_CONST(obj)	(G_TYPE_CHECK_INSTANCE_CAST ((obj), GEDIT_TYPE_OPEN_TOOL_BUTTON, GeditOpenToolButton const))
#define GEDIT_OPEN_TOOL_BUTTON_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), GEDIT_TYPE_OPEN_TOOL_BUTTON, GeditOpenToolButtonClass))
#define GEDIT_IS_OPEN_TOOL_BUTTON(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GEDIT_TYPE_OPEN_TOOL_BUTTON))
#define GEDIT_IS_OPEN_TOOL_BUTTON_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GEDIT_TYPE_OPEN_TOOL_BUTTON))
#define GEDIT_OPEN_TOOL_BUTTON_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), GEDIT_TYPE_OPEN_TOOL_BUTTON, GeditOpenToolButtonClass))

typedef struct _GeditOpenToolButton		GeditOpenToolButton;
typedef struct _GeditOpenToolButtonClass	GeditOpenToolButtonClass;
typedef struct _GeditOpenToolButtonPrivate	GeditOpenToolButtonPrivate;

struct _GeditOpenToolButton
{
	GtkMenuToolButton parent;

	GeditOpenToolButtonPrivate *priv;
};

struct _GeditOpenToolButtonClass
{
	GtkMenuToolButtonClass parent_class;
};

GType		  gedit_open_tool_button_get_type (void) G_GNUC_CONST;

GtkToolItem	 *gedit_open_tool_button_new      (void);

G_END_DECLS

#endif /* __GEDIT_OPEN_TOOL_BUTTON_H__ */
/* ex:set ts=8 noet: */
