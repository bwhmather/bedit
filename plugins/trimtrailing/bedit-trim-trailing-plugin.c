/*
 * bedit-trim-trailing-plugin.c
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

#include "config.h"

#include <libpeas-gtk/peas-gtk-configurable.h>

#include <bedit-trim-trailing-view-activatable.h>

#include "bedit-trim-trailing-plugin.h"


G_MODULE_EXPORT void peas_register_types(PeasObjectModule *module) {
    bedit_trim_trailing_view_activatable_register(module);
}

