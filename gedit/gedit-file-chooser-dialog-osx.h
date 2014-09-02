/*
 * gedit-file-chooser-dialog-gtk.h
 * This file is part of gedit
 *
 * Copyright (C) 2005 - Paolo Maggi
 * Copyright (C) 2014 - Jesse van den Kieboom
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

#ifndef __GEDIT_FILE_CHOOSER_DIALOG_OSX_H__
#define __GEDIT_FILE_CHOOSER_DIALOG_OSX_H__

#include <gtk/gtk.h>
#include "gedit-file-chooser-dialog.h"

G_BEGIN_DECLS

#define GEDIT_TYPE_FILE_CHOOSER_DIALOG_OSX             (gedit_file_chooser_dialog_osx_get_type ())
#define GEDIT_FILE_CHOOSER_DIALOG_OSX(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GEDIT_TYPE_FILE_CHOOSER_DIALOG_OSX, GeditFileChooserDialogOSX))
#define GEDIT_FILE_CHOOSER_DIALOG_OSX_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GEDIT_TYPE_FILE_CHOOSER_DIALOG_OSX, GeditFileChooserDialogOSXClass))
#define GEDIT_IS_FILE_CHOOSER_DIALOG_OSX(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GEDIT_TYPE_FILE_CHOOSER_DIALOG_OSX))
#define GEDIT_IS_FILE_CHOOSER_DIALOG_OSX_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GEDIT_TYPE_FILE_CHOOSER_DIALOG_OSX))
#define GEDIT_FILE_CHOOSER_DIALOG_OSX_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GEDIT_TYPE_FILE_CHOOSER_DIALOG_OSX, GeditFileChooserDialogOSXClass))

typedef struct _GeditFileChooserDialogOSX		GeditFileChooserDialogOSX;
typedef struct _GeditFileChooserDialogOSXClass		GeditFileChooserDialogOSXClass;
typedef struct _GeditFileChooserDialogOSXPrivate	GeditFileChooserDialogOSXPrivate;

struct _GeditFileChooserDialogOSXClass
{
	GtkFileChooserDialogClass parent_class;
};

struct _GeditFileChooserDialogOSX
{
	GtkFileChooserDialog parent_instance;

	GeditFileChooserDialogOSXPrivate *priv;
};

GType		 	 gedit_file_chooser_dialog_osx_get_type		(void) G_GNUC_CONST;

GeditFileChooserDialog	*gedit_file_chooser_dialog_osx_create		(const gchar              *title,
									 GtkWindow                *parent,
									 GeditFileChooserFlags     flags,
									 const GtkSourceEncoding  *encoding);

G_END_DECLS

#endif /* __GEDIT_FILE_CHOOSER_DIALOG_OSX_H__ */

/* ex:set ts=8 noet: */
