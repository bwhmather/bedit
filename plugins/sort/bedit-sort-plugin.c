/*
 * bedit-sort-plugin.c
 *
 * Original author: Carlo Borreo <borreo@softhome.net>
 * Ported to Bedit2 by Lee Mallabone <gnome@fonicmonkey.net>
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

#include "config.h"

#include "bedit-sort-plugin.h"

#include <string.h>
#include <glib/gi18n.h>

#include <bedit/bedit-debug.h>
#include <bedit/bedit-utils.h>
#include <bedit/bedit-app.h>
#include <bedit/bedit-window.h>
#include <bedit/bedit-app-activatable.h>
#include <bedit/bedit-window-activatable.h>

static void bedit_app_activatable_iface_init (BeditAppActivatableInterface *iface);
static void bedit_window_activatable_iface_init (BeditWindowActivatableInterface *iface);

struct _BeditSortPluginPrivate
{
	BeditWindow *window;

	GSimpleAction *action;
	GtkWidget *dialog;
	GtkWidget *col_num_spinbutton;
	GtkWidget *reverse_order_checkbutton;
	GtkWidget *case_checkbutton;
	GtkWidget *remove_dups_checkbutton;

	BeditApp *app;
	BeditMenuExtension *menu_ext;

	GtkTextIter start, end; /* selection */
};

enum
{
	PROP_0,
	PROP_WINDOW,
	PROP_APP
};

G_DEFINE_DYNAMIC_TYPE_EXTENDED (BeditSortPlugin,
				bedit_sort_plugin,
				PEAS_TYPE_EXTENSION_BASE,
				0,
				G_IMPLEMENT_INTERFACE_DYNAMIC (GEDIT_TYPE_APP_ACTIVATABLE,
							       bedit_app_activatable_iface_init)
				G_IMPLEMENT_INTERFACE_DYNAMIC (GEDIT_TYPE_WINDOW_ACTIVATABLE,
							       bedit_window_activatable_iface_init)
				G_ADD_PRIVATE_DYNAMIC (BeditSortPlugin))

static void
do_sort (BeditSortPlugin *plugin)
{
	BeditSortPluginPrivate *priv;
	BeditDocument *doc;
	GtkSourceSortFlags sort_flags = 0;
	gint starting_column;

	bedit_debug (DEBUG_PLUGINS);

	priv = plugin->priv;

	doc = bedit_window_get_active_document (priv->window);
	g_return_if_fail (doc != NULL);

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->case_checkbutton)))
	{
		sort_flags |= GTK_SOURCE_SORT_FLAGS_CASE_SENSITIVE;
	}

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->reverse_order_checkbutton)))
	{
		sort_flags |= GTK_SOURCE_SORT_FLAGS_REVERSE_ORDER;
	}

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->remove_dups_checkbutton)))
	{
		sort_flags |= GTK_SOURCE_SORT_FLAGS_REMOVE_DUPLICATES;
	}

	starting_column = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (priv->col_num_spinbutton)) - 1;

	gtk_source_buffer_sort_lines (GTK_SOURCE_BUFFER (doc),
	                              &priv->start,
	                              &priv->end,
	                              sort_flags,
	                              starting_column);

	bedit_debug_message (DEBUG_PLUGINS, "Done.");
}

static void
sort_dialog_response_handler (GtkDialog       *dlg,
			      gint             response,
			      BeditSortPlugin *plugin)
{
	bedit_debug (DEBUG_PLUGINS);

	if (response == GTK_RESPONSE_OK)
	{
		do_sort (plugin);
	}

	gtk_widget_destroy (GTK_WIDGET (dlg));
}

/* NOTE: we store the current selection in the dialog since focusing
 * the text field (like the combo box) looses the documnent selection.
 * Storing the selection ONLY works because the dialog is modal */
static void
get_current_selection (BeditSortPlugin *plugin)
{
	BeditSortPluginPrivate *priv;
	BeditDocument *doc;

	bedit_debug (DEBUG_PLUGINS);

	priv = plugin->priv;

	doc = bedit_window_get_active_document (priv->window);

	if (!gtk_text_buffer_get_selection_bounds (GTK_TEXT_BUFFER (doc),
						   &priv->start,
						   &priv->end))
	{
		/* No selection, get the whole document. */
		gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (doc),
					    &priv->start,
					    &priv->end);
	}
}

