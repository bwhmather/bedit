/*
 * gedit-changecase-plugin.c
 *
 * Copyright (C) 2004-2010 - Paolo Borelli
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gedit-changecase-plugin.h"

#include <glib/gi18n.h>
#include <gmodule.h>

#include <gedit/gedit-window.h>
#include <gedit/gedit-window-activatable.h>
#include <gedit/gedit-debug.h>

struct _GeditChangecasePluginPrivate
{
	GeditWindow    *window;

	GtkActionGroup *action_group;
	guint           ui_id;
};

enum
{
	PROP_0,
	PROP_WINDOW
};

static void gedit_window_activatable_iface_init (GeditWindowActivatableInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (GeditChangecasePlugin,
				gedit_changecase_plugin,
				PEAS_TYPE_EXTENSION_BASE,
				0,
				G_IMPLEMENT_INTERFACE_DYNAMIC (GEDIT_TYPE_WINDOW_ACTIVATABLE,
							       gedit_window_activatable_iface_init))

static void
change_case (GeditWindow             *window,
             GtkSourceChangeCaseType  choice)
{
	GeditDocument *doc;
	GtkTextIter start, end;

	gedit_debug (DEBUG_PLUGINS);

	doc = gedit_window_get_active_document (window);
	g_return_if_fail (doc != NULL);

	if (!gtk_text_buffer_get_selection_bounds (GTK_TEXT_BUFFER (doc),
						   &start, &end))
	{
		return;
	}

	gtk_source_buffer_change_case (GTK_SOURCE_BUFFER (doc), choice, &start, &end);
}

static void
upper_case_cb (GtkAction   *action,
               GeditWindow *window)
{
	change_case (window, GTK_SOURCE_CHANGE_CASE_UPPER);
}

static void
lower_case_cb (GtkAction   *action,
               GeditWindow *window)
{
	change_case (window, GTK_SOURCE_CHANGE_CASE_LOWER);
}

static void
invert_case_cb (GtkAction   *action,
                GeditWindow *window)
{
	change_case (window, GTK_SOURCE_CHANGE_CASE_TOGGLE);
}

static void
title_case_cb (GtkAction   *action,
               GeditWindow *window)
{
	change_case (window, GTK_SOURCE_CHANGE_CASE_TITLE);
}

static const GtkActionEntry action_entries[] =
{
	{ "ChangeCase", NULL, N_("C_hange Case") },
	{ "UpperCase", NULL, N_("All _Upper Case"), NULL,
	  N_("Change selected text to upper case"),
	  G_CALLBACK (upper_case_cb) },
	{ "LowerCase", NULL, N_("All _Lower Case"), NULL,
	  N_("Change selected text to lower case"),
	  G_CALLBACK (lower_case_cb) },
	{ "InvertCase", NULL, N_("_Invert Case"), NULL,
	  N_("Invert the case of selected text"),
	  G_CALLBACK (invert_case_cb) },
	{ "TitleCase", NULL, N_("_Title Case"), NULL,
	  N_("Capitalize the first letter of each selected word"),
	  G_CALLBACK (title_case_cb) }
};

const gchar submenu[] =
"<ui>"
"  <menubar name='MenuBar'>"
"    <menu name='EditMenu' action='Edit'>"
"      <placeholder name='EditOps_6'>"
"        <menu action='ChangeCase'>"
"          <menuitem action='UpperCase'/>"
"          <menuitem action='LowerCase'/>"
"          <menuitem action='InvertCase'/>"
"          <menuitem action='TitleCase'/>"
"        </menu>"
"      </placeholder>"
"    </menu>"
"  </menubar>"
"</ui>";

static void
gedit_changecase_plugin_init (GeditChangecasePlugin *plugin)
{
	gedit_debug_message (DEBUG_PLUGINS, "GeditChangecasePlugin initializing");

	plugin->priv = G_TYPE_INSTANCE_GET_PRIVATE (plugin,
						    GEDIT_TYPE_CHANGECASE_PLUGIN,
						    GeditChangecasePluginPrivate);
}

static void
gedit_changecase_plugin_dispose (GObject *object)
{
	GeditChangecasePlugin *plugin = GEDIT_CHANGECASE_PLUGIN (object);

	gedit_debug_message (DEBUG_PLUGINS, "GeditChangecasePlugin disponsing");

	g_clear_object (&plugin->priv->window);
	g_clear_object (&plugin->priv->action_group);

	G_OBJECT_CLASS (gedit_changecase_plugin_parent_class)->dispose (object);
}

static void
gedit_changecase_plugin_finalize (GObject *object)
{
	G_OBJECT_CLASS (gedit_changecase_plugin_parent_class)->finalize (object);

	gedit_debug_message (DEBUG_PLUGINS, "GeditChangecasePlugin finalizing");
}

static void
gedit_changecase_plugin_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
	GeditChangecasePlugin *plugin = GEDIT_CHANGECASE_PLUGIN (object);

	switch (prop_id)
	{
		case PROP_WINDOW:
			plugin->priv->window = GEDIT_WINDOW (g_value_dup_object (value));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gedit_changecase_plugin_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
	GeditChangecasePlugin *plugin = GEDIT_CHANGECASE_PLUGIN (object);

	switch (prop_id)
	{
		case PROP_WINDOW:
			g_value_set_object (value, plugin->priv->window);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
update_ui (GeditChangecasePlugin *plugin)
{
	GtkTextView *view;
	GtkAction *action;
	gboolean sensitive = FALSE;

	gedit_debug (DEBUG_PLUGINS);

	view = GTK_TEXT_VIEW (gedit_window_get_active_view (plugin->priv->window));

	if (view != NULL)
	{
		GtkTextBuffer *buffer;

		buffer = gtk_text_view_get_buffer (view);
		sensitive = (gtk_text_view_get_editable (view) &&
			     gtk_text_buffer_get_has_selection (buffer));
	}

	action = gtk_action_group_get_action (plugin->priv->action_group,
					      "ChangeCase");
	gtk_action_set_sensitive (action, sensitive);
}

static void
gedit_changecase_plugin_activate (GeditWindowActivatable *activatable)
{
	GeditChangecasePluginPrivate *priv;
	GtkUIManager *manager;
	GError *error = NULL;

	gedit_debug (DEBUG_PLUGINS);

	priv = GEDIT_CHANGECASE_PLUGIN (activatable)->priv;

	manager = gedit_window_get_ui_manager (priv->window);

	priv->action_group = gtk_action_group_new ("GeditChangecasePluginActions");
	gtk_action_group_set_translation_domain (priv->action_group,
						 GETTEXT_PACKAGE);
	gtk_action_group_add_actions (priv->action_group,
				      action_entries,
				      G_N_ELEMENTS (action_entries),
				      priv->window);

	gtk_ui_manager_insert_action_group (manager, priv->action_group, -1);

	priv->ui_id = gtk_ui_manager_add_ui_from_string (manager,
							 submenu,
							 -1,
							 &error);
	if (priv->ui_id == 0)
	{
		g_warning ("%s", error->message);
		return;
	}

	update_ui (GEDIT_CHANGECASE_PLUGIN (activatable));
}

static void
gedit_changecase_plugin_deactivate (GeditWindowActivatable *activatable)
{
	GeditChangecasePluginPrivate *priv;
	GtkUIManager *manager;

	gedit_debug (DEBUG_PLUGINS);

	priv = GEDIT_CHANGECASE_PLUGIN (activatable)->priv;

	manager = gedit_window_get_ui_manager (priv->window);

	gtk_ui_manager_remove_ui (manager, priv->ui_id);
	gtk_ui_manager_remove_action_group (manager, priv->action_group);
}

static void
gedit_changecase_plugin_update_state (GeditWindowActivatable *activatable)
{
	gedit_debug (DEBUG_PLUGINS);

	update_ui (GEDIT_CHANGECASE_PLUGIN (activatable));
}

static void
gedit_changecase_plugin_class_init (GeditChangecasePluginClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = gedit_changecase_plugin_dispose;
	object_class->finalize = gedit_changecase_plugin_finalize;
	object_class->set_property = gedit_changecase_plugin_set_property;
	object_class->get_property = gedit_changecase_plugin_get_property;

	g_object_class_override_property (object_class, PROP_WINDOW, "window");

	g_type_class_add_private (klass, sizeof (GeditChangecasePluginPrivate));
}

static void
gedit_window_activatable_iface_init (GeditWindowActivatableInterface *iface)
{
	iface->activate = gedit_changecase_plugin_activate;
	iface->deactivate = gedit_changecase_plugin_deactivate;
	iface->update_state = gedit_changecase_plugin_update_state;
}

static void
gedit_changecase_plugin_class_finalize (GeditChangecasePluginClass *klass)
{
}


G_MODULE_EXPORT void
peas_register_types (PeasObjectModule *module)
{
	gedit_changecase_plugin_register_type (G_TYPE_MODULE (module));

	peas_object_module_register_extension_type (module,
						    GEDIT_TYPE_WINDOW_ACTIVATABLE,
						    GEDIT_TYPE_CHANGECASE_PLUGIN);
}

/* ex:set ts=8 noet: */
