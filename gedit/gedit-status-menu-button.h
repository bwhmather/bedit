/*
 * gedit-status-menu-button.h
 * This file is part of gedit
 *
 * Copyright (C) 2008 - Jesse van den Kieboom
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

#ifndef __GEDIT_STATUS_MENU_BUTTON_H__
#define __GEDIT_STATUS_MENU_BUTTON_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GEDIT_TYPE_STATUS_MENU_BUTTON			(gedit_status_menu_button_get_type ())
#define GEDIT_STATUS_MENU_BUTTON(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), GEDIT_TYPE_STATUS_MENU_BUTTON, GeditStatusMenuButton))
#define GEDIT_STATUS_MENU_BUTTON_CONST(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GEDIT_TYPE_STATUS_MENU_BUTTON, GeditStatusMenuButton const))
#define GEDIT_STATUS_MENU_BUTTON_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), GEDIT_TYPE_STATUS_MENU_BUTTON, GeditStatusMenuButtonClass))
#define GEDIT_IS_STATUS_MENU_BUTTON(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GEDIT_TYPE_STATUS_MENU_BUTTON))
#define GEDIT_IS_STATUS_MENU_BUTTON_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GEDIT_TYPE_STATUS_MENU_BUTTON))
#define GEDIT_STATUS_MENU_BUTTON_GET_CLASS(obj)		(G_TYPE_INSTANCE_GET_CLASS ((obj), GEDIT_TYPE_STATUS_MENU_BUTTON, GeditStatusMenuButtonClass))

typedef struct _GeditStatusMenuButton			GeditStatusMenuButton;
typedef struct _GeditStatusMenuButtonPrivate		GeditStatusMenuButtonPrivate;
typedef struct _GeditStatusMenuButtonClass		GeditStatusMenuButtonClass;
typedef struct _GeditStatusMenuButtonClassPrivate	GeditStatusMenuButtonClassPrivate;

struct _GeditStatusMenuButton
{
	GtkMenuButton parent;

	GeditStatusMenuButtonPrivate *priv;
};

struct _GeditStatusMenuButtonClass
{
	GtkMenuButtonClass parent_class;

	GeditStatusMenuButtonClassPrivate *priv;
};

GType gedit_status_menu_button_get_type 	(void) G_GNUC_CONST;

GtkWidget *gedit_status_menu_button_new		(void);

void gedit_status_menu_button_set_label		(GeditStatusMenuButton *button,
						 const gchar           *label);

const gchar *gedit_status_menu_button_get_label (GeditStatusMenuButton *button);

G_END_DECLS

#endif /* __GEDIT_STATUS_MENU_BUTTON_H__ */

/* ex:set ts=8 noet: */
