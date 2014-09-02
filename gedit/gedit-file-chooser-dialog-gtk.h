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

#ifndef __GEDIT_FILE_CHOOSER_DIALOG_GTK_H__
#define __GEDIT_FILE_CHOOSER_DIALOG_GTK_H__

#include <gtk/gtk.h>
#include "gedit-file-chooser-dialog.h"

G_BEGIN_DECLS

#define GEDIT_TYPE_FILE_CHOOSER_DIALOG_GTK             (gedit_file_chooser_dialog_gtk_get_type ())
#define GEDIT_FILE_CHOOSER_DIALOG_GTK(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GEDIT_TYPE_FILE_CHOOSER_DIALOG_GTK, GeditFileChooserDialogGtk))
#define GEDIT_FILE_CHOOSER_DIALOG_GTK_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GEDIT_TYPE_FILE_CHOOSER_DIALOG_GTK, GeditFileChooserDialogGtkClass))
#define GEDIT_IS_FILE_CHOOSER_DIALOG_GTK(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GEDIT_TYPE_FILE_CHOOSER_DIALOG_GTK))
#define GEDIT_IS_FILE_CHOOSER_DIALOG_GTK_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GEDIT_TYPE_FILE_CHOOSER_DIALOG_GTK))
#define GEDIT_FILE_CHOOSER_DIALOG_GTK_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GEDIT_TYPE_FILE_CHOOSER_DIALOG_GTK, GeditFileChooserDialogGtkClass))

typedef struct _GeditFileChooserDialogGtk		GeditFileChooserDialogGtk;
typedef struct _GeditFileChooserDialogGtkClass		GeditFileChooserDialogGtkClass;
typedef struct _GeditFileChooserDialogGtkPrivate	GeditFileChooserDialogGtkPrivate;

struct _GeditFileChooserDialogGtkClass
{
	GtkFileChooserDialogClass parent_class;
};

struct _GeditFileChooserDialogGtk
{
	GtkFileChooserDialog parent_instance;

	GeditFileChooserDialogGtkPrivate *priv;
};

GType		 	 gedit_file_chooser_dialog_gtk_get_type		(void) G_GNUC_CONST;

GeditFileChooserDialog	*gedit_file_chooser_dialog_gtk_create		(const gchar             *title,
									 GtkWindow               *parent,
									 GeditFileChooserFlags    flags,
									 const GtkSourceEncoding *encoding);

G_END_DECLS

#endif /* __GEDIT_FILE_CHOOSER_DIALOG_GTK_H__ */

/* ex:set ts=8 noet: */
