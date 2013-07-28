/*
 * gedit-open-tool-button.c
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include "gedit-open-tool-button.h"

struct _GeditOpenToolButtonPrivate
{
	gint limit;
};

enum
{
	PROP_0,
	PROP_LIMIT
};

G_DEFINE_TYPE_WITH_PRIVATE (GeditOpenToolButton, gedit_open_tool_button, GTK_TYPE_MENU_TOOL_BUTTON)

static void
set_recent_menu (GeditOpenToolButton *button)
{
	GtkRecentManager *recent_manager;
	GtkRecentFilter *filter;
	GtkWidget *recent_menu;

	recent_manager = gtk_recent_manager_get_default ();

	recent_menu = gtk_recent_chooser_menu_new_for_manager (recent_manager);
	gtk_recent_chooser_set_local_only (GTK_RECENT_CHOOSER (recent_menu),
					   FALSE);
	gtk_recent_chooser_set_sort_type (GTK_RECENT_CHOOSER (recent_menu),
					  GTK_RECENT_SORT_MRU);

	g_object_bind_property (button, "limit",
	                        recent_menu, "limit",
	                        G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

	filter = gtk_recent_filter_new ();
	gtk_recent_filter_add_group (filter, "gedit");
	gtk_recent_chooser_set_filter (GTK_RECENT_CHOOSER (recent_menu),
				       filter);

	gtk_menu_tool_button_set_menu (GTK_MENU_TOOL_BUTTON (button),
				       recent_menu);
}

static void
gedit_open_tool_button_dispose (GObject *object)
{
	G_OBJECT_CLASS (gedit_open_tool_button_parent_class)->dispose (object);
}

static void
gedit_open_tool_button_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
	GeditOpenToolButton *button = GEDIT_OPEN_TOOL_BUTTON (object);

	switch (prop_id)
	{
		case PROP_LIMIT:
			g_value_set_int (value, button->priv->limit);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gedit_open_tool_button_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
	GeditOpenToolButton *button = GEDIT_OPEN_TOOL_BUTTON (object);

	switch (prop_id)
	{
		case PROP_LIMIT:
			button->priv->limit = g_value_get_int (value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gedit_open_tool_button_constructed (GObject *object)
{
	GeditOpenToolButton *button = GEDIT_OPEN_TOOL_BUTTON (object);

	set_recent_menu (button);

	gtk_tool_item_set_tooltip_text (GTK_TOOL_ITEM (button), _("Open a file"));
	gtk_menu_tool_button_set_arrow_tooltip_text (GTK_MENU_TOOL_BUTTON (button),
						     _("Open a recently used file"));

	G_OBJECT_CLASS (gedit_open_tool_button_parent_class)->constructed (object);
}

static void
gedit_open_tool_button_init (GeditOpenToolButton *button)
{
	button->priv = gedit_open_tool_button_get_instance_private (button);
	button->priv->limit = 10;
}

static void
gedit_open_tool_button_class_init (GeditOpenToolButtonClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = gedit_open_tool_button_dispose;
	object_class->get_property = gedit_open_tool_button_get_property;
	object_class->set_property = gedit_open_tool_button_set_property;
	object_class->constructed = gedit_open_tool_button_constructed;

	g_object_class_install_property (object_class, PROP_LIMIT,
	                                 g_param_spec_int ("limit",
	                                                   "Limit",
	                                                   "The maximum number of recently used documents.",
	                                                   0,
	                                                   G_MAXINT,
	                                                   10,
	                                                   G_PARAM_READWRITE |
	                                                   G_PARAM_STATIC_STRINGS));
}

GtkToolItem *
gedit_open_tool_button_new (void)
{
	GeditOpenToolButton *button;

	button = g_object_new (GEDIT_TYPE_OPEN_TOOL_BUTTON,
	                       "stock-id", GTK_STOCK_OPEN,
	                       NULL);

	return GTK_TOOL_ITEM (button);
}

/* ex:set ts=8 noet: */
