/*
 * bedit-trim-trailing-plugin.h
 *
 * Copyright (C) 2020 Ben Mather
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
 *
 */

#ifndef BEDIT_TRIM_TRAILING_PLUGIN_H
#define BEDIT_TRIM_TRAILING_PLUGIN_H

#include <glib-object.h>
#include <glib.h>
#include <libpeas/peas-extension-base.h>
#include <libpeas/peas-object-module.h>

G_BEGIN_DECLS

#define BEDIT_TYPE_TRIM_TRAILING_PLUGIN                                     \
    (bedit_trim_trailing_plugin_get_type())

G_DECLARE_FINAL_TYPE(
    BeditTrimTrailingPlugin, bedit_trim_trailing_plugin,
    BEDIT, TRIM_TRAILING_PLUGIN,
    PeasExtensionBase
)

GType bedit_trim_trailing_plugin_get_type(void) G_GNUC_CONST;

G_MODULE_EXPORT void peas_register_types(PeasObjectModule *module);

G_END_DECLS

#endif /* BEDIT_TRIM_TRAILING_PLUGIN_H */

