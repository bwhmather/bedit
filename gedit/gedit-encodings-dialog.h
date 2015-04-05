/*
 * gedit-encodings-dialog.h
 * This file is part of gedit
 *
 * Copyright (C) 2003-2005 Paolo Maggi
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

#ifndef __GEDIT_ENCODINGS_DIALOG_H__
#define __GEDIT_ENCODINGS_DIALOG_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GEDIT_TYPE_ENCODINGS_DIALOG              (gedit_encodings_dialog_get_type())
#define GEDIT_ENCODINGS_DIALOG(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), GEDIT_TYPE_ENCODINGS_DIALOG, GeditEncodingsDialog))
#define GEDIT_ENCODINGS_DIALOG_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), GEDIT_TYPE_ENCODINGS_DIALOG, GeditEncodingsDialogClass))
#define GEDIT_IS_ENCODINGS_DIALOG(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), GEDIT_TYPE_ENCODINGS_DIALOG))
#define GEDIT_IS_ENCODINGS_DIALOG_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GEDIT_TYPE_ENCODINGS_DIALOG))
#define GEDIT_ENCODINGS_DIALOG_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), GEDIT_TYPE_ENCODINGS_DIALOG, GeditEncodingsDialogClass))

typedef struct _GeditEncodingsDialog		GeditEncodingsDialog;
typedef struct _GeditEncodingsDialogClass	GeditEncodingsDialogClass;
typedef struct _GeditEncodingsDialogPrivate	GeditEncodingsDialogPrivate;

struct _GeditEncodingsDialog
{
	GtkDialog dialog;

	GeditEncodingsDialogPrivate *priv;
};

struct _GeditEncodingsDialogClass
{
	GtkDialogClass parent_class;
};

GType		 gedit_encodings_dialog_get_type	(void) G_GNUC_CONST;

GtkWidget	*gedit_encodings_dialog_new		(void);

G_END_DECLS

#endif /* __GEDIT_ENCODINGS_DIALOG_H__ */

/* ex:set ts=8 noet: */
