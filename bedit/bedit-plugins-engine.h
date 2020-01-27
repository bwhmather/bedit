/*
 * bedit-plugins-engine.h
 * This file is part of bedit
 *
 * Copyright (C) 2002-2005 - Paolo Maggi
 * Copyright (C) 2010 - Steve Frécinaux
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef BEDIT_PLUGINS_ENGINE_H
#define BEDIT_PLUGINS_ENGINE_H

#include <glib.h>
#include <libpeas/peas.h>

G_BEGIN_DECLS

#define BEDIT_TYPE_PLUGINS_ENGINE (bedit_plugins_engine_get_type())
G_DECLARE_FINAL_TYPE(
    BeditPluginsEngine, bedit_plugins_engine, BEDIT, PLUGINS_ENGINE, PeasEngine)

BeditPluginsEngine *bedit_plugins_engine_get_default(void);

G_END_DECLS

#endif /* BEDIT_PLUGINS_ENGINE_H */

