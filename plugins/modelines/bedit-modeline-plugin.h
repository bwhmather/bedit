/*
 * bedit-modeline-plugin.h
 * Emacs, Kate and Vim-style modelines support for bedit.
 *
 * Copyright (C) 2005-2007 - Steve Frécinaux <code@istique.net>
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

#ifndef BEDIT_MODELINE_PLUGIN_H
#define BEDIT_MODELINE_PLUGIN_H

#include <glib-object.h>
#include <glib.h>
#include <libpeas/peas-extension-base.h>
#include <libpeas/peas-object-module.h>

G_BEGIN_DECLS

#define BEDIT_TYPE_MODELINE_PLUGIN (bedit_modeline_plugin_get_type())
#define BEDIT_MODELINE_PLUGIN(o)                                            \
    (G_TYPE_CHECK_INSTANCE_CAST(                                            \
        (o), BEDIT_TYPE_MODELINE_PLUGIN, BeditModelinePlugin                \
    ))
#define BEDIT_MODELINE_PLUGIN_CLASS(k)                                      \
    (G_TYPE_CHECK_CLASS_CAST(                                               \
        (k), BEDIT_TYPE_MODELINE_PLUGIN, BeditModelinePluginClass           \
    ))
#define BEDIT_IS_MODELINE_PLUGIN(o)                                         \
    (G_TYPE_CHECK_INSTANCE_TYPE((o), BEDIT_TYPE_MODELINE_PLUGIN))
#define BEDIT_IS_MODELINE_PLUGIN_CLASS(k)                                   \
    (G_TYPE_CHECK_CLASS_TYPE((k), BEDIT_TYPE_MODELINE_PLUGIN))
#define BEDIT_MODELINE_PLUGIN_GET_CLASS(o)                                  \
    (G_TYPE_INSTANCE_GET_CLASS(                                             \
        (o), BEDIT_TYPE_MODELINE_PLUGIN, BeditModelinePluginClass           \
    ))

typedef struct _BeditModelinePlugin BeditModelinePlugin;
typedef struct _BeditModelinePluginPrivate BeditModelinePluginPrivate;
typedef struct _BeditModelinePluginClass BeditModelinePluginClass;

struct _BeditModelinePlugin {
    PeasExtensionBase parent;

    /*< private >*/
    BeditModelinePluginPrivate *priv;
};

struct _BeditModelinePluginClass {
    PeasExtensionBaseClass parent_class;
};

GType bedit_modeline_plugin_get_type(void) G_GNUC_CONST;

G_MODULE_EXPORT void peas_register_types(PeasObjectModule *module);

G_END_DECLS

#endif /* BEDIT_MODELINE_PLUGIN_H */

