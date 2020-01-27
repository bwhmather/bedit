/*
 * bedit-time-plugin.h
 *
 * Copyright (C) 2002-2005 - Paolo Maggi
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

#ifndef BEDIT_TIME_PLUGIN_H
#define BEDIT_TIME_PLUGIN_H

#include <glib-object.h>
#include <glib.h>
#include <libpeas/peas-extension-base.h>
#include <libpeas/peas-object-module.h>

G_BEGIN_DECLS

#define BEDIT_TYPE_TIME_PLUGIN (bedit_time_plugin_get_type())
#define BEDIT_TIME_PLUGIN(o)                                                   \
    (G_TYPE_CHECK_INSTANCE_CAST((o), BEDIT_TYPE_TIME_PLUGIN, BeditTimePlugin))
#define BEDIT_TIME_PLUGIN_CLASS(k)                                             \
    (G_TYPE_CHECK_CLASS_CAST((k), BEDIT_TYPE_TIME_PLUGIN, BeditTimePluginClass))
#define BEDIT_IS_TIME_PLUGIN(o)                                                \
    (G_TYPE_CHECK_INSTANCE_TYPE((o), BEDIT_TYPE_TIME_PLUGIN))
#define BEDIT_IS_TIME_PLUGIN_CLASS(k)                                          \
    (G_TYPE_CHECK_CLASS_TYPE((k), BEDIT_TYPE_TIME_PLUGIN))
#define BEDIT_TIME_PLUGIN_GET_CLASS(o)                                         \
    (G_TYPE_INSTANCE_GET_CLASS(                                                \
        (o), BEDIT_TYPE_TIME_PLUGIN, BeditTimePluginClass))

typedef struct _BeditTimePlugin BeditTimePlugin;
typedef struct _BeditTimePluginPrivate BeditTimePluginPrivate;
typedef struct _BeditTimePluginClass BeditTimePluginClass;

struct _BeditTimePlugin {
    PeasExtensionBase parent_instance;

    /*< private >*/
    BeditTimePluginPrivate *priv;
};

struct _BeditTimePluginClass {
    PeasExtensionBaseClass parent_class;
};

GType bedit_time_plugin_get_type(void) G_GNUC_CONST;

G_MODULE_EXPORT void peas_register_types(PeasObjectModule *module);

G_END_DECLS

#endif /* BEDIT_TIME_PLUGIN_H */

