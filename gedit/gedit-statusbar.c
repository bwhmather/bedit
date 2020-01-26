/*
 * gedit-statusbar.c
 * This file is part of gedit
 *
 * Copyright (C) 2005 - Paolo Borelli
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

#include "config.h"

#include "gedit-statusbar.h"

#include <string.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "gedit-app.h"
#include "gedit-status-menu-button.h"

struct _BeditStatusbar
{
	GtkStatusbar  parent_instance;

	GtkWidget     *error_frame;
	GtkWidget     *error_image;
	GtkWidget     *state_frame;
	GtkWidget     *load_image;
	GtkWidget     *save_image;
	GtkWidget     *print_image;
	GtkWidget     *overwrite_mode_label;

	/* tmp flash timeout data */
	guint          flash_timeout;
	guint          flash_context_id;
	guint          flash_message_id;
};

G_DEFINE_TYPE (BeditStatusbar, gedit_statusbar, GTK_TYPE_STATUSBAR)

static gchar *
get_overwrite_mode_string (gboolean overwrite)
{
	/* Use spaces to leave padding proportional to the font size */
	return g_strdup_printf ("  %s  ", overwrite ? _("OVR") : _("INS"));
}

static gint
get_overwrite_mode_length (void)
{
	return 4 + MAX (g_utf8_strlen (_("OVR"), -1), g_utf8_strlen (_("INS"), -1));
}

static void
gedit_statusbar_dispose (GObject *object)
{
	BeditStatusbar *statusbar = GEDIT_STATUSBAR (object);

	if (statusbar->flash_timeout > 0)
	{
		g_source_remove (statusbar->flash_timeout);
		statusbar->flash_timeout = 0;
	}

	G_OBJECT_CLASS (gedit_statusbar_parent_class)->dispose (object);
}

static void
gedit_statusbar_class_init (BeditStatusbarClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->dispose = gedit_statusbar_dispose;

	gtk_widget_class_set_template_from_resource (widget_class,
		                                    "/com/bwhmather/bedit/ui/gedit-statusbar.ui");

	gtk_widget_class_bind_template_child (widget_class, BeditStatusbar, error_frame);
	gtk_widget_class_bind_template_child (widget_class, BeditStatusbar, error_image);
	gtk_widget_class_bind_template_child (widget_class, BeditStatusbar, state_frame);
	gtk_widget_class_bind_template_child (widget_class, BeditStatusbar, load_image);
	gtk_widget_class_bind_template_child (widget_class, BeditStatusbar, save_image);
	gtk_widget_class_bind_template_child (widget_class, BeditStatusbar, print_image);
	gtk_widget_class_bind_template_child (widget_class, BeditStatusbar, overwrite_mode_label);
}

static void
gedit_statusbar_init (BeditStatusbar *statusbar)
{
	gtk_widget_init_template (GTK_WIDGET (statusbar));

	gtk_label_set_width_chars (GTK_LABEL (statusbar->overwrite_mode_label),
	                           get_overwrite_mode_length ());
}

/**
 * gedit_statusbar_new:
 *
 * Creates a new #BeditStatusbar.
 *
 * Return value: the new #BeditStatusbar object
 **/
GtkWidget *
gedit_statusbar_new (void)
{
	return GTK_WIDGET (g_object_new (GEDIT_TYPE_STATUSBAR, NULL));
}

/**
 * gedit_statusbar_set_overwrite:
 * @statusbar: a #BeditStatusbar
 * @overwrite: if the overwrite mode is set
 *
 * Sets the overwrite mode on the statusbar.
 **/
void
gedit_statusbar_set_overwrite (BeditStatusbar *statusbar,
                               gboolean        overwrite)
{
	gchar *msg;

	g_return_if_fail (GEDIT_IS_STATUSBAR (statusbar));

	msg = get_overwrite_mode_string (overwrite);
	gtk_label_set_text (GTK_LABEL (statusbar->overwrite_mode_label), msg);
	g_free (msg);
}

void
gedit_statusbar_clear_overwrite (BeditStatusbar *statusbar)
{
	g_return_if_fail (GEDIT_IS_STATUSBAR (statusbar));

	gtk_label_set_text (GTK_LABEL (statusbar->overwrite_mode_label), NULL);
}

static gboolean
remove_message_timeout (BeditStatusbar *statusbar)
{
	gtk_statusbar_remove (GTK_STATUSBAR (statusbar),
	                      statusbar->flash_context_id,
	                      statusbar->flash_message_id);

	/* remove the timeout */
	statusbar->flash_timeout = 0;
  	return FALSE;
}

/* FIXME this is an issue for introspection */
/**
 * gedit_statusbar_flash_message:
 * @statusbar: a #BeditStatusbar
 * @context_id: message context_id
 * @format: message to flash on the statusbar
 * @...: the arguments to insert in @format
 *
 * Flash a temporary message on the statusbar.
 */
void
gedit_statusbar_flash_message (BeditStatusbar *statusbar,
			       guint           context_id,
			       const gchar    *format,
			       ...)
{
	const guint32 flash_length = 3000; /* three seconds */
	va_list args;
	gchar *msg;

	g_return_if_fail (GEDIT_IS_STATUSBAR (statusbar));
	g_return_if_fail (format != NULL);

	va_start (args, format);
	msg = g_strdup_vprintf (format, args);
	va_end (args);

	/* remove a currently ongoing flash message */
	if (statusbar->flash_timeout > 0)
	{
		g_source_remove (statusbar->flash_timeout);
		statusbar->flash_timeout = 0;

		gtk_statusbar_remove (GTK_STATUSBAR (statusbar),
		                      statusbar->flash_context_id,
		                      statusbar->flash_message_id);
	}

	statusbar->flash_context_id = context_id;
	statusbar->flash_message_id = gtk_statusbar_push (GTK_STATUSBAR (statusbar),
							  context_id,
							  msg);

	statusbar->flash_timeout = g_timeout_add (flash_length,
						  (GSourceFunc) remove_message_timeout,
						  statusbar);

	g_free (msg);
}

void
gedit_statusbar_set_window_state (BeditStatusbar   *statusbar,
				  BeditWindowState  state,
				  gint              num_of_errors)
{
	g_return_if_fail (GEDIT_IS_STATUSBAR (statusbar));

	gtk_widget_hide (statusbar->state_frame);
	gtk_widget_hide (statusbar->save_image);
	gtk_widget_hide (statusbar->load_image);
	gtk_widget_hide (statusbar->print_image);

	if (state & GEDIT_WINDOW_STATE_SAVING)
	{
		gtk_widget_show (statusbar->state_frame);
		gtk_widget_show (statusbar->save_image);
	}
	if (state & GEDIT_WINDOW_STATE_LOADING)
	{
		gtk_widget_show (statusbar->state_frame);
		gtk_widget_show (statusbar->load_image);
	}
	if (state & GEDIT_WINDOW_STATE_PRINTING)
	{
		gtk_widget_show (statusbar->state_frame);
		gtk_widget_show (statusbar->print_image);
	}
	if (state & GEDIT_WINDOW_STATE_ERROR)
	{
	 	gchar *tip;

 		tip = g_strdup_printf (ngettext("There is a tab with errors",
		                                "There are %d tabs with errors",
		                                num_of_errors),
		                       num_of_errors);

		gtk_widget_set_tooltip_text (statusbar->error_image, tip);
		g_free (tip);

		gtk_widget_show (statusbar->error_frame);
	}
	else
	{
		gtk_widget_hide (statusbar->error_frame);
	}
}

/* ex:set ts=8 noet: */
