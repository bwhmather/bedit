/*
 * gedit-highlight-mode-dialog.h
 * This file is part of gedit
 *
 * Copyright (C) 2013 - Ignacio Casal Quinteiro
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
 * along with gedit. If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef __GEDIT_HIGHLIGHT_MODE_DIALOG_H__
#define __GEDIT_HIGHLIGHT_MODE_DIALOG_H__

#include <glib-object.h>
#include <gtksourceview/gtksource.h>
#include "gedit-window.h"

G_BEGIN_DECLS

#define GEDIT_TYPE_HIGHLIGHT_MODE_DIALOG		(gedit_highlight_mode_dialog_get_type ())
#define GEDIT_HIGHLIGHT_MODE_DIALOG(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GEDIT_TYPE_HIGHLIGHT_MODE_DIALOG, GeditHighlightModeDialog))
#define GEDIT_HIGHLIGHT_MODE_DIALOG_CONST(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GEDIT_TYPE_HIGHLIGHT_MODE_DIALOG, GeditHighlightModeDialog const))
#define GEDIT_HIGHLIGHT_MODE_DIALOG_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), GEDIT_TYPE_HIGHLIGHT_MODE_DIALOG, GeditHighlightModeDialogClass))
#define GEDIT_IS_HIGHLIGHT_MODE_DIALOG(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GEDIT_TYPE_HIGHLIGHT_MODE_DIALOG))
#define GEDIT_IS_HIGHLIGHT_MODE_DIALOG_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GEDIT_TYPE_HIGHLIGHT_MODE_DIALOG))
#define GEDIT_HIGHLIGHT_MODE_DIALOG_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), GEDIT_TYPE_HIGHLIGHT_MODE_DIALOG, GeditHighlightModeDialogClass))

typedef struct _GeditHighlightModeDialog	GeditHighlightModeDialog;
typedef struct _GeditHighlightModeDialogClass	GeditHighlightModeDialogClass;
typedef struct _GeditHighlightModeDialogPrivate	GeditHighlightModeDialogPrivate;

struct _GeditHighlightModeDialog
{
	GtkDialog parent;

	GeditHighlightModeDialogPrivate *priv;
};

struct _GeditHighlightModeDialogClass
{
	GtkDialogClass parent_class;

	void (* language_selected) (GeditHighlightModeDialog *dialog,
	                            GtkSourceLanguage        *language);
};

GType                    gedit_highlight_mode_dialog_get_type        (void) G_GNUC_CONST;

GtkWidget               *gedit_highlight_mode_dialog_new             (GtkWindow *parent);

void                     gedit_highlight_mode_dialog_select_language (GeditHighlightModeDialog *dlg,
                                                                      GtkSourceLanguage        *language);

G_END_DECLS

#endif /* __GEDIT_HIGHLIGHT_MODE_DIALOG_H__ */

/* ex:set ts=8 noet: */
