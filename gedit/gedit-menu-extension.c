/*
 * gedit-menu-extension.c
 * This file is part of gedit
 *
 * Copyright (C) 2014 - Ignacio Casal Quinteiro
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

#include "gedit-menu-extension.h"

#include <string.h>

static guint last_merge_id = 0;

struct _GeditMenuExtension
{
	GObject parent_instance;

	GMenu *menu;
	guint merge_id;
	gboolean dispose_has_run;
};

enum
{
	PROP_0,
	PROP_MENU,
	LAST_PROP
};

static GParamSpec *properties[LAST_PROP];

G_DEFINE_TYPE (GeditMenuExtension, gedit_menu_extension, G_TYPE_OBJECT)

static void
gedit_menu_extension_dispose (GObject *object)
{
	GeditMenuExtension *menu = GEDIT_MENU_EXTENSION (object);

	if (!menu->dispose_has_run)
	{
		gedit_menu_extension_remove_items (menu);
		menu->dispose_has_run = TRUE;
	}

	g_clear_object (&menu->menu);

	G_OBJECT_CLASS (gedit_menu_extension_parent_class)->dispose (object);
}

static void
gedit_menu_extension_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
	GeditMenuExtension *menu = GEDIT_MENU_EXTENSION (object);

	switch (prop_id)
	{
		case PROP_MENU:
			g_value_set_object (value, menu->menu);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gedit_menu_extension_set_property (GObject     *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
	GeditMenuExtension *menu = GEDIT_MENU_EXTENSION (object);

	switch (prop_id)
	{
		case PROP_MENU:
			menu->menu = g_value_dup_object (value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gedit_menu_extension_class_init (GeditMenuExtensionClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = gedit_menu_extension_dispose;
	object_class->get_property = gedit_menu_extension_get_property;
	object_class->set_property = gedit_menu_extension_set_property;

	properties[PROP_MENU] =
		g_param_spec_object ("menu",
		                     "Menu",
		                     "The main menu",
		                     G_TYPE_MENU,
		                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
gedit_menu_extension_init (GeditMenuExtension *menu)
{
	menu->merge_id = ++last_merge_id;
}

GeditMenuExtension *
gedit_menu_extension_new (GMenu *menu)
{
	return g_object_new (GEDIT_TYPE_MENU_EXTENSION, "menu", menu, NULL);
}

void
gedit_menu_extension_append_menu_item (GeditMenuExtension *menu,
                                       GMenuItem       *item)
{
	g_return_if_fail (GEDIT_IS_MENU_EXTENSION (menu));
	g_return_if_fail (G_IS_MENU_ITEM (item));

	if (menu->menu != NULL)
	{
		g_menu_item_set_attribute (item, "gedit-merge-id", "u", menu->merge_id);
		g_menu_append_item (menu->menu, item);
	}
}

void
gedit_menu_extension_prepend_menu_item (GeditMenuExtension *menu,
                                        GMenuItem       *item)
{
	g_return_if_fail (GEDIT_IS_MENU_EXTENSION (menu));
	g_return_if_fail (G_IS_MENU_ITEM (item));

	if (menu->menu != NULL)
	{
		g_menu_item_set_attribute (item, "gedit-merge-id", "u", menu->merge_id);
		g_menu_prepend_item (menu->menu, item);
	}
}

void
gedit_menu_extension_remove_items (GeditMenuExtension *menu)
{
	gint i, n_items;

	g_return_if_fail (GEDIT_IS_MENU_EXTENSION (menu));

	n_items = g_menu_model_get_n_items (G_MENU_MODEL (menu->menu));
	i = 0;
	while (i < n_items)
	{
		guint id = 0;

		if (g_menu_model_get_item_attribute (G_MENU_MODEL (menu->menu),
		                                     i, "gedit-merge-id", "u", &id) &&
		    id == menu->merge_id)
		{
			g_menu_remove (menu->menu, i);
			n_items--;
		}
		else
		{
			i++;
		}
	}
}

/* ex:set ts=8 noet: */
