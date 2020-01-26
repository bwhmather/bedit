/*
 * bedit-sort-plugin.h
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

#ifndef GEDIT_SORT_PLUGIN_H
#define GEDIT_SORT_PLUGIN_H

#include <glib.h>
#include <glib-object.h>
#include <libpeas/peas-extension-base.h>
#include <libpeas/peas-object-module.h>

G_BEGIN_DECLS

#define GEDIT_TYPE_SORT_PLUGIN		(bedit_sort_plugin_get_type ())
#define GEDIT_SORT_PLUGIN(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GEDIT_TYPE_SORT_PLUGIN, BeditSortPlugin))
#define GEDIT_SORT_PLUGIN_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), GEDIT_TYPE_SORT_PLUGIN, BeditSortPluginClass))
#define GEDIT_IS_SORT_PLUGIN(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GEDIT_TYPE_SORT_PLUGIN))
#define GEDIT_IS_SORT_PLUGIN_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GEDIT_TYPE_SORT_PLUGIN))
#define GEDIT_SORT_PLUGIN_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GEDIT_TYPE_SORT_PLUGIN, BeditSortPluginClass))

typedef struct _BeditSortPlugin		BeditSortPlugin;
typedef struct _BeditSortPluginPrivate	BeditSortPluginPrivate;
typedef struct _BeditSortPluginClass	BeditSortPluginClass;

struct _BeditSortPlugin
{
	PeasExtensionBase parent;

	/*< private >*/
	BeditSortPluginPrivate *priv;
};

struct _BeditSortPluginClass
{
	PeasExtensionBaseClass parent_class;
};

GType			bedit_sort_plugin_get_type	(void) G_GNUC_CONST;

G_MODULE_EXPORT void	peas_register_types		(PeasObjectModule *module);

G_END_DECLS

#endif /* GEDIT_SORT_PLUGIN_H */
/* ex:set ts=8 noet: */
