/*
 * Copyright (C) 2009 - Ignacio Casal Quinteiro <icq@gnome.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GEDIT_CHECK_UPDATE_PLUGIN_H
#define GEDIT_CHECK_UPDATE_PLUGIN_H

#include <glib.h>
#include <glib-object.h>
#include <libpeas/peas-extension-base.h>
#include <libpeas/peas-object-module.h>

G_BEGIN_DECLS

#define GEDIT_TYPE_CHECK_UPDATE_PLUGIN		(gedit_check_update_plugin_get_type ())
#define GEDIT_CHECK_UPDATE_PLUGIN(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GEDIT_TYPE_CHECK_UPDATE_PLUGIN, BeditCheckUpdatePlugin))
#define GEDIT_CHECK_UPDATE_PLUGIN_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), GEDIT_TYPE_CHECK_UPDATE_PLUGIN, BeditCheckUpdatePluginClass))
#define GEDIT_IS_CHECK_UPDATE_PLUGIN(o)	(G_TYPE_CHECK_INSTANCE_TYPE ((o), GEDIT_TYPE_CHECK_UPDATE_PLUGIN))
#define GEDIT_IS_CHECK_UPDATE_PLUGIN_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GEDIT_TYPE_CHECK_UPDATE_PLUGIN))
#define GEDIT_CHECK_UPDATE_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GEDIT_TYPE_CHECK_UPDATE_PLUGIN, BeditCheckUpdatePluginClass))

typedef struct _BeditCheckUpdatePluginPrivate	BeditCheckUpdatePluginPrivate;
typedef struct _BeditCheckUpdatePlugin		BeditCheckUpdatePlugin;
typedef struct _BeditCheckUpdatePluginClass	BeditCheckUpdatePluginClass;

struct _BeditCheckUpdatePlugin
{
	PeasExtensionBase parent;

	/*< private >*/
	BeditCheckUpdatePluginPrivate *priv;
};

struct _BeditCheckUpdatePluginClass
{
	PeasExtensionBaseClass parent_class;
};

GType			gedit_check_update_plugin_get_type	(void) G_GNUC_CONST;

G_MODULE_EXPORT void	peas_register_types			(PeasObjectModule *module);

G_END_DECLS

#endif /* GEDIT_CHECK_UPDATE_PLUGIN_H */
/* ex:set ts=8 noet: */
