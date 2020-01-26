/*
 * gedit-file-browser-plugin.h - Bedit plugin providing easy file access
 * from the sidepanel
 *
 * Copyright (C) 2006 - Jesse van den Kieboom <jesse@icecrew.nl>
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

#ifndef GEDIT_FILE_BROWSER_PLUGIN_H
#define GEDIT_FILE_BROWSER_PLUGIN_H

#include <glib.h>
#include <glib-object.h>
#include <libpeas/peas-extension-base.h>
#include <libpeas/peas-object-module.h>

G_BEGIN_DECLS
/*
 * Type checking and casting macros
 */
#define GEDIT_TYPE_FILE_BROWSER_PLUGIN		(gedit_file_browser_plugin_get_type ())
#define GEDIT_FILE_BROWSER_PLUGIN(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GEDIT_TYPE_FILE_BROWSER_PLUGIN, BeditFileBrowserPlugin))
#define GEDIT_FILE_BROWSER_PLUGIN_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), GEDIT_TYPE_FILE_BROWSER_PLUGIN, BeditFileBrowserPluginClass))
#define GEDIT_IS_FILE_BROWSER_PLUGIN(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GEDIT_TYPE_FILE_BROWSER_PLUGIN))
#define GEDIT_IS_FILE_BROWSER_PLUGIN_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GEDIT_TYPE_FILE_BROWSER_PLUGIN))
#define GEDIT_FILE_BROWSER_GET_CLASS(o)		(G_TYPE_INSTANCE_GET_CLASS ((o), GEDIT_TYPE_FILE_BROWSER_PLUGIN, BeditFileBrowserPluginClass))

/* Private structure type */
typedef struct _BeditFileBrowserPluginPrivate BeditFileBrowserPluginPrivate;
typedef struct _BeditFileBrowserPlugin        BeditFileBrowserPlugin;
typedef struct _BeditFileBrowserPluginClass   BeditFileBrowserPluginClass;

struct _BeditFileBrowserPlugin
{
	GObject parent_instance;

	/* < private > */
	BeditFileBrowserPluginPrivate *priv;
};

struct _BeditFileBrowserPluginClass
{
	GObjectClass parent_class;
};

/*
 * Public methods
 */
GType			gedit_file_browser_plugin_get_type	(void) G_GNUC_CONST;

/* All the plugins must implement this function */
G_MODULE_EXPORT void	peas_register_types			(PeasObjectModule *module);

G_END_DECLS

#endif /* GEDIT_FILE_BROWSER_PLUGIN_H */

/* ex:set ts=8 noet: */
