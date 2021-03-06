/*
 * bedit-spell-app-activatable.h
 * This file is part of bedit
 *
 * Copyright (C) 2014 - Ignacio Casal Quinteiro
 *
 * bedit is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * bedit is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with bedit. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef BEDIT_SPELL_APP_ACTIVATABLE_H
#define BEDIT_SPELL_APP_ACTIVATABLE_H

#include <glib-object.h>

G_BEGIN_DECLS

#define BEDIT_TYPE_SPELL_APP_ACTIVATABLE                                    \
    (bedit_spell_app_activatable_get_type())
G_DECLARE_FINAL_TYPE(
    BeditSpellAppActivatable, bedit_spell_app_activatable, BEDIT,
    SPELL_APP_ACTIVATABLE, GObject
)

void bedit_spell_app_activatable_register(GTypeModule *module);

G_END_DECLS

#endif /* BEDIT_SPELL_APP_ACTIVATABLE_H */

