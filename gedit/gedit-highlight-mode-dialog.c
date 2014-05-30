/*
 * gedit-highlight-mode-dialog.c
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

#include <gtk/gtk.h>
#include "gedit-highlight-mode-dialog.h"

struct _GeditHighlightModeDialogPrivate
{
	GeditHighlightModeSelector *selector;
};

G_DEFINE_TYPE_WITH_PRIVATE (GeditHighlightModeDialog, gedit_highlight_mode_dialog, GTK_TYPE_DIALOG)

static void
gedit_highlight_mode_dialog_response (GtkDialog *dialog,
                                      gint       response_id)
{
	GeditHighlightModeDialogPrivate *priv = GEDIT_HIGHLIGHT_MODE_DIALOG (dialog)->priv;

	if (response_id == GTK_RESPONSE_OK)
	{
		/* The dialog will be destroyed if a language is selected */
		gedit_highlight_mode_selector_activate_selected_language (priv->selector);
	}
	else
	{
		gtk_widget_destroy (GTK_WIDGET (dialog));
	}
}

static void
on_language_selected (GeditHighlightModeSelector *sel,
                      GtkSourceLanguage          *language,
                      GtkDialog                  *dialog)
{
	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
gedit_highlight_mode_dialog_class_init (GeditHighlightModeDialogClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
	GtkDialogClass *dialog_class = GTK_DIALOG_CLASS (klass);

	dialog_class->response = gedit_highlight_mode_dialog_response;

	/* Bind class to template */
	gtk_widget_class_set_template_from_resource (widget_class,
	                                             "/org/gnome/gedit/ui/gedit-highlight-mode-dialog.ui");
	gtk_widget_class_bind_template_child_private (widget_class, GeditHighlightModeDialog, selector);
}

static void
gedit_highlight_mode_dialog_init (GeditHighlightModeDialog *dlg)
{
	dlg->priv = gedit_highlight_mode_dialog_get_instance_private (dlg);

	gtk_widget_init_template (GTK_WIDGET (dlg));
	gtk_dialog_set_default_response (GTK_DIALOG (dlg), GTK_RESPONSE_OK);

	g_signal_connect (dlg->priv->selector, "language-selected",
	                  G_CALLBACK (on_language_selected), dlg);
}

GtkWidget *
gedit_highlight_mode_dialog_new (GtkWindow *parent)
{
	return GTK_WIDGET (g_object_new (GEDIT_TYPE_HIGHLIGHT_MODE_DIALOG,
	                                 "transient-for", parent,
	                                 "use-header-bar", TRUE,
	                                 NULL));
}

GeditHighlightModeSelector *
gedit_highlight_mode_dialog_get_selector (GeditHighlightModeDialog *dlg)
{
	g_return_val_if_fail (GEDIT_IS_HIGHLIGHT_MODE_DIALOG (dlg), NULL);

	return dlg->priv->selector;
}

/* ex:set ts=8 noet: */
