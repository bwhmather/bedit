/*
 * bedit-view-activatable.h
 * This file is part of bedit
 *
 * Copyright (C) 2010 Steve Frécinaux
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Library General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "bedit-view-activatable.h"

#include "bedit-view.h"

/**
 * SECTION:bedit-view-activatable
 * @short_description: Interface for activatable extensions on views
 * @see_also: #PeasExtensionSet
 *
 * #BeditViewActivatable is an interface which should be implemented by
 * extensions that should be activated on a bedit view.
 **/

G_DEFINE_INTERFACE(BeditViewActivatable, bedit_view_activatable, G_TYPE_OBJECT)

static void
bedit_view_activatable_default_init (BeditViewActivatableInterface *iface)
{
	/**
	 * BeditViewActivatable:view:
	 *
	 * The window property contains the bedit window for this
	 * #BeditViewActivatable instance.
	 */
	g_object_interface_install_property (iface,
	                                     g_param_spec_object ("view",
	                                                          "view",
	                                                          "A bedit view",
	                                                          GEDIT_TYPE_VIEW,
	                                                          G_PARAM_READWRITE |
	                                                          G_PARAM_CONSTRUCT_ONLY |
	                                                          G_PARAM_STATIC_STRINGS));
}

/**
 * bedit_view_activatable_activate:
 * @activatable: A #BeditViewActivatable.
 *
 * Activates the extension on the window property.
 */
void
bedit_view_activatable_activate (BeditViewActivatable *activatable)
{
	BeditViewActivatableInterface *iface;

	g_return_if_fail (GEDIT_IS_VIEW_ACTIVATABLE (activatable));

	iface = GEDIT_VIEW_ACTIVATABLE_GET_IFACE (activatable);
	if (iface->activate != NULL)
	{
		iface->activate (activatable);
	}
}

/**
 * bedit_view_activatable_deactivate:
 * @activatable: A #BeditViewActivatable.
 *
 * Deactivates the extension on the window property.
 */
void
bedit_view_activatable_deactivate (BeditViewActivatable *activatable)
{
	BeditViewActivatableInterface *iface;

	g_return_if_fail (GEDIT_IS_VIEW_ACTIVATABLE (activatable));

	iface = GEDIT_VIEW_ACTIVATABLE_GET_IFACE (activatable);
	if (iface->deactivate != NULL)
	{
		iface->deactivate (activatable);
	}
}

