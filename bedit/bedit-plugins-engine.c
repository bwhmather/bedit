/*
 * bedit-plugins-engine.c
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * Based on gedit-plugins-engine.c from Gedit.
 *
 * Copyright (C) 2002 - Fernando Herrera, Kjartan Maraas
 * Copyright (C) 2002-2007 - Paolo Maggi
 * Copyright (C) 2004-2015 - Paolo Borelli
 * Copyright (C) 2006-2010 - Steve Frécinaux
 * Copyright (C) 2008-2010 - Jesse van den Kieboom
 * Copyright (C) 2008-2013 - Ignacio Casal Quinteiro
 * Copyright (C) 2009 - Javier Jardón
 * Copyright (C) 2010-2011 - Garrett Regier
 * Copyright (C) 2013-2019 - Sébastien Wilmet
 * Copyright (C) 2014 - Robert Roth
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

#include "config.h"

#include "bedit-plugins-engine.h"

#include <girepository.h>
#include <glib/gi18n.h>
#include <string.h>
#include "bedit-debug.h"
#include "bedit-dirs.h"
#include "bedit-settings.h"

struct _BeditPluginsEngine {
    PeasEngine parent_instance;

    GSettings *plugin_settings;
};

G_DEFINE_TYPE(BeditPluginsEngine, bedit_plugins_engine, PEAS_TYPE_ENGINE)

static BeditPluginsEngine *default_engine = NULL;

static void bedit_plugins_engine_init(BeditPluginsEngine *engine) {
    gchar *typelib_dir;
    GError *error = NULL;

    bedit_debug(DEBUG_PLUGINS);

    peas_engine_enable_loader(PEAS_ENGINE(engine), "python3");

    engine->plugin_settings = g_settings_new("com.bwhmather.bedit.plugins");

    /* Require bedit's typelib. */
    typelib_dir = g_build_filename(
        bedit_dirs_get_bedit_lib_dir(), "girepository-1.0", NULL);

    if (!g_irepository_require_private(
            g_irepository_get_default(), typelib_dir, "Bedit", "3.0", 0,
            &error)) {
        g_warning("Could not load Bedit repository: %s", error->message);
        g_error_free(error);
        error = NULL;
    }

    g_free(typelib_dir);

    /* This should be moved to libpeas */
    if (!g_irepository_require(
            g_irepository_get_default(), "Peas", "1.0", 0, &error)) {
        g_warning("Could not load Peas repository: %s", error->message);
        g_error_free(error);
        error = NULL;
    }

    if (!g_irepository_require(
            g_irepository_get_default(), "PeasGtk", "1.0", 0, &error)) {
        g_warning("Could not load PeasGtk repository: %s", error->message);
        g_error_free(error);
        error = NULL;
    }

    peas_engine_add_search_path(
        PEAS_ENGINE(engine), bedit_dirs_get_user_plugins_dir(),
        bedit_dirs_get_user_plugins_dir());

    peas_engine_add_search_path(
        PEAS_ENGINE(engine), bedit_dirs_get_bedit_plugins_dir(),
        bedit_dirs_get_bedit_plugins_data_dir());

    g_settings_bind(
        engine->plugin_settings, BEDIT_SETTINGS_ACTIVE_PLUGINS, engine,
        "loaded-plugins", G_SETTINGS_BIND_DEFAULT);
}

static void bedit_plugins_engine_dispose(GObject *object) {
    BeditPluginsEngine *engine = BEDIT_PLUGINS_ENGINE(object);

    g_clear_object(&engine->plugin_settings);

    G_OBJECT_CLASS(bedit_plugins_engine_parent_class)->dispose(object);
}

static void bedit_plugins_engine_class_init(BeditPluginsEngineClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->dispose = bedit_plugins_engine_dispose;
}

BeditPluginsEngine *bedit_plugins_engine_get_default(void) {
    if (default_engine == NULL) {
        default_engine =
            BEDIT_PLUGINS_ENGINE(g_object_new(BEDIT_TYPE_PLUGINS_ENGINE, NULL));

        g_object_add_weak_pointer(
            G_OBJECT(default_engine), (gpointer)&default_engine);
    }

    return default_engine;
}

