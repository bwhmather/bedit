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


#ifndef __GEDIT_SPELL_APP_ACTIVATABLE_H__
#define __GEDIT_SPELL_APP_ACTIVATABLE_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define GEDIT_TYPE_SPELL_APP_ACTIVATABLE		(gedit_spell_app_activatable_get_type ())
#define GEDIT_SPELL_APP_ACTIVATABLE(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GEDIT_TYPE_SPELL_APP_ACTIVATABLE, GeditSpellAppActivatable))
#define GEDIT_SPELL_APP_ACTIVATABLE_CONST(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GEDIT_TYPE_SPELL_APP_ACTIVATABLE, GeditSpellAppActivatable const))
#define GEDIT_SPELL_APP_ACTIVATABLE_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), GEDIT_TYPE_SPELL_APP_ACTIVATABLE, GeditSpellAppActivatableClass))
#define GEDIT_IS_SPELL_APP_ACTIVATABLE(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GEDIT_TYPE_SPELL_APP_ACTIVATABLE))
#define GEDIT_IS_SPELL_APP_ACTIVATABLE_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GEDIT_TYPE_SPELL_APP_ACTIVATABLE))
#define GEDIT_SPELL_APP_ACTIVATABLE_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), GEDIT_TYPE_SPELL_APP_ACTIVATABLE, GeditSpellAppActivatableClass))

typedef struct _GeditSpellAppActivatable	GeditSpellAppActivatable;
typedef struct _GeditSpellAppActivatableClass	GeditSpellAppActivatableClass;

struct _GeditSpellAppActivatable
{
	GObject parent;
};

struct _GeditSpellAppActivatableClass
{
	GObjectClass parent_class;
};

GType          gedit_spell_app_activatable_get_type (void) G_GNUC_CONST;

void           gedit_spell_app_activatable_register (GTypeModule *module);

G_END_DECLS

#endif /* __GEDIT_SPELL_APP_ACTIVATABLE_H__ */
