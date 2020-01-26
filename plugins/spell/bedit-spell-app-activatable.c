/*
 * bedit-spell-app-activatable.c
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

#include "bedit-spell-app-activatable.h"
#include <bedit/bedit-app-activatable.h>
#include <bedit/bedit-app.h>
#include <glib/gi18n.h>
#include <libpeas/peas-object-module.h>

typedef struct _BeditSpellAppActivatablePrivate BeditSpellAppActivatablePrivate;

struct _BeditSpellAppActivatable {
    GObject parent;
};

struct _BeditSpellAppActivatablePrivate {
    BeditApp *app;
    BeditMenuExtension *menu_ext;
};

enum { PROP_0, PROP_APP };

static void bedit_app_activatable_iface_init(
    BeditAppActivatableInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED(
    BeditSpellAppActivatable, bedit_spell_app_activatable, G_TYPE_OBJECT, 0,
    G_ADD_PRIVATE_DYNAMIC(BeditSpellAppActivatable)
        G_IMPLEMENT_INTERFACE_DYNAMIC(
            GEDIT_TYPE_APP_ACTIVATABLE, bedit_app_activatable_iface_init))

static void bedit_spell_app_activatable_dispose(GObject *object) {
    BeditSpellAppActivatable *activatable = GEDIT_SPELL_APP_ACTIVATABLE(object);
    BeditSpellAppActivatablePrivate *priv =
        bedit_spell_app_activatable_get_instance_private(activatable);

    g_clear_object(&priv->app);
    g_clear_object(&priv->menu_ext);

    G_OBJECT_CLASS(bedit_spell_app_activatable_parent_class)->dispose(object);
}

static void bedit_spell_app_activatable_set_property(
    GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) {
    BeditSpellAppActivatable *activatable = GEDIT_SPELL_APP_ACTIVATABLE(object);
    BeditSpellAppActivatablePrivate *priv =
        bedit_spell_app_activatable_get_instance_private(activatable);

    switch (prop_id) {
    case PROP_APP:
        priv->app = GEDIT_APP(g_value_dup_object(value));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void bedit_spell_app_activatable_get_property(
    GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) {
    BeditSpellAppActivatable *activatable = GEDIT_SPELL_APP_ACTIVATABLE(object);
    BeditSpellAppActivatablePrivate *priv =
        bedit_spell_app_activatable_get_instance_private(activatable);

    switch (prop_id) {
    case PROP_APP:
        g_value_set_object(value, priv->app);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void bedit_spell_app_activatable_class_init(
    BeditSpellAppActivatableClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->dispose = bedit_spell_app_activatable_dispose;
    object_class->set_property = bedit_spell_app_activatable_set_property;
    object_class->get_property = bedit_spell_app_activatable_get_property;

    g_object_class_override_property(object_class, PROP_APP, "app");
}

static void bedit_spell_app_activatable_class_finalize(
    BeditSpellAppActivatableClass *klass) {}

static void bedit_spell_app_activatable_init(BeditSpellAppActivatable *self) {}

static void bedit_spell_app_activatable_activate(
    BeditAppActivatable *activatable) {
    BeditSpellAppActivatable *app_activatable =
        GEDIT_SPELL_APP_ACTIVATABLE(activatable);
    BeditSpellAppActivatablePrivate *priv =
        bedit_spell_app_activatable_get_instance_private(app_activatable);
    GMenuItem *item;

    const gchar *accels[] = {"<Shift>F7", NULL};
    gtk_application_set_accels_for_action(
        GTK_APPLICATION(priv->app), "win.check-spell", accels);
    priv->menu_ext =
        bedit_app_activatable_extend_menu(activatable, "spell-section");

    item = g_menu_item_new(_("_Check Spelling…"), "win.check-spell");
    bedit_menu_extension_append_menu_item(priv->menu_ext, item);
    g_object_unref(item);

    item = g_menu_item_new(_("Set _Language…"), "win.config-spell");
    bedit_menu_extension_append_menu_item(priv->menu_ext, item);
    g_object_unref(item);

    item = g_menu_item_new(
        _("_Highlight Misspelled Words"), "win.inline-spell-checker");
    bedit_menu_extension_append_menu_item(priv->menu_ext, item);
    g_object_unref(item);
}

static void bedit_spell_app_activatable_deactivate(
    BeditAppActivatable *activatable) {
    BeditSpellAppActivatable *app_activatable =
        GEDIT_SPELL_APP_ACTIVATABLE(activatable);
    BeditSpellAppActivatablePrivate *priv =
        bedit_spell_app_activatable_get_instance_private(app_activatable);

    const gchar *accels[] = {NULL};
    gtk_application_set_accels_for_action(
        GTK_APPLICATION(priv->app), "win.check-spell", accels);
    g_clear_object(&priv->menu_ext);
}

static void bedit_app_activatable_iface_init(
    BeditAppActivatableInterface *iface) {
    iface->activate = bedit_spell_app_activatable_activate;
    iface->deactivate = bedit_spell_app_activatable_deactivate;
}

void bedit_spell_app_activatable_register(GTypeModule *module) {
    bedit_spell_app_activatable_register_type(module);

    peas_object_module_register_extension_type(
        PEAS_OBJECT_MODULE(module), GEDIT_TYPE_APP_ACTIVATABLE,
        GEDIT_TYPE_SPELL_APP_ACTIVATABLE);
}

/* ex:set ts=8 noet: */
