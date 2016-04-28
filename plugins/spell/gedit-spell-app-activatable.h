/*
 * gedit-spell-app-activatable.h
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


#ifndef GEDIT_SPELL_APP_ACTIVATABLE_H
#define GEDIT_SPELL_APP_ACTIVATABLE_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GEDIT_TYPE_SPELL_APP_ACTIVATABLE (gedit_spell_app_activatable_get_type ())
G_DECLARE_FINAL_TYPE (GeditSpellAppActivatable, gedit_spell_app_activatable,
		      GEDIT, SPELL_APP_ACTIVATABLE,
		      GObject)

void	gedit_spell_app_activatable_register	(GTypeModule *module);

G_END_DECLS

#endif /* GEDIT_SPELL_APP_ACTIVATABLE_H */