static void
create_sort_dialog (BeditSortPlugin *plugin)
{
	BeditSortPluginPrivate *priv;
	GtkBuilder *builder;

	bedit_debug (DEBUG_PLUGINS);

	priv = plugin->priv;

	builder = gtk_builder_new ();
	gtk_builder_add_from_resource (builder, "/com/bwhmather/bedit/plugins/sort/ui/bedit-sort-plugin.ui", NULL);
	priv->dialog = GTK_WIDGET (gtk_builder_get_object (builder, "sort_dialog"));
	priv->reverse_order_checkbutton = GTK_WIDGET (gtk_builder_get_object (builder, "reverse_order_checkbutton"));
	priv->col_num_spinbutton = GTK_WIDGET (gtk_builder_get_object (builder, "col_num_spinbutton"));
	priv->case_checkbutton = GTK_WIDGET (gtk_builder_get_object (builder, "case_checkbutton"));
	priv->remove_dups_checkbutton = GTK_WIDGET (gtk_builder_get_object (builder, "remove_dups_checkbutton"));
	g_object_unref (builder);

	gtk_dialog_set_default_response (GTK_DIALOG (priv->dialog),
					 GTK_RESPONSE_OK);

	g_signal_connect (priv->dialog,
			  "destroy",
			  G_CALLBACK (gtk_widget_destroyed),
			  &priv->dialog);

	g_signal_connect (priv->dialog,
			  "response",
			  G_CALLBACK (sort_dialog_response_handler),
			  plugin);

	get_current_selection (plugin);
}

static void
sort_cb (GAction         *action,
         GVariant        *parameter,
	 BeditSortPlugin *plugin)
{
	BeditSortPluginPrivate *priv;
	GtkWindowGroup *wg;

	bedit_debug (DEBUG_PLUGINS);

	priv = plugin->priv;

	create_sort_dialog (plugin);

	wg = bedit_window_get_group (priv->window);
	gtk_window_group_add_window (wg,
				     GTK_WINDOW (priv->dialog));

	gtk_window_set_transient_for (GTK_WINDOW (priv->dialog),
				      GTK_WINDOW (priv->window));

	gtk_window_set_modal (GTK_WINDOW (priv->dialog),
			      TRUE);

	gtk_widget_show (GTK_WIDGET (priv->dialog));
}

static void
update_ui (BeditSortPlugin *plugin)
{
	BeditView *view;

	bedit_debug (DEBUG_PLUGINS);

	view = bedit_window_get_active_view (plugin->priv->window);

	g_simple_action_set_enabled (plugin->priv->action,
	                             (view != NULL) &&
	                             gtk_text_view_get_editable (GTK_TEXT_VIEW (view)));
}

static void
bedit_sort_plugin_app_activate (BeditAppActivatable *activatable)
{
	BeditSortPluginPrivate *priv;
	GMenuItem *item;

	bedit_debug (DEBUG_PLUGINS);

	priv = GEDIT_SORT_PLUGIN (activatable)->priv;

	priv->menu_ext = bedit_app_activatable_extend_menu (activatable, "tools-section");
	item = g_menu_item_new (_("S_ort…"), "win.sort");
	bedit_menu_extension_append_menu_item (priv->menu_ext, item);
	g_object_unref (item);
}

static void
bedit_sort_plugin_app_deactivate (BeditAppActivatable *activatable)
{
	BeditSortPluginPrivate *priv;

	bedit_debug (DEBUG_PLUGINS);

	priv = GEDIT_SORT_PLUGIN (activatable)->priv;

	g_clear_object (&priv->menu_ext);
}

static void
bedit_sort_plugin_window_activate (BeditWindowActivatable *activatable)
{
	BeditSortPluginPrivate *priv;

	bedit_debug (DEBUG_PLUGINS);

	priv = GEDIT_SORT_PLUGIN (activatable)->priv;

	priv->action = g_simple_action_new ("sort", NULL);
	g_signal_connect (priv->action, "activate",
	                  G_CALLBACK (sort_cb), activatable);
	g_action_map_add_action (G_ACTION_MAP (priv->window),
	                         G_ACTION (priv->action));

	update_ui (GEDIT_SORT_PLUGIN (activatable));
}

static void
bedit_sort_plugin_window_deactivate (BeditWindowActivatable *activatable)
{
	BeditSortPluginPrivate *priv;

	bedit_debug (DEBUG_PLUGINS);

	priv = GEDIT_SORT_PLUGIN (activatable)->priv;
	g_action_map_remove_action (G_ACTION_MAP (priv->window), "sort");
}

static void
bedit_sort_plugin_window_update_state (BeditWindowActivatable *activatable)
{
	bedit_debug (DEBUG_PLUGINS);

	update_ui (GEDIT_SORT_PLUGIN (activatable));
}

static void
bedit_sort_plugin_init (BeditSortPlugin *plugin)
{
	bedit_debug_message (DEBUG_PLUGINS, "BeditSortPlugin initializing");

	plugin->priv = bedit_sort_plugin_get_instance_private (plugin);
}

static void
bedit_sort_plugin_dispose (GObject *object)
{
	BeditSortPlugin *plugin = GEDIT_SORT_PLUGIN (object);

	bedit_debug_message (DEBUG_PLUGINS, "BeditSortPlugin disposing");

	g_clear_object (&plugin->priv->action);
	g_clear_object (&plugin->priv->window);
	g_clear_object (&plugin->priv->menu_ext);
	g_clear_object (&plugin->priv->app);

	G_OBJECT_CLASS (bedit_sort_plugin_parent_class)->dispose (object);
}


static void
bedit_sort_plugin_finalize (GObject *object)
{
	bedit_debug_message (DEBUG_PLUGINS, "BeditSortPlugin finalizing");

	G_OBJECT_CLASS (bedit_sort_plugin_parent_class)->finalize (object);
}

static void
bedit_sort_plugin_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
	BeditSortPlugin *plugin = GEDIT_SORT_PLUGIN (object);

	switch (prop_id)
	{
		case PROP_WINDOW:
			plugin->priv->window = GEDIT_WINDOW (g_value_dup_object (value));
			break;
		case PROP_APP:
			plugin->priv->app = GEDIT_APP (g_value_dup_object (value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
bedit_sort_plugin_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
	BeditSortPlugin *plugin = GEDIT_SORT_PLUGIN (object);

	switch (prop_id)
	{
		case PROP_WINDOW:
			g_value_set_object (value, plugin->priv->window);
			break;
		case PROP_APP:
			g_value_set_object (value, plugin->priv->app);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
bedit_sort_plugin_class_init (BeditSortPluginClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = bedit_sort_plugin_dispose;
	object_class->finalize = bedit_sort_plugin_finalize;
	object_class->set_property = bedit_sort_plugin_set_property;
	object_class->get_property = bedit_sort_plugin_get_property;

	g_object_class_override_property (object_class, PROP_WINDOW, "window");
	g_object_class_override_property (object_class, PROP_APP, "app");
}

static void
bedit_sort_plugin_class_finalize (BeditSortPluginClass *klass)
{
}

static void
bedit_app_activatable_iface_init (BeditAppActivatableInterface *iface)
{
	iface->activate = bedit_sort_plugin_app_activate;
	iface->deactivate = bedit_sort_plugin_app_deactivate;
}

static void
bedit_window_activatable_iface_init (BeditWindowActivatableInterface *iface)
{
	iface->activate = bedit_sort_plugin_window_activate;
	iface->deactivate = bedit_sort_plugin_window_deactivate;
	iface->update_state = bedit_sort_plugin_window_update_state;
}

G_MODULE_EXPORT void
peas_register_types (PeasObjectModule *module)
{
	bedit_sort_plugin_register_type (G_TYPE_MODULE (module));

	peas_object_module_register_extension_type (module,
						    GEDIT_TYPE_APP_ACTIVATABLE,
						    GEDIT_TYPE_SORT_PLUGIN);
	peas_object_module_register_extension_type (module,
						    GEDIT_TYPE_WINDOW_ACTIVATABLE,
						    GEDIT_TYPE_SORT_PLUGIN);
}

/* ex:set ts=8 noet: */
