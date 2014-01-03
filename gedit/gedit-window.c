/*
 * gedit-window.c
 * This file is part of gedit
 *
 * Copyright (C) 2005 - Paolo Maggi
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the gedit Team, 2005. See the AUTHORS file for a
 * list of people on the gedit Team.
 * See the ChangeLog files for a list of changes.
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <time.h>
#include <sys/types.h>
#include <string.h>

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gtksourceview/gtksource.h>
#include <libpeas/peas-extension-set.h>

#include "gedit-window.h"
#include "gedit-window-private.h"
#include "gedit-app.h"
#include "gedit-notebook.h"
#include "gedit-notebook-popup-menu.h"
#include "gedit-multi-notebook.h"
#include "gedit-statusbar.h"
#include "gedit-utils.h"
#include "gedit-commands.h"
#include "gedit-debug.h"
#include "gedit-open-menu-button.h"
#include "gedit-panel.h"
#include "gedit-documents-panel.h"
#include "gedit-plugins-engine.h"
#include "gedit-window-activatable.h"
#include "gedit-enum-types.h"
#include "gedit-dirs.h"
#include "gedit-status-menu-button.h"
#include "gedit-settings.h"
#include "gedit-marshal.h"
#include "gedit-document.h"
#include "gedit-small-button.h"

#define TAB_WIDTH_DATA "GeditWindowTabWidthData"
#define FULLSCREEN_ANIMATION_SPEED 4

/* Signals */
enum
{
	TAB_ADDED,
	TAB_REMOVED,
	TABS_REORDERED,
	ACTIVE_TAB_CHANGED,
	ACTIVE_TAB_STATE_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

enum
{
	PROP_0,
	PROP_STATE
};

enum
{
	TARGET_URI_LIST = 100,
	TARGET_XDNDDIRECTSAVE
};

static const GtkTargetEntry drop_types [] = {
	{ "XdndDirectSave0", 0, TARGET_XDNDDIRECTSAVE }, /* XDS Protocol Type */
	{ "text/uri-list", 0, TARGET_URI_LIST}
};

G_DEFINE_TYPE_WITH_PRIVATE (GeditWindow, gedit_window, GTK_TYPE_APPLICATION_WINDOW)

static void
gedit_window_get_property (GObject    *object,
			   guint       prop_id,
			   GValue     *value,
			   GParamSpec *pspec)
{
	GeditWindow *window = GEDIT_WINDOW (object);

	switch (prop_id)
	{
		case PROP_STATE:
			g_value_set_enum (value,
					  gedit_window_get_state (window));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
save_panels_state (GeditWindow *window)
{
	gint panel_page;

	gedit_debug (DEBUG_WINDOW);

	if (window->priv->side_panel_size > 0)
	{
		g_settings_set_int (window->priv->window_settings,
				    GEDIT_SETTINGS_SIDE_PANEL_SIZE,
				    window->priv->side_panel_size);
	}

	panel_page = _gedit_panel_get_active_item_id (GEDIT_PANEL (window->priv->side_panel));
	if (panel_page != 0)
	{
		g_settings_set_int (window->priv->window_settings,
				    GEDIT_SETTINGS_SIDE_PANEL_ACTIVE_PAGE,
				    panel_page);
	}

	if (window->priv->bottom_panel_size > 0)
	{
		g_settings_set_int (window->priv->window_settings,
				    GEDIT_SETTINGS_BOTTOM_PANEL_SIZE,
				    window->priv->bottom_panel_size);
	}

	panel_page = _gedit_panel_get_active_item_id (GEDIT_PANEL (window->priv->bottom_panel));
	if (panel_page != 0)
	{
		g_settings_set_int (window->priv->window_settings,
				    GEDIT_SETTINGS_BOTTOM_PANEL_ACTIVE_PAGE, panel_page);
	}

	g_settings_apply (window->priv->window_settings);
}

static void
save_window_state (GtkWidget *widget)
{
	GeditWindow *window = GEDIT_WINDOW (widget);

	if ((window->priv->window_state &
	    (GDK_WINDOW_STATE_MAXIMIZED | GDK_WINDOW_STATE_FULLSCREEN)) == 0)
	{
		GtkAllocation allocation;

		gtk_widget_get_allocation (widget, &allocation);

		window->priv->width = allocation.width;
		window->priv->height = allocation.height;

		g_settings_set (window->priv->window_settings, GEDIT_SETTINGS_WINDOW_SIZE,
				"(ii)", window->priv->width, window->priv->height);
	}
}

static void
gedit_window_dispose (GObject *object)
{
	GeditWindow *window;

	gedit_debug (DEBUG_WINDOW);

	window = GEDIT_WINDOW (object);

	/* Stop tracking removal of panels otherwise we always
	 * end up with thinking we had no panel active, since they
	 * should all be removed below */
	if (window->priv->bottom_panel_item_removed_handler_id != 0)
	{
		g_signal_handler_disconnect (window->priv->bottom_panel,
					     window->priv->bottom_panel_item_removed_handler_id);
		window->priv->bottom_panel_item_removed_handler_id = 0;
	}

	/* First of all, force collection so that plugins
	 * really drop some of the references.
	 */
	peas_engine_garbage_collect (PEAS_ENGINE (gedit_plugins_engine_get_default ()));

	/* save the panels position and make sure to deactivate plugins
	 * for this window, but only once */
	if (!window->priv->dispose_has_run)
	{
		save_window_state (GTK_WIDGET (window));
		save_panels_state (window);

		/* Note that unreffing the extensions will automatically remove
		   all extensions which in turn will deactivate the extension */
		g_object_unref (window->priv->extensions);

		peas_engine_garbage_collect (PEAS_ENGINE (gedit_plugins_engine_get_default ()));

		window->priv->dispose_has_run = TRUE;
	}

	if (window->priv->fullscreen_animation_timeout_id != 0)
	{
		g_source_remove (window->priv->fullscreen_animation_timeout_id);
		window->priv->fullscreen_animation_timeout_id = 0;
	}

	if (window->priv->fullscreen_controls != NULL)
	{
		gtk_widget_destroy (window->priv->fullscreen_controls);

		window->priv->fullscreen_controls = NULL;
	}

	if (window->priv->update_documents_list_menu_id != 0)
	{
		g_source_remove (window->priv->update_documents_list_menu_id);
		window->priv->update_documents_list_menu_id = 0;
	}

	g_clear_object (&window->priv->manager);
	g_clear_object (&window->priv->message_bus);
	g_clear_object (&window->priv->window_group);
	g_clear_object (&window->priv->default_location);

	/* We must free the settings after saving the panels */
	g_clear_object (&window->priv->editor_settings);
	g_clear_object (&window->priv->ui_settings);
	g_clear_object (&window->priv->window_settings);

	/* Now that there have broken some reference loops,
	 * force collection again.
	 */
	peas_engine_garbage_collect (PEAS_ENGINE (gedit_plugins_engine_get_default ()));

	G_OBJECT_CLASS (gedit_window_parent_class)->dispose (object);
}

static void
gedit_window_finalize (GObject *object)
{
	gedit_debug (DEBUG_WINDOW);

	G_OBJECT_CLASS (gedit_window_parent_class)->finalize (object);
}

static gboolean
gedit_window_window_state_event (GtkWidget           *widget,
				 GdkEventWindowState *event)
{
	GeditWindow *window = GEDIT_WINDOW (widget);

	window->priv->window_state = event->new_window_state;

	g_settings_set_int (window->priv->window_settings, GEDIT_SETTINGS_WINDOW_STATE,
			    window->priv->window_state);

	return GTK_WIDGET_CLASS (gedit_window_parent_class)->window_state_event (widget, event);
}

static gboolean
gedit_window_configure_event (GtkWidget         *widget,
			      GdkEventConfigure *event)
{
	GeditWindow *window = GEDIT_WINDOW (widget);

	if (gtk_widget_get_realized (widget) &&
	    (window->priv->window_state &
	    (GDK_WINDOW_STATE_MAXIMIZED | GDK_WINDOW_STATE_FULLSCREEN)) == 0)
	{
		save_window_state (widget);
	}

	return GTK_WIDGET_CLASS (gedit_window_parent_class)->configure_event (widget, event);
}

/*
 * GtkWindow catches keybindings for the menu items _before_ passing them to
 * the focused widget. This is unfortunate and means that pressing ctrl+V
 * in an entry on a panel ends up pasting text in the TextView.
 * Here we override GtkWindow's handler to do the same things that it
 * does, but in the opposite order and then we chain up to the grand
 * parent handler, skipping gtk_window_key_press_event.
 */
static gboolean
gedit_window_key_press_event (GtkWidget   *widget,
			      GdkEventKey *event)
{
	static gpointer grand_parent_class = NULL;

	GtkWindow *window = GTK_WINDOW (widget);
	gboolean handled = FALSE;

	if (grand_parent_class == NULL)
	{
		grand_parent_class = g_type_class_peek_parent (gedit_window_parent_class);
	}

	/* handle focus widget key events */
	if (!handled)
	{
		handled = gtk_window_propagate_key_event (window, event);
	}

	/* handle mnemonics and accelerators */
	if (!handled)
	{
		handled = gtk_window_activate_key (window, event);
	}

	/* Chain up, invokes binding set on window */
	if (!handled)
	{
		handled = GTK_WIDGET_CLASS (grand_parent_class)->key_press_event (widget, event);
	}

	if (!handled)
	{
		return gedit_app_process_window_event (GEDIT_APP (g_application_get_default ()),
		                                       GEDIT_WINDOW (widget),
		                                       (GdkEvent *)event);
	}

	return TRUE;
}

static void
gedit_window_tab_removed (GeditWindow *window,
			  GeditTab    *tab)
{
	peas_engine_garbage_collect (PEAS_ENGINE (gedit_plugins_engine_get_default ()));
}

static void
gedit_window_class_init (GeditWindowClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	klass->tab_removed = gedit_window_tab_removed;

	object_class->dispose = gedit_window_dispose;
	object_class->finalize = gedit_window_finalize;
	object_class->get_property = gedit_window_get_property;

	widget_class->window_state_event = gedit_window_window_state_event;
	widget_class->configure_event = gedit_window_configure_event;
	widget_class->key_press_event = gedit_window_key_press_event;

	signals[TAB_ADDED] =
		g_signal_new ("tab-added",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GeditWindowClass, tab_added),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1,
			      GEDIT_TYPE_TAB);
	signals[TAB_REMOVED] =
		g_signal_new ("tab-removed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GeditWindowClass, tab_removed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1,
			      GEDIT_TYPE_TAB);
	signals[TABS_REORDERED] =
		g_signal_new ("tabs-reordered",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GeditWindowClass, tabs_reordered),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);
	signals[ACTIVE_TAB_CHANGED] =
		g_signal_new ("active-tab-changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GeditWindowClass, active_tab_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1,
			      GEDIT_TYPE_TAB);
	signals[ACTIVE_TAB_STATE_CHANGED] =
		g_signal_new ("active-tab-state-changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GeditWindowClass, active_tab_state_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);

	g_object_class_install_property (object_class,
					 PROP_STATE,
					 g_param_spec_flags ("state",
							     "State",
							     "The window's state",
							     GEDIT_TYPE_WINDOW_STATE,
							     GEDIT_WINDOW_STATE_NORMAL,
							     G_PARAM_READABLE |
							     G_PARAM_STATIC_STRINGS));

	/* Bind class to template */
	gtk_widget_class_set_template_from_resource (widget_class,
	                                             "/org/gnome/gedit/ui/gedit-window.ui");
	gtk_widget_class_bind_template_child_private (widget_class, GeditWindow, titlebar_paned);
	gtk_widget_class_bind_template_child_private (widget_class, GeditWindow, side_headerbar);
	gtk_widget_class_bind_template_child_private (widget_class, GeditWindow, headerbar);
	gtk_widget_class_bind_template_child_private (widget_class, GeditWindow, open_menu);
	gtk_widget_class_bind_template_child_private (widget_class, GeditWindow, gear_menu);
	gtk_widget_class_bind_template_child_private (widget_class, GeditWindow, hpaned);
	gtk_widget_class_bind_template_child_private (widget_class, GeditWindow, side_panel);
	gtk_widget_class_bind_template_child_private (widget_class, GeditWindow, vpaned);
	gtk_widget_class_bind_template_child_private (widget_class, GeditWindow, multi_notebook);
	gtk_widget_class_bind_template_child_private (widget_class, GeditWindow, bottom_panel);
	gtk_widget_class_bind_template_child_private (widget_class, GeditWindow, statusbar);
	gtk_widget_class_bind_template_child_private (widget_class, GeditWindow, fullscreen_controls);
}

static void
update_next_prev_doc_sensitivity (GeditWindow *window,
				  GeditTab    *tab)
{
	GeditNotebook *notebook;
	gint tab_number;
	GAction *action;

	gedit_debug (DEBUG_WINDOW);

	notebook = gedit_multi_notebook_get_active_notebook (window->priv->multi_notebook);
	tab_number = gtk_notebook_page_num (GTK_NOTEBOOK (notebook), GTK_WIDGET (tab));

	g_return_if_fail (tab_number >= 0);

	action = g_action_map_lookup_action (G_ACTION_MAP (window),
	                                     "previous_document");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
	                             tab_number != 0);

	action = g_action_map_lookup_action (G_ACTION_MAP (window),
	                                     "next_document");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
	                             tab_number < gtk_notebook_get_n_pages (GTK_NOTEBOOK (notebook)) - 1);
}

static void
update_next_prev_doc_sensitivity_per_window (GeditWindow *window)
{
	GeditTab *tab;
	GAction *action;

	gedit_debug (DEBUG_WINDOW);

	tab = gedit_window_get_active_tab (window);
	if (tab != NULL)
	{
		update_next_prev_doc_sensitivity (window, tab);

		return;
	}

	action = g_action_map_lookup_action (G_ACTION_MAP (window),
	                                     "previous_document");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);

	action = g_action_map_lookup_action (G_ACTION_MAP (window),
	                                     "next_document");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);
}

static void
received_clipboard_contents (GtkClipboard     *clipboard,
			     GtkSelectionData *selection_data,
			     GeditWindow      *window)
{
	GeditTab *tab;
	gboolean enabled;
	GAction *action;

	/* getting clipboard contents is async, so we need to
	 * get the current tab and its state */

	tab = gedit_window_get_active_tab (window);

	if (tab != NULL)
	{
		GeditTabState state;
		gboolean state_normal;

		state = gedit_tab_get_state (tab);
		state_normal = (state == GEDIT_TAB_STATE_NORMAL);

		enabled = state_normal &&
		          gtk_selection_data_targets_include_text (selection_data);
	}
	else
	{
		enabled = FALSE;
	}

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "paste");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), enabled);

	g_object_unref (window);
}

static void
set_paste_sensitivity_according_to_clipboard (GeditWindow  *window,
					      GtkClipboard *clipboard)
{
	GdkDisplay *display;

	display = gtk_clipboard_get_display (clipboard);

	if (gdk_display_supports_selection_notification (display))
	{
		gtk_clipboard_request_contents (clipboard,
						gdk_atom_intern_static_string ("TARGETS"),
						(GtkClipboardReceivedFunc) received_clipboard_contents,
						g_object_ref (window));
	}
	else
	{
		GAction *action;

		action = g_action_map_lookup_action (G_ACTION_MAP (window), "paste");
		/* XFIXES extension not availbale, make
		 * Paste always sensitive */
		g_simple_action_set_enabled (G_SIMPLE_ACTION (action), TRUE);
	}
}

static void
extension_update_state (PeasExtensionSet *extensions,
		        PeasPluginInfo   *info,
		        PeasExtension    *exten,
		        GeditWindow      *window)
{
	gedit_window_activatable_update_state (GEDIT_WINDOW_ACTIVATABLE (exten));
}

static void
set_sensitivity_according_to_tab (GeditWindow *window,
				  GeditTab    *tab)
{
	GeditDocument *doc;
	GeditView *view;
	GAction *action;
	gboolean b;
	gboolean state_normal;
	gboolean editable;
	GeditTabState state;
	GtkClipboard *clipboard;
	GeditLockdownMask lockdown;
	gboolean enable_syntax_highlighting;

	g_return_if_fail (GEDIT_TAB (tab));

	gedit_debug (DEBUG_WINDOW);

	enable_syntax_highlighting = g_settings_get_boolean (window->priv->editor_settings,
							     GEDIT_SETTINGS_SYNTAX_HIGHLIGHTING);

	lockdown = gedit_app_get_lockdown (GEDIT_APP (g_application_get_default ()));

	state = gedit_tab_get_state (tab);
	state_normal = (state == GEDIT_TAB_STATE_NORMAL);

	view = gedit_tab_get_view (tab);
	editable = gtk_text_view_get_editable (GTK_TEXT_VIEW (view));

	doc = GEDIT_DOCUMENT (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)));

	clipboard = gtk_widget_get_clipboard (GTK_WIDGET (window),
					      GDK_SELECTION_CLIPBOARD);

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "save");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
				     (state_normal ||
				      (state == GEDIT_TAB_STATE_EXTERNALLY_MODIFIED_NOTIFICATION) ||
				      (state == GEDIT_TAB_STATE_SHOWING_PRINT_PREVIEW)) &&
				     !gedit_document_get_readonly (doc) &&
				     !(lockdown & GEDIT_LOCKDOWN_SAVE_TO_DISK));

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "save_as");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
				     (state_normal ||
				      (state == GEDIT_TAB_STATE_SAVING_ERROR) ||
				      (state == GEDIT_TAB_STATE_EXTERNALLY_MODIFIED_NOTIFICATION) ||
				      (state == GEDIT_TAB_STATE_SHOWING_PRINT_PREVIEW)) &&
				     !(lockdown & GEDIT_LOCKDOWN_SAVE_TO_DISK));

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "revert");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
				     (state_normal ||
				      (state == GEDIT_TAB_STATE_EXTERNALLY_MODIFIED_NOTIFICATION)) &&
				     !gedit_document_is_untitled (doc));

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "print");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
				     (state_normal ||
				     (state == GEDIT_TAB_STATE_SHOWING_PRINT_PREVIEW)) &&
				     !(lockdown & GEDIT_LOCKDOWN_PRINTING));

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "close");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
				     (state != GEDIT_TAB_STATE_CLOSING) &&
				     (state != GEDIT_TAB_STATE_SAVING) &&
				     (state != GEDIT_TAB_STATE_SHOWING_PRINT_PREVIEW) &&
				     (state != GEDIT_TAB_STATE_PRINTING) &&
				     (state != GEDIT_TAB_STATE_PRINT_PREVIEWING) &&
				     (state != GEDIT_TAB_STATE_SAVING_ERROR));

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "undo");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
				     state_normal &&
				     gtk_source_buffer_can_undo (GTK_SOURCE_BUFFER (doc)));

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "redo");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
				     state_normal &&
				     gtk_source_buffer_can_redo (GTK_SOURCE_BUFFER (doc)));

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "cut");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
				     state_normal &&
				     editable &&
				     gtk_text_buffer_get_has_selection (GTK_TEXT_BUFFER (doc)));

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "copy");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
				     (state_normal ||
				      state == GEDIT_TAB_STATE_EXTERNALLY_MODIFIED_NOTIFICATION) &&
				     gtk_text_buffer_get_has_selection (GTK_TEXT_BUFFER (doc)));

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "paste");
	if (state_normal && editable)
	{
		set_paste_sensitivity_according_to_clipboard (window,
							      clipboard);
	}
	else
	{
		g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);
	}

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "delete");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
				     state_normal &&
				     editable &&
				     gtk_text_buffer_get_has_selection (GTK_TEXT_BUFFER (doc)));

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "find");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
				     (state_normal ||
				      state == GEDIT_TAB_STATE_EXTERNALLY_MODIFIED_NOTIFICATION));

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "replace");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
				     state_normal &&
				     editable);

	b = !_gedit_document_get_empty_search (doc);
	action = g_action_map_lookup_action (G_ACTION_MAP (window), "find_next");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
				     (state_normal ||
				      state == GEDIT_TAB_STATE_EXTERNALLY_MODIFIED_NOTIFICATION) && b);

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "find_prev");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
				     (state_normal ||
				      state == GEDIT_TAB_STATE_EXTERNALLY_MODIFIED_NOTIFICATION) && b);

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "clear_highlight");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
				     (state_normal ||
				      state == GEDIT_TAB_STATE_EXTERNALLY_MODIFIED_NOTIFICATION) && b);

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "goto_line");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
				     (state_normal ||
				      state == GEDIT_TAB_STATE_EXTERNALLY_MODIFIED_NOTIFICATION));

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "highlight_mode");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
				     (state != GEDIT_TAB_STATE_CLOSING) &&
				     enable_syntax_highlighting);

	update_next_prev_doc_sensitivity (window, tab);

	peas_extension_set_foreach (window->priv->extensions,
	                            (PeasExtensionSetForeachFunc) extension_update_state,
	                            window);
}

void
_gedit_recent_add (GeditWindow *window,
		   GFile       *location,
		   const gchar *mime)
{
	GtkRecentManager *recent_manager;
	GtkRecentData *recent_data;
	gchar *uri;

	static gchar *groups[2] = {
		"gedit",
		NULL
	};

	recent_manager =  gtk_recent_manager_get_default ();

	recent_data = g_slice_new (GtkRecentData);

	recent_data->display_name = NULL;
	recent_data->description = NULL;
	recent_data->mime_type = (gchar *) mime;
	recent_data->app_name = (gchar *) g_get_application_name ();
	recent_data->app_exec = g_strjoin (" ", g_get_prgname (), "%u", NULL);
	recent_data->groups = groups;
	recent_data->is_private = FALSE;

	uri = g_file_get_uri (location);
	gtk_recent_manager_add_full (recent_manager,
				     uri,
				     recent_data);

	g_free (uri);
	g_free (recent_data->app_exec);

	g_slice_free (GtkRecentData, recent_data);
}

void
_gedit_recent_remove (GeditWindow *window,
		      GFile       *location)
{
	GtkRecentManager *recent_manager;
	gchar *uri;

	recent_manager =  gtk_recent_manager_get_default ();

	uri = g_file_get_uri (location);
	gtk_recent_manager_remove_item (recent_manager, uri, NULL);
	g_free (uri);
}

static void
open_recent_file (GFile       *location,
		  GeditWindow *window)
{
	GSList *locations = NULL;
	GSList *loaded = NULL;

	locations = g_slist_prepend (locations, (gpointer) location);
	loaded = gedit_commands_load_locations (window, locations, NULL, 0, 0);

	if (!loaded || loaded->next) /* if it doesn't contain just 1 element */
	{
		_gedit_recent_remove (window, location);
	}

	g_slist_free (locations);
	g_slist_free (loaded);
}

static void
recent_chooser_item_activated (GtkRecentChooser *chooser,
			       GeditWindow      *window)
{
	GFile *location;
	gchar *uri;

	/* TODO: get_current_file when exists */
	uri = gtk_recent_chooser_get_current_uri (chooser);
	location = g_file_new_for_uri (uri);

	if (location)
	{
		open_recent_file (location, window);
		g_object_unref (location);
	}

	g_free (uri);
}

static void
setup_headerbar_open_button (GeditWindow *window)
{
	GtkMenu *recent_menu;

	g_settings_bind (window->priv->ui_settings,
	                 GEDIT_SETTINGS_MAX_RECENTS,
	                 window->priv->open_menu,
	                 "limit",
	                 G_SETTINGS_BIND_GET);

	recent_menu = gtk_menu_button_get_popup (GTK_MENU_BUTTON (window->priv->open_menu));

	g_signal_connect (recent_menu,
			  "item-activated",
			  G_CALLBACK (recent_chooser_item_activated),
			  window);
}

static void
create_menu_bar_and_toolbar (GeditWindow *window)
{
	GtkActionGroup *action_group;
	GtkUIManager *manager;

	manager = gtk_ui_manager_new ();
	window->priv->manager = manager;

	gedit_debug (DEBUG_WINDOW);

	/* list of open documents menu */
	action_group = gtk_action_group_new ("DocumentsListActions");
	gtk_action_group_set_translation_domain (action_group, NULL);
	window->priv->documents_list_action_group = action_group;
	gtk_ui_manager_insert_action_group (manager, action_group, 0);
	g_object_unref (action_group);
}

static void
documents_list_menu_activate (GtkToggleAction *action,
			      GeditWindow     *window)
{
	gint n;

	if (gtk_toggle_action_get_active (action) == FALSE)
		return;

	n = gtk_radio_action_get_current_value (GTK_RADIO_ACTION (action));
	gedit_multi_notebook_set_current_page (window->priv->multi_notebook, n);
}

static gchar *
get_menu_tip_for_tab (GeditTab *tab)
{
	GeditDocument *doc;
	gchar *uri;
	gchar *ruri;
	gchar *tip;

	doc = gedit_tab_get_document (tab);

	uri = gedit_document_get_uri_for_display (doc);
	ruri = gedit_utils_replace_home_dir_with_tilde (uri);
	g_free (uri);

	/* Translators: %s is a URI */
	tip =  g_strdup_printf (_("Activate '%s'"), ruri);
	g_free (ruri);

	return tip;
}

static gboolean
update_documents_list_menu_idle (GeditWindow *window)
{
	GeditWindowPrivate *p = window->priv;
	GList *actions, *l;
	gint n_notebooks, n_nb, n, i;
	guint id;
	GSList *group = NULL;

	gedit_debug (DEBUG_WINDOW);

	g_return_val_if_fail (p->documents_list_action_group != NULL, FALSE);

	if (p->documents_list_menu_ui_id != 0)
	{
		gtk_ui_manager_remove_ui (p->manager,
					  p->documents_list_menu_ui_id);
	}

	actions = gtk_action_group_list_actions (p->documents_list_action_group);
	for (l = actions; l != NULL; l = l->next)
	{
		g_signal_handlers_disconnect_by_func (GTK_ACTION (l->data),
						      G_CALLBACK (documents_list_menu_activate),
						      window);
		gtk_action_group_remove_action (p->documents_list_action_group,
						GTK_ACTION (l->data));
	}
	g_list_free (actions);

	n = gedit_multi_notebook_get_n_tabs (p->multi_notebook);

	id = (n > 0) ? gtk_ui_manager_new_merge_id (p->manager) : 0;

	n_notebooks = gedit_multi_notebook_get_n_notebooks (p->multi_notebook);

	i = 0;
	n_nb = 0;
	while (n_nb < n_notebooks)
	{
		GeditNotebook *notebook = gedit_multi_notebook_get_nth_notebook (p->multi_notebook, n_nb);
		gint j;

		n = gtk_notebook_get_n_pages (GTK_NOTEBOOK (notebook));

		for (j = 0; j < n; j++, i++)
		{
			GtkWidget *tab;
			GtkRadioAction *action;
			gchar *action_name;
			gchar *tab_name;
			gchar *name;
			gchar *tip;
			gboolean active_notebook;

			tab = gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), j);

			active_notebook = notebook == gedit_multi_notebook_get_active_notebook (p->multi_notebook);

			/* NOTE: the action is associated to the position of the tab in
			 * the notebook not to the tab itself! This is needed to work
			 * around the gtk+ bug #170727: gtk leaves around the accels
			 * of the action. Since the accel depends on the tab position
			 * the problem is worked around, action with the same name always
			 * get the same accel.
			 */
			if (active_notebook)
			{
				action_name = g_strdup_printf ("Active_Tab_%d", i);
			}
			else
			{
				action_name = g_strdup_printf ("Inactive_Tab_%d", i);
			}
			tab_name = _gedit_tab_get_name (GEDIT_TAB (tab));
			name = gedit_utils_escape_underscores (tab_name, -1);
			tip =  get_menu_tip_for_tab (GEDIT_TAB (tab));

			action = gtk_radio_action_new (action_name,
						       name,
						       tip,
						       NULL,
						       i);

			if (group != NULL)
				gtk_radio_action_set_group (action, group);

			/* note that group changes each time we add an action, so it must be updated */
			group = gtk_radio_action_get_group (action);

			/* alt + 1, 2, 3... 0 to switch to the first ten tabs */
			if (active_notebook)
			{
				gchar *accel;
				gchar const *mod;

#ifndef OS_OSX
				mod = "alt";
#else
				mod = "meta";
#endif

				accel = (j < 10) ? g_strdup_printf ("<%s>%d", mod, (j + 1) % 10) : NULL;

				gtk_action_group_add_action_with_accel (p->documents_list_action_group,
									GTK_ACTION (action),
									accel);
				g_free (accel);
			}
			else
			{
				gtk_action_group_add_action (p->documents_list_action_group,
							     GTK_ACTION (action));
			}

			g_signal_connect (action,
					  "activate",
					  G_CALLBACK (documents_list_menu_activate),
					  window);

			gtk_ui_manager_add_ui (p->manager,
					       id,
					       "/MenuBar/DocumentsMenu/DocumentsListPlaceholder",
					       action_name, action_name,
					       GTK_UI_MANAGER_MENUITEM,
					       FALSE);

			if (GEDIT_TAB (tab) == gedit_window_get_active_tab (window))
				gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);

			g_object_unref (action);

			g_free (action_name);
			g_free (tab_name);
			g_free (name);
			g_free (tip);
		}

		n_nb++;
	}

	p->documents_list_menu_ui_id = id;

	window->priv->update_documents_list_menu_id = 0;

	return FALSE;
}

static void
update_documents_list_menu (GeditWindow *window)
{
	/* Do the real update in an idle so that we consolidate
	 * multiple updates when loading or closing many docs */
	if (window->priv->update_documents_list_menu_id == 0)
	{
		window->priv->update_documents_list_menu_id =
			gdk_threads_add_idle ((GSourceFunc) update_documents_list_menu_idle,
			                      window);
	}
}

static void
activate_documents_list_item (GeditWindow *window,
			      GeditTab    *tab)
{
	GtkAction *action;
	gchar *action_name;
	gint page_num;
	GeditNotebook *active_notebook;
	gboolean is_active;

	active_notebook = gedit_multi_notebook_get_active_notebook (window->priv->multi_notebook);
	page_num = gtk_notebook_page_num (GTK_NOTEBOOK (active_notebook),
					  GTK_WIDGET (tab));

	is_active = (page_num != -1);

	page_num = gedit_multi_notebook_get_page_num (window->priv->multi_notebook,
						      tab);

	/* get the action name related with the page number */
	if (is_active)
	{
		action_name = g_strdup_printf ("Active_Tab_%d", page_num);
	}
	else
	{
		action_name = g_strdup_printf ("Inactive_Tab_%d", page_num);
	}
	action = gtk_action_group_get_action (window->priv->documents_list_action_group,
					      action_name);

	/* sometimes the action doesn't exist yet, and the proper action
	 * is set active during the documents list menu creation
	 * CHECK: would it be nicer if active_tab was a property and we monitored the notify signal?
	 */
	if (action != NULL)
	{
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);
	}

	g_free (action_name);
}

static void
tab_width_combo_item_activated (GtkMenuItem *item,
				GeditWindow *window)
{
	GeditView *view;
	guint width_data = 0;

	view = gedit_window_get_active_view (window);
	if (!view)
		return;

	width_data = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (item), TAB_WIDTH_DATA));
	if (width_data == 0)
		return;

	gtk_source_view_set_tab_width (GTK_SOURCE_VIEW (view), width_data);
}

static void
use_spaces_toggled (GtkCheckMenuItem *item,
		    GeditWindow      *window)
{
	GeditView *view;
	gboolean active;

	view = gedit_window_get_active_view (window);
	if (!view)
		return;

	active = gtk_check_menu_item_get_active (item);

	gtk_source_view_set_insert_spaces_instead_of_tabs (GTK_SOURCE_VIEW (view),
	                                                   active);
}

static void
create_tab_width_combo (GeditWindow *window)
{
	static guint tab_widths[] = { 2, 4, 8 };

	guint i = 0;
	GtkWidget *item;

	window->priv->tab_width_combo = gedit_status_menu_button_new ();
	window->priv->tab_width_combo_menu = gtk_menu_new ();
	gtk_menu_button_set_popup (GTK_MENU_BUTTON (window->priv->tab_width_combo),
	                           window->priv->tab_width_combo_menu);
	gtk_widget_show (window->priv->tab_width_combo);
	gtk_box_pack_end (GTK_BOX (window->priv->statusbar),
			  window->priv->tab_width_combo,
			  FALSE,
			  TRUE,
			  0);

	for (i = 0; i < G_N_ELEMENTS (tab_widths); i++)
	{
		gchar *label = g_strdup_printf ("%u", tab_widths[i]);
		item = gtk_menu_item_new_with_label (label);
		g_free (label);

		g_object_set_data (G_OBJECT (item), TAB_WIDTH_DATA, GINT_TO_POINTER (tab_widths[i]));

		g_signal_connect (item,
				  "activate",
				  G_CALLBACK (tab_width_combo_item_activated),
				  window);

		gtk_menu_shell_append (GTK_MENU_SHELL (window->priv->tab_width_combo_menu),
		                       GTK_WIDGET (item));
		gtk_widget_show (item);
	}

	/* Add a hidden item for custom values */
	item = gtk_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (window->priv->tab_width_combo_menu),
	                       GTK_WIDGET (item));

	g_signal_connect (item,
			  "activate",
			  G_CALLBACK (tab_width_combo_item_activated),
			  window);

	item = gtk_separator_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (window->priv->tab_width_combo_menu),
	                       GTK_WIDGET (item));
	gtk_widget_show (item);

	item = gtk_check_menu_item_new_with_label (_("Use Spaces"));
	gtk_menu_shell_append (GTK_MENU_SHELL (window->priv->tab_width_combo_menu),
	                       GTK_WIDGET (item));
	gtk_widget_show (item);

	g_signal_connect (item,
			  "toggled",
			  G_CALLBACK (use_spaces_toggled),
			  window);
}

static void
on_language_button_clicked (GtkButton   *button,
                            GeditWindow *window)
{
	_gedit_cmd_view_highlight_mode (NULL, NULL, window);
}

static void
create_language_button (GeditWindow *window)
{
	GtkWidget *box;
	GtkWidget *arrow;

	window->priv->language_button = gedit_small_button_new ();
	gtk_widget_show (window->priv->language_button);
	gtk_box_pack_end (GTK_BOX (window->priv->statusbar),
			  window->priv->language_button,
			  FALSE,
			  TRUE,
			  0);

	box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 3);
	gtk_widget_show (box);
	gtk_container_add (GTK_CONTAINER (window->priv->language_button), box);

	window->priv->language_button_label = gtk_label_new (NULL);
	gtk_widget_show (window->priv->language_button_label);
	gtk_box_pack_start (GTK_BOX (box), window->priv->language_button_label,
	                    FALSE, TRUE, 0);

	arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_NONE);
	gtk_widget_show (arrow);
	gtk_box_pack_start (GTK_BOX (box), arrow, FALSE, FALSE, 0);

	g_signal_connect (window->priv->language_button, "clicked",
	                  G_CALLBACK (on_language_button_clicked), window);
}

static void
setup_statusbar (GeditWindow *window)
{
	gedit_debug (DEBUG_WINDOW);

	window->priv->generic_message_cid = gtk_statusbar_get_context_id
		(GTK_STATUSBAR (window->priv->statusbar), "generic_message");
	window->priv->tip_message_cid = gtk_statusbar_get_context_id
		(GTK_STATUSBAR (window->priv->statusbar), "tip_message");
	window->priv->bracket_match_message_cid = gtk_statusbar_get_context_id
		(GTK_STATUSBAR (window->priv->statusbar), "bracket_match_message");

	create_tab_width_combo (window);
	create_language_button (window);

	g_settings_bind (window->priv->ui_settings,
	                 "statusbar-visible",
	                 window->priv->statusbar,
	                 "visible",
	                 G_SETTINGS_BIND_GET);
}

static GeditWindow *
clone_window (GeditWindow *origin)
{
	GeditWindow *window;
	GdkScreen *screen;
	GeditApp  *app;
	gint panel_page;

	gedit_debug (DEBUG_WINDOW);

	app = GEDIT_APP (g_application_get_default ());

	screen = gtk_window_get_screen (GTK_WINDOW (origin));
	window = gedit_app_create_window (app, screen);

	gtk_window_set_default_size (GTK_WINDOW (window),
				     origin->priv->width,
				     origin->priv->height);

	if ((origin->priv->window_state & GDK_WINDOW_STATE_MAXIMIZED) != 0)
		gtk_window_maximize (GTK_WINDOW (window));
	else
		gtk_window_unmaximize (GTK_WINDOW (window));

	if ((origin->priv->window_state & GDK_WINDOW_STATE_STICKY) != 0)
		gtk_window_stick (GTK_WINDOW (window));
	else
		gtk_window_unstick (GTK_WINDOW (window));

	/* set the panels size, the paned position will be set when
	 * they are mapped */
	window->priv->side_panel_size = origin->priv->side_panel_size;
	window->priv->bottom_panel_size = origin->priv->bottom_panel_size;

	panel_page = _gedit_panel_get_active_item_id (GEDIT_PANEL (origin->priv->side_panel));
	_gedit_panel_set_active_item_by_id (GEDIT_PANEL (window->priv->side_panel),
					    panel_page);

	panel_page = _gedit_panel_get_active_item_id (GEDIT_PANEL (origin->priv->bottom_panel));
	_gedit_panel_set_active_item_by_id (GEDIT_PANEL (window->priv->bottom_panel),
					    panel_page);

	return window;
}

static void
bracket_matched_cb (GtkSourceBuffer           *buffer,
		    GtkTextIter               *iter,
		    GtkSourceBracketMatchType  result,
		    GeditWindow               *window)
{
	if (buffer != GTK_SOURCE_BUFFER (gedit_window_get_active_document (window)))
		return;

	switch (result)
	{
		case GTK_SOURCE_BRACKET_MATCH_NONE:
			gtk_statusbar_pop (GTK_STATUSBAR (window->priv->statusbar),
					   window->priv->bracket_match_message_cid);
			break;
		case GTK_SOURCE_BRACKET_MATCH_OUT_OF_RANGE:
			gedit_statusbar_flash_message (GEDIT_STATUSBAR (window->priv->statusbar),
						       window->priv->bracket_match_message_cid,
						       _("Bracket match is out of range"));
			break;
		case GTK_SOURCE_BRACKET_MATCH_NOT_FOUND:
			gedit_statusbar_flash_message (GEDIT_STATUSBAR (window->priv->statusbar),
						       window->priv->bracket_match_message_cid,
						       _("Bracket match not found"));
			break;
		case GTK_SOURCE_BRACKET_MATCH_FOUND:
			gedit_statusbar_flash_message (GEDIT_STATUSBAR (window->priv->statusbar),
						       window->priv->bracket_match_message_cid,
						       _("Bracket match found on line: %d"),
						       gtk_text_iter_get_line (iter) + 1);
			break;
		default:
			g_assert_not_reached ();
	}
}

static void
update_cursor_position_statusbar (GtkTextBuffer *buffer,
				  GeditWindow   *window)
{
	gint row, col;
	GtkTextIter iter;
	GeditView *view;

	gedit_debug (DEBUG_WINDOW);

 	if (buffer != GTK_TEXT_BUFFER (gedit_window_get_active_document (window)))
 		return;

 	view = gedit_window_get_active_view (window);

	gtk_text_buffer_get_iter_at_mark (buffer,
					  &iter,
					  gtk_text_buffer_get_insert (buffer));

	row = gtk_text_iter_get_line (&iter);
	col = gtk_source_view_get_visual_column (GTK_SOURCE_VIEW (view), &iter);

	gedit_statusbar_set_cursor_position (
				GEDIT_STATUSBAR (window->priv->statusbar),
				row + 1,
				col + 1);
}

static void
update_overwrite_mode_statusbar (GtkTextView *view,
				 GeditWindow *window)
{
	if (view != GTK_TEXT_VIEW (gedit_window_get_active_view (window)))
		return;

	/* Note that we have to use !gtk_text_view_get_overwrite since we
	   are in the in the signal handler of "toggle overwrite" that is
	   G_SIGNAL_RUN_LAST
	*/
	gedit_statusbar_set_overwrite (
			GEDIT_STATUSBAR (window->priv->statusbar),
			!gtk_text_view_get_overwrite (view));
}

#define MAX_TITLE_LENGTH 100

static void
set_title (GeditWindow *window)
{
	GeditTab *tab;
	GeditDocument *doc = NULL;
	gchar *name;
	gchar *dirname = NULL;
	gchar *main_title = NULL;
	gchar *title = NULL;
	gchar *subtitle = NULL;
	gint len;

	tab = gedit_window_get_active_tab (window);

	if (tab == NULL)
	{
		gedit_app_set_window_title (GEDIT_APP (g_application_get_default ()),
		                            window,
		                            "gedit");
		gtk_header_bar_set_title (GTK_HEADER_BAR (window->priv->headerbar),
		                          "gedit");
		gtk_header_bar_set_subtitle (GTK_HEADER_BAR (window->priv->headerbar),
		                             NULL);
		return;
	}

	doc = gedit_tab_get_document (tab);
	g_return_if_fail (doc != NULL);

	name = gedit_document_get_short_name_for_display (doc);

	len = g_utf8_strlen (name, -1);

	/* if the name is awfully long, truncate it and be done with it,
	 * otherwise also show the directory (ellipsized if needed)
	 */
	if (len > MAX_TITLE_LENGTH)
	{
		gchar *tmp;

		tmp = gedit_utils_str_middle_truncate (name,
						       MAX_TITLE_LENGTH);
		g_free (name);
		name = tmp;
	}
	else
	{
		GFile *file;

		file = gedit_document_get_location (doc);
		if (file != NULL)
		{
			gchar *str;

			str = gedit_utils_location_get_dirname_for_display (file);
			g_object_unref (file);

			/* use the remaining space for the dir, but use a min of 20 chars
			 * so that we do not end up with a dirname like "(a...b)".
			 * This means that in the worst case when the filename is long 99
			 * we have a title long 99 + 20, but I think it's a rare enough
			 * case to be acceptable. It's justa darn title afterall :)
			 */
			dirname = gedit_utils_str_middle_truncate (str,
								   MAX (20, MAX_TITLE_LENGTH - len));
			g_free (str);
		}
	}

	if (gtk_text_buffer_get_modified (GTK_TEXT_BUFFER (doc)))
	{
		gchar *tmp_name;

		tmp_name = g_strdup_printf ("*%s", name);
		g_free (name);

		name = tmp_name;
	}

	if (gedit_document_get_readonly (doc))
	{
		title = g_strdup_printf ("%s [%s]",
		                         name, _("Read-Only"));

		if (dirname != NULL)
		{
			main_title = g_strdup_printf ("%s [%s] (%s) - gedit",
			                              name,
			                              _("Read-Only"),
			                              dirname);
			subtitle = dirname;
		}
		else
		{
			main_title = g_strdup_printf ("%s [%s] - gedit",
			                              name,
			                              _("Read-Only"));
		}
	}
	else
	{
		title = g_strdup (name);

		if (dirname != NULL)
		{
			main_title = g_strdup_printf ("%s (%s) - gedit",
			                              name,
			                              dirname);
			subtitle = dirname;
		}
		else
		{
			main_title = g_strdup_printf ("%s - gedit",
			                              name);
		}
	}

	gedit_app_set_window_title (GEDIT_APP (g_application_get_default ()),
				    window,
				    main_title);

	gtk_header_bar_set_title (GTK_HEADER_BAR (window->priv->headerbar),
	                          title);
	gtk_header_bar_set_subtitle (GTK_HEADER_BAR (window->priv->headerbar),
	                             subtitle);

	g_free (dirname);
	g_free (name);
	g_free (title);
	g_free (main_title);
}

#undef MAX_TITLE_LENGTH

static void
tab_width_changed (GObject     *object,
		   GParamSpec  *pspec,
		   GeditWindow *window)
{
	GList *items;
	GList *item;
	guint new_tab_width;
	gchar *label;
	gboolean found = FALSE;

	items = gtk_container_get_children (GTK_CONTAINER (window->priv->tab_width_combo_menu));

	new_tab_width = gtk_source_view_get_tab_width (GTK_SOURCE_VIEW (object));

	label = g_strdup_printf (_("Tab Width: %u"), new_tab_width);
	gedit_status_menu_button_set_label (GEDIT_STATUS_MENU_BUTTON (window->priv->tab_width_combo), label);
	g_free (label);

	for (item = items; item; item = item->next)
	{
		guint tab_width = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (item->data), TAB_WIDTH_DATA));

		if (tab_width == new_tab_width)
		{
			found = TRUE;
		}

		if (GTK_IS_SEPARATOR_MENU_ITEM (item->next->data))
		{
			if (!found)
			{
				/* Show the last menu item with a custom value */
				label = g_strdup_printf ("%u", new_tab_width);
				gtk_menu_item_set_label (GTK_MENU_ITEM (item->data), label);
				g_free (label);
				gtk_widget_show (GTK_WIDGET (item->data));
			}
			else
			{
				gtk_widget_hide (GTK_WIDGET (item->data));
			}

			break;
		}
	}

	g_list_free (items);
}

static void
language_changed (GObject     *object,
		  GParamSpec  *pspec,
		  GeditWindow *window)
{
	GtkSourceLanguage *new_language;
	const gchar *label;

	new_language = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (object));

	if (new_language)
		label = gtk_source_language_get_name (new_language);
	else
		label = _("Plain Text");

	gtk_label_set_text (GTK_LABEL (window->priv->language_button_label), label);

	peas_extension_set_foreach (window->priv->extensions,
	                            (PeasExtensionSetForeachFunc) extension_update_state,
	                            window);
}

static void
update_statusbar (GeditWindow *window,
		  GeditView   *old_view,
		  GeditView   *new_view)
{
	GeditDocument *doc;
	GList *items;
	GtkCheckMenuItem *item;

	if (old_view)
	{
		g_clear_object (&window->priv->spaces_instead_of_tabs_binding);

		if (window->priv->tab_width_id)
		{
			g_signal_handler_disconnect (old_view,
						     window->priv->tab_width_id);

			window->priv->tab_width_id = 0;
		}

		if (window->priv->language_changed_id)
		{
			g_signal_handler_disconnect (gtk_text_view_get_buffer (GTK_TEXT_VIEW (old_view)),
						     window->priv->language_changed_id);

			window->priv->language_changed_id = 0;
		}
	}

	if (new_view == NULL)
		return;

	doc = GEDIT_DOCUMENT (gtk_text_view_get_buffer (GTK_TEXT_VIEW (new_view)));

	/* sync the statusbar */
	update_cursor_position_statusbar (GTK_TEXT_BUFFER (doc),
					  window);

	gedit_statusbar_set_overwrite (GEDIT_STATUSBAR (window->priv->statusbar),
				       gtk_text_view_get_overwrite (GTK_TEXT_VIEW (new_view)));

	gtk_widget_show (window->priv->tab_width_combo);
	gtk_widget_show (window->priv->language_button);

	/* find the use spaces item */
	items = gtk_container_get_children (GTK_CONTAINER (window->priv->tab_width_combo_menu));
	item = GTK_CHECK_MENU_ITEM (g_list_last (items)->data);
	g_list_free (items);

	window->priv->spaces_instead_of_tabs_binding =
		g_object_bind_property (new_view,
		                        "insert-spaces-instead-of-tabs",
		                        item,
		                        "active",
		                         G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

	window->priv->tab_width_id = g_signal_connect (new_view,
						       "notify::tab-width",
						       G_CALLBACK (tab_width_changed),
						       window);

	window->priv->language_changed_id = g_signal_connect (doc,
							      "notify::language",
							      G_CALLBACK (language_changed),
							      window);

	/* call it for the first time */
	tab_width_changed (G_OBJECT (new_view), NULL, window);
	language_changed (G_OBJECT (doc), NULL, window);
}

static void
tab_switched (GeditMultiNotebook *mnb,
	      GeditNotebook      *old_notebook,
	      GeditTab           *old_tab,
	      GeditNotebook      *new_notebook,
	      GeditTab           *new_tab,
	      GeditWindow        *window)
{
	GeditView *old_view, *new_view;

	old_view = old_tab ? gedit_tab_get_view (old_tab) : NULL;
	new_view = new_tab ? gedit_tab_get_view (new_tab) : NULL;

	update_statusbar (window, old_view, new_view);

	if (new_tab == NULL || window->priv->dispose_has_run)
		return;

	set_title (window);
	set_sensitivity_according_to_tab (window, new_tab);

	/* activate the right item in the documents menu */
	activate_documents_list_item (window, new_tab);

	g_signal_emit (G_OBJECT (window),
		       signals[ACTIVE_TAB_CHANGED],
		       0,
		       new_tab);
}

static void
set_sensitivity_according_to_window_state (GeditWindow *window)
{
	GAction *action;
	GeditLockdownMask lockdown;
	gint num_tabs;

	lockdown = gedit_app_get_lockdown (GEDIT_APP (g_application_get_default ()));
	num_tabs = gedit_multi_notebook_get_n_tabs (window->priv->multi_notebook);

	/* We disable File->Quit/SaveAll/CloseAll while printing to avoid to have two
	   operations (save and print/print preview) that uses the message area at
	   the same time (may be we can remove this limitation in the future) */
	/* We disable File->Quit/CloseAll if state is saving since saving cannot be
	   cancelled (may be we can remove this limitation in the future) */
	action = g_action_map_lookup_action (G_ACTION_MAP (g_application_get_default ()),
	                                     "quit");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
	                             !(window->priv->state & GEDIT_WINDOW_STATE_SAVING) &&
	                             !(window->priv->state & GEDIT_WINDOW_STATE_PRINTING) &&
	                             num_tabs > 0);

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "close_all");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
				     !(window->priv->state & GEDIT_WINDOW_STATE_SAVING) &&
				     !(window->priv->state & GEDIT_WINDOW_STATE_PRINTING));

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "save_all");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
				     !(window->priv->state & GEDIT_WINDOW_STATE_PRINTING) &&
				     !(lockdown & GEDIT_LOCKDOWN_SAVE_TO_DISK));

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "save");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), num_tabs > 0);

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "save_as");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), num_tabs > 0);

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "revert");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), num_tabs > 0);

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "print");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), num_tabs > 0);

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "find");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), num_tabs > 0);

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "replace");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), num_tabs > 0);

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "find_next");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), num_tabs > 0);

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "find_prev");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), num_tabs > 0);

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "clear_highlight");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), num_tabs > 0);

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "goto_line");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), num_tabs > 0);

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "new_tab_group");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), num_tabs > 0);

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "previous_document");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), num_tabs > 0);

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "next_document");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), num_tabs > 0);

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "move_to_new_window");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), num_tabs > 1);

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "highlight_mode");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), num_tabs > 0);

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "close");
	if (!g_action_get_enabled (action))
	{
#ifdef OS_OSX
		/* On OS X, File Close is always sensitive */
		g_simple_action_set_enabled (G_SIMPLE_ACTION (action), TRUE);
#else
		g_simple_action_set_enabled (G_SIMPLE_ACTION (action), num_tabs > 0);
#endif
	}
}

static void
set_auto_save_enabled (GeditTab *tab,
		       gpointer  autosave)
{
	gedit_tab_set_auto_save_enabled (tab, GPOINTER_TO_BOOLEAN (autosave));
}

void
_gedit_window_set_lockdown (GeditWindow       *window,
			    GeditLockdownMask  lockdown)
{
	GeditTab *tab;
	GAction *action;
	gboolean autosave;

	/* start/stop autosave in each existing tab */
	autosave = g_settings_get_boolean (window->priv->editor_settings,
					   GEDIT_SETTINGS_AUTO_SAVE);

	gedit_multi_notebook_foreach_tab (window->priv->multi_notebook,
					  (GtkCallback)set_auto_save_enabled,
					  &autosave);

	/* update menues wrt the current active tab */
	tab = gedit_window_get_active_tab (window);

	set_sensitivity_according_to_tab (window, tab);

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "save_all");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
				     !(window->priv->state & GEDIT_WINDOW_STATE_PRINTING) &&
				     !(lockdown & GEDIT_LOCKDOWN_SAVE_TO_DISK));
}

static void
analyze_tab_state (GeditTab    *tab,
		   GeditWindow *window)
{
	GeditTabState ts;

	ts = gedit_tab_get_state (tab);

	switch (ts)
	{
		case GEDIT_TAB_STATE_LOADING:
		case GEDIT_TAB_STATE_REVERTING:
			window->priv->state |= GEDIT_WINDOW_STATE_LOADING;
			break;

		case GEDIT_TAB_STATE_SAVING:
			window->priv->state |= GEDIT_WINDOW_STATE_SAVING;
			break;

		case GEDIT_TAB_STATE_PRINTING:
		case GEDIT_TAB_STATE_PRINT_PREVIEWING:
			window->priv->state |= GEDIT_WINDOW_STATE_PRINTING;
			break;

		case GEDIT_TAB_STATE_LOADING_ERROR:
		case GEDIT_TAB_STATE_REVERTING_ERROR:
		case GEDIT_TAB_STATE_SAVING_ERROR:
		case GEDIT_TAB_STATE_GENERIC_ERROR:
			window->priv->state |= GEDIT_WINDOW_STATE_ERROR;
			++window->priv->num_tabs_with_error;
		default:
			/* NOP */
			break;
	}
}

static void
update_window_state (GeditWindow *window)
{
	GeditWindowState old_ws;
	gint old_num_of_errors;

	gedit_debug_message (DEBUG_WINDOW, "Old state: %x", window->priv->state);

	old_ws = window->priv->state;
	old_num_of_errors = window->priv->num_tabs_with_error;

	window->priv->state = 0;
	window->priv->num_tabs_with_error = 0;

	gedit_multi_notebook_foreach_tab (window->priv->multi_notebook,
					  (GtkCallback)analyze_tab_state,
					  window);

	gedit_debug_message (DEBUG_WINDOW, "New state: %x", window->priv->state);

	if (old_ws != window->priv->state)
	{
		set_sensitivity_according_to_window_state (window);

		gedit_statusbar_set_window_state (GEDIT_STATUSBAR (window->priv->statusbar),
						  window->priv->state,
						  window->priv->num_tabs_with_error);

		g_object_notify (G_OBJECT (window), "state");
	}
	else if (old_num_of_errors != window->priv->num_tabs_with_error)
	{
		gedit_statusbar_set_window_state (GEDIT_STATUSBAR (window->priv->statusbar),
						  window->priv->state,
						  window->priv->num_tabs_with_error);
	}
}

static void
update_can_close (GeditWindow *window)
{
	GeditWindowPrivate *priv = window->priv;
	GList *tabs;
	GList *l;
	gboolean can_close = TRUE;

	gedit_debug (DEBUG_WINDOW);

	tabs = gedit_multi_notebook_get_all_tabs (priv->multi_notebook);

	for (l = tabs; l != NULL; l = g_list_next (l))
	{
		GeditTab *tab = l->data;

		if (!_gedit_tab_get_can_close (tab))
		{
			can_close = FALSE;
			break;
		}
	}

	if (can_close && (priv->inhibition_cookie != 0))
	{
		gtk_application_uninhibit (GTK_APPLICATION (g_application_get_default ()),
					   priv->inhibition_cookie);
		priv->inhibition_cookie = 0;
	}
	else if (!can_close && (priv->inhibition_cookie == 0))
	{
		priv->inhibition_cookie = gtk_application_inhibit (GTK_APPLICATION (g_application_get_default ()),
		                                                   GTK_WINDOW (window),
		                                                   GTK_APPLICATION_INHIBIT_LOGOUT,
		                                                   _("There are unsaved documents"));
	}

	g_list_free (tabs);
}

static void
sync_state (GeditTab    *tab,
	    GParamSpec  *pspec,
	    GeditWindow *window)
{
	gedit_debug (DEBUG_WINDOW);

	update_window_state (window);

	if (tab != gedit_window_get_active_tab (window))
		return;

	set_sensitivity_according_to_tab (window, tab);

	g_signal_emit (G_OBJECT (window), signals[ACTIVE_TAB_STATE_CHANGED], 0);
}

static void
sync_name (GeditTab    *tab,
	   GParamSpec  *pspec,
	   GeditWindow *window)
{
	GtkAction *action;
	gchar *action_name;
	GeditDocument *doc;
	gint page_num;
	GeditNotebook *active_notebook;
	gboolean is_active;

	if (tab == gedit_window_get_active_tab (window))
	{
		GAction *gaction;

		set_title (window);

		doc = gedit_tab_get_document (tab);
		
		gaction = g_action_map_lookup_action (G_ACTION_MAP (window),
		                                      "revert");
		g_simple_action_set_enabled (G_SIMPLE_ACTION (gaction),
		                             !gedit_document_is_untitled (doc));
	}

	/* sync the item in the documents list menu */

	active_notebook = gedit_multi_notebook_get_active_notebook (window->priv->multi_notebook);
	page_num = gtk_notebook_page_num (GTK_NOTEBOOK (active_notebook),
					  GTK_WIDGET (tab));

	is_active = (page_num != -1);

	page_num = gedit_multi_notebook_get_page_num (window->priv->multi_notebook,
						      tab);

	/* get the action name related with the page number */
	if (is_active)
	{
		action_name = g_strdup_printf ("Active_Tab_%d", page_num);
	}
	else
	{
		action_name = g_strdup_printf ("Inactive_Tab_%d", page_num);
	}
	action = gtk_action_group_get_action (window->priv->documents_list_action_group,
					      action_name);
	g_free (action_name);

	/* action may be NULL if the idle has not populated the menu yet */
	if (action != NULL)
	{
		gchar *tab_name;
		gchar *escaped_name;
		gchar *tip;

		tab_name = _gedit_tab_get_name (tab);
		escaped_name = gedit_utils_escape_underscores (tab_name, -1);
		tip =  get_menu_tip_for_tab (tab);

		g_object_set (action, "label", escaped_name, NULL);
		g_object_set (action, "tooltip", tip, NULL);

		g_free (tab_name);
		g_free (escaped_name);
		g_free (tip);
	}

	peas_extension_set_foreach (window->priv->extensions,
	                            (PeasExtensionSetForeachFunc) extension_update_state,
	                            window);
}

static void
sync_can_close (GeditTab    *tab,
		GParamSpec  *pspec,
		GeditWindow *window)
{
	update_can_close (window);
}

static GeditWindow *
get_drop_window (GtkWidget *widget)
{
	GtkWidget *target_window;

	target_window = gtk_widget_get_toplevel (widget);
	g_return_val_if_fail (GEDIT_IS_WINDOW (target_window), NULL);

	return GEDIT_WINDOW (target_window);
}

static void
load_uris_from_drop (GeditWindow  *window,
		     gchar       **uri_list)
{
	GSList *locations = NULL;
	gint i;
	GSList *loaded;

	if (uri_list == NULL)
		return;

	for (i = 0; uri_list[i] != NULL; ++i)
	{
		locations = g_slist_prepend (locations, g_file_new_for_uri (uri_list[i]));
	}

	locations = g_slist_reverse (locations);
	loaded = gedit_commands_load_locations (window,
	                                        locations,
	                                        NULL,
	                                        0,
	                                        0);

	g_slist_free (loaded);
	g_slist_free_full (locations, g_object_unref);
}

/* Handle drops on the GeditWindow */
static void
drag_data_received_cb (GtkWidget        *widget,
		       GdkDragContext   *context,
		       gint              x,
		       gint              y,
		       GtkSelectionData *selection_data,
		       guint             info,
		       guint             timestamp,
		       gpointer          data)
{
	GeditWindow *window;
	gchar **uri_list;

	window = get_drop_window (widget);

	if (window == NULL)
		return;

	switch (info)
	{
		case TARGET_URI_LIST:
			uri_list = gedit_utils_drop_get_uris(selection_data);
			load_uris_from_drop (window, uri_list);
			g_strfreev (uri_list);

			gtk_drag_finish (context, TRUE, FALSE, timestamp);

			break;

		case TARGET_XDNDDIRECTSAVE:
			/* Indicate that we don't provide "F" fallback */
			if (gtk_selection_data_get_format (selection_data) == 8 &&
			    gtk_selection_data_get_length (selection_data) == 1 &&
			    gtk_selection_data_get_data (selection_data)[0] == 'F')
			{
				gdk_property_change (gdk_drag_context_get_source_window (context),
						     gdk_atom_intern ("XdndDirectSave0", FALSE),
						     gdk_atom_intern ("text/plain", FALSE), 8,
						     GDK_PROP_MODE_REPLACE, (const guchar *) "", 0);
			}
			else if (gtk_selection_data_get_format (selection_data) == 8 &&
				 gtk_selection_data_get_length (selection_data) == 1 &&
				 gtk_selection_data_get_data (selection_data)[0] == 'S' &&
				 window->priv->direct_save_uri != NULL)
			{
				gchar **uris;

				uris = g_new (gchar *, 2);
				uris[0] = window->priv->direct_save_uri;
				uris[1] = NULL;

				load_uris_from_drop (window, uris);
				g_free (uris);
			}

			g_free (window->priv->direct_save_uri);
			window->priv->direct_save_uri = NULL;

			gtk_drag_finish (context, TRUE, FALSE, timestamp);

			break;
	}
}

static void
drag_drop_cb (GtkWidget      *widget,
	      GdkDragContext *context,
	      gint            x,
	      gint            y,
	      guint           time,
	      gpointer        user_data)
{
	GeditWindow *window;
	GtkTargetList *target_list;
	GdkAtom target;

	window = get_drop_window (widget);

	target_list = gtk_drag_dest_get_target_list (widget);
	target = gtk_drag_dest_find_target (widget, context, target_list);

	if (target != GDK_NONE)
	{
		guint info;
		gboolean found;

		found = gtk_target_list_find (target_list, target, &info);
		g_assert (found);

		if (info == TARGET_XDNDDIRECTSAVE)
		{
			gchar *uri;
			uri = gedit_utils_set_direct_save_filename (context);

			if (uri != NULL)
			{
				g_free (window->priv->direct_save_uri);
				window->priv->direct_save_uri = uri;
			}
		}

		gtk_drag_get_data (GTK_WIDGET (widget), context,
				   target, time);
	}
}

/* Handle drops on the GeditView */
static void
drop_uris_cb (GtkWidget    *widget,
	      gchar       **uri_list,
	      GeditWindow  *window)
{
	load_uris_from_drop (window, uri_list);
}

static void
fullscreen_controls_show (GeditWindow *window)
{
	GdkScreen *screen;
	GdkRectangle fs_rect;
	gint w, h;

	screen = gtk_window_get_screen (GTK_WINDOW (window));
	gdk_screen_get_monitor_geometry (screen,
					 gdk_screen_get_monitor_at_window (screen,
									   gtk_widget_get_window (GTK_WIDGET (window))),
					 &fs_rect);

	gtk_widget_show_all (window->priv->fullscreen_controls);
	gtk_window_get_size (GTK_WINDOW (window->priv->fullscreen_controls), &w, &h);

	gtk_window_resize (GTK_WINDOW (window->priv->fullscreen_controls),
			   fs_rect.width, h);

	gtk_window_move (GTK_WINDOW (window->priv->fullscreen_controls),
			 fs_rect.x, fs_rect.y - h + 1);
}

static gboolean
run_fullscreen_animation (gpointer data)
{
	GeditWindow *window = GEDIT_WINDOW (data);
	GdkScreen *screen;
	GdkRectangle fs_rect;
	gint x, y;

	screen = gtk_window_get_screen (GTK_WINDOW (window));
	gdk_screen_get_monitor_geometry (screen,
					 gdk_screen_get_monitor_at_window (screen,
									   gtk_widget_get_window (GTK_WIDGET (window))),
					 &fs_rect);

	gtk_window_get_position (GTK_WINDOW (window->priv->fullscreen_controls),
				 &x, &y);

	if (window->priv->fullscreen_animation_enter)
	{
		if (y == fs_rect.y)
		{
			window->priv->fullscreen_animation_timeout_id = 0;
			return FALSE;
		}
		else
		{
			gtk_window_move (GTK_WINDOW (window->priv->fullscreen_controls),
					 x, y + 1);
			return TRUE;
		}
	}
	else
	{
		gint w, h;

		gtk_window_get_size (GTK_WINDOW (window->priv->fullscreen_controls),
				     &w, &h);

		if (y == fs_rect.y - h + 1)
		{
			window->priv->fullscreen_animation_timeout_id = 0;
			return FALSE;
		}
		else
		{
			gtk_window_move (GTK_WINDOW (window->priv->fullscreen_controls),
					 x, y - 1);
			return TRUE;
		}
	}
}

static void
show_hide_fullscreen_controls (GeditWindow *window,
			       gboolean     show,
			       gint         height)
{
	GtkSettings *settings;
	gboolean enable_animations;

	settings = gtk_widget_get_settings (GTK_WIDGET (window));
	g_object_get (G_OBJECT (settings),
		      "gtk-enable-animations",
		      &enable_animations,
		      NULL);

	if (enable_animations)
	{
		window->priv->fullscreen_animation_enter = show;

		if (window->priv->fullscreen_animation_timeout_id == 0)
		{
			window->priv->fullscreen_animation_timeout_id =
				g_timeout_add (FULLSCREEN_ANIMATION_SPEED,
					       (GSourceFunc) run_fullscreen_animation,
					       window);
		}
	}
	else
	{
		GdkRectangle fs_rect;
		GdkScreen *screen;

		screen = gtk_window_get_screen (GTK_WINDOW (window));
		gdk_screen_get_monitor_geometry (screen,
						 gdk_screen_get_monitor_at_window (screen,
										   gtk_widget_get_window (GTK_WIDGET (window))),
						 &fs_rect);

		if (show)
		{
			gtk_window_move (GTK_WINDOW (window->priv->fullscreen_controls),
				 fs_rect.x, fs_rect.y);
		}
		else
		{
			gtk_window_move (GTK_WINDOW (window->priv->fullscreen_controls),
					 fs_rect.x, fs_rect.y - height + 1);
		}
	}

}

static gboolean
on_fullscreen_controls_enter_notify_event (GtkWidget        *widget,
					   GdkEventCrossing *event,
					   GeditWindow      *window)
{
	show_hide_fullscreen_controls (window, TRUE, 0);

	return FALSE;
}

static gboolean
on_fullscreen_controls_leave_notify_event (GtkWidget        *widget,
					   GdkEventCrossing *event,
					   GeditWindow      *window)
{
	GdkDevice *device;
	gint w, h;
	gint x, y;

	device = gdk_event_get_device ((GdkEvent *)event);

	gtk_window_get_size (GTK_WINDOW (window->priv->fullscreen_controls), &w, &h);
	gdk_device_get_position (device, NULL, &x, &y);

	/* gtk seems to emit leave notify when clicking on tool items,
	 * work around it by checking the coordinates
	 */
	if (y >= h)
	{
		show_hide_fullscreen_controls (window, FALSE, h);
	}

	return FALSE;
}

static void
fullscreen_controls_setup (GeditWindow *window)
{
	GeditWindowPrivate *priv = window->priv;

	if (priv->fullscreen_controls_setup)
		return;

	gtk_window_set_transient_for (GTK_WINDOW (priv->fullscreen_controls),
				      GTK_WINDOW (&window->window));
	gtk_window_set_attached_to (GTK_WINDOW (window), priv->fullscreen_controls);

	g_signal_connect (priv->fullscreen_controls, "enter-notify-event",
			  G_CALLBACK (on_fullscreen_controls_enter_notify_event),
			  window);
	g_signal_connect (priv->fullscreen_controls, "leave-notify-event",
			  G_CALLBACK (on_fullscreen_controls_leave_notify_event),
			  window);
	priv->fullscreen_controls_setup = TRUE;
}

static void
empty_search_notify_cb (GeditDocument *doc,
			GParamSpec    *pspec,
			GeditWindow   *window)
{
	GAction *action;
	gboolean enabled;

	if (doc != gedit_window_get_active_document (window))
		return;

	enabled = !_gedit_document_get_empty_search (doc);

	action = g_action_map_lookup_action (G_ACTION_MAP (window),
	                                     "clear_highlight");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), enabled);


	action = g_action_map_lookup_action (G_ACTION_MAP (window),
	                                     "find_next");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), enabled);


	action = g_action_map_lookup_action (G_ACTION_MAP (window),
	                                     "find_prev");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), enabled);
}

static void
can_undo (GeditDocument *doc,
	  GParamSpec    *pspec,
	  GeditWindow   *window)
{
	GAction *action;
	gboolean enabled;

	enabled = gtk_source_buffer_can_undo (GTK_SOURCE_BUFFER (doc));

	if (doc != gedit_window_get_active_document (window))
		return;

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "undo");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), enabled);
}

static void
can_redo (GeditDocument *doc,
	  GParamSpec    *pspec,
	  GeditWindow   *window)
{
	GAction *action;
	gboolean enabled;

	enabled = gtk_source_buffer_can_redo (GTK_SOURCE_BUFFER (doc));

	if (doc != gedit_window_get_active_document (window))
		return;

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "redo");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), enabled);
}

static void
selection_changed (GeditDocument *doc,
		   GParamSpec    *pspec,
		   GeditWindow   *window)
{
	GeditTab *tab;
	GeditView *view;
	GAction *action;
	GeditTabState state;
	gboolean state_normal;
	gboolean editable;

	gedit_debug (DEBUG_WINDOW);

	if (doc != gedit_window_get_active_document (window))
		return;

	tab = gedit_tab_get_from_document (doc);
	state = gedit_tab_get_state (tab);
	state_normal = (state == GEDIT_TAB_STATE_NORMAL);

	view = gedit_tab_get_view (tab);
	editable = gtk_text_view_get_editable (GTK_TEXT_VIEW (view));

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "cut");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
				     state_normal &&
				     editable &&
				     gtk_text_buffer_get_has_selection (GTK_TEXT_BUFFER (doc)));

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "copy");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
				     (state_normal ||
				      state == GEDIT_TAB_STATE_EXTERNALLY_MODIFIED_NOTIFICATION) &&
				     gtk_text_buffer_get_has_selection (GTK_TEXT_BUFFER (doc)));

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "delete");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
				     state_normal &&
				     editable &&
				     gtk_text_buffer_get_has_selection (GTK_TEXT_BUFFER (doc)));

	peas_extension_set_foreach (window->priv->extensions,
	                            (PeasExtensionSetForeachFunc) extension_update_state,
	                            window);
}

static void
readonly_changed (GeditDocument *doc,
		  GParamSpec    *pspec,
		  GeditWindow   *window)
{
	set_sensitivity_according_to_tab (window,
					  gedit_window_get_active_tab (window));

	sync_name (gedit_window_get_active_tab (window), NULL, window);

	peas_extension_set_foreach (window->priv->extensions,
	                            (PeasExtensionSetForeachFunc) extension_update_state,
	                            window);
}

static void
editable_changed (GeditView  *view,
                  GParamSpec  *arg1,
                  GeditWindow *window)
{
	peas_extension_set_foreach (window->priv->extensions,
	                            (PeasExtensionSetForeachFunc) extension_update_state,
	                            window);
}

static void
update_sensitivity_according_to_open_tabs (GeditWindow *window,
					   gint         num_notebooks,
					   gint         num_tabs)
{
	GAction *action;

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "save");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), num_tabs > 0);

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "save_as");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), num_tabs > 0);

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "revert");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), num_tabs > 0);

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "print");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), num_tabs > 0);

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "find");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), num_tabs > 0);

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "find_next");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), num_tabs > 0);

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "replace");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), num_tabs > 0);

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "find_prev");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), num_tabs > 0);

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "clear_highlight");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), num_tabs > 0);

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "goto_line");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), num_tabs > 0);

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "new_tab_group");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), num_tabs > 0);

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "previous_document");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), num_tabs > 0);

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "next_document");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), num_tabs > 0);

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "move_to_new_window");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
	                             num_tabs > 1);

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "previous_tab_group");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
	                             num_notebooks > 1);

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "next_tab_group");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
	                             num_notebooks > 1);

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "highlight_mode");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), num_tabs > 0);

	/* Do not set close action insensitive on OS X */
#ifndef OS_OSX
	action = g_action_map_lookup_action (G_ACTION_MAP (window), "close");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action),
	                             num_tabs != 0);
#endif
}

static void
on_tab_added (GeditMultiNotebook *multi,
	      GeditNotebook      *notebook,
	      GeditTab           *tab,
	      GeditWindow        *window)
{
	GeditView *view;
	GeditDocument *doc;
	gint num_tabs;
	gint num_notebooks;

	gedit_debug (DEBUG_WINDOW);

	num_notebooks = gedit_multi_notebook_get_n_notebooks (multi);
	num_tabs = gedit_multi_notebook_get_n_tabs (multi);

	update_sensitivity_according_to_open_tabs (window, num_notebooks,
						   num_tabs);

	view = gedit_tab_get_view (tab);
	doc = gedit_tab_get_document (tab);

	/* IMPORTANT: remember to disconnect the signal in notebook_tab_removed
	 * if a new signal is connected here */

	g_signal_connect (tab,
			 "notify::name",
			  G_CALLBACK (sync_name),
			  window);
	g_signal_connect (tab,
			 "notify::state",
			  G_CALLBACK (sync_state),
			  window);
	g_signal_connect (tab,
			  "notify::can-close",
			  G_CALLBACK (sync_can_close),
			  window);
	g_signal_connect (tab,
			  "drop_uris",
			  G_CALLBACK (drop_uris_cb),
			  window);
	g_signal_connect (doc,
			  "bracket-matched",
			  G_CALLBACK (bracket_matched_cb),
			  window);
	g_signal_connect (doc,
			  "cursor-moved",
			  G_CALLBACK (update_cursor_position_statusbar),
			  window);
	g_signal_connect (doc,
			  "notify::empty-search",
			  G_CALLBACK (empty_search_notify_cb),
			  window);
	g_signal_connect (doc,
			  "notify::can-undo",
			  G_CALLBACK (can_undo),
			  window);
	g_signal_connect (doc,
			  "notify::can-redo",
			  G_CALLBACK (can_redo),
			  window);
	g_signal_connect (doc,
			  "notify::has-selection",
			  G_CALLBACK (selection_changed),
			  window);
	g_signal_connect (doc,
			  "notify::read-only",
			  G_CALLBACK (readonly_changed),
			  window);
	g_signal_connect (view,
			  "toggle_overwrite",
			  G_CALLBACK (update_overwrite_mode_statusbar),
			  window);
	g_signal_connect (view,
			  "notify::editable",
			  G_CALLBACK (editable_changed),
			  window);

	update_documents_list_menu (window);

	update_window_state (window);
	update_can_close (window);

	g_signal_emit (G_OBJECT (window), signals[TAB_ADDED], 0, tab);
}

static void
on_tab_removed (GeditMultiNotebook *multi,
		GeditNotebook      *notebook,
		GeditTab           *tab,
		GeditWindow        *window)
{
	GeditView     *view;
	GeditDocument *doc;
	gint num_notebooks;
	gint num_tabs;

	gedit_debug (DEBUG_WINDOW);

	num_notebooks = gedit_multi_notebook_get_n_notebooks (multi);
	num_tabs = gedit_multi_notebook_get_n_tabs (multi);

	view = gedit_tab_get_view (tab);
	doc = gedit_tab_get_document (tab);

	g_signal_handlers_disconnect_by_func (tab,
					      G_CALLBACK (sync_name),
					      window);
	g_signal_handlers_disconnect_by_func (tab,
					      G_CALLBACK (sync_state),
					      window);
	g_signal_handlers_disconnect_by_func (tab,
					      G_CALLBACK (sync_can_close),
					      window);
	g_signal_handlers_disconnect_by_func (tab,
					      G_CALLBACK (drop_uris_cb),
					      window);
	g_signal_handlers_disconnect_by_func (doc,
					      G_CALLBACK (bracket_matched_cb),
					      window);
	g_signal_handlers_disconnect_by_func (doc,
					      G_CALLBACK (update_cursor_position_statusbar),
					      window);
	g_signal_handlers_disconnect_by_func (doc,
					      G_CALLBACK (empty_search_notify_cb),
					      window);
	g_signal_handlers_disconnect_by_func (doc,
					      G_CALLBACK (can_undo),
					      window);
	g_signal_handlers_disconnect_by_func (doc,
					      G_CALLBACK (can_redo),
					      window);
	g_signal_handlers_disconnect_by_func (doc,
					      G_CALLBACK (selection_changed),
					      window);
	g_signal_handlers_disconnect_by_func (doc,
					      G_CALLBACK (readonly_changed),
					      window);
	g_signal_handlers_disconnect_by_func (view,
					      G_CALLBACK (update_overwrite_mode_statusbar),
					      window);
	g_signal_handlers_disconnect_by_func (view,
					      G_CALLBACK (editable_changed),
					      window);

	if (tab == gedit_multi_notebook_get_active_tab (multi))
	{
		g_clear_object (&window->priv->spaces_instead_of_tabs_binding);

		if (window->priv->tab_width_id)
		{
			g_signal_handler_disconnect (view, window->priv->tab_width_id);
			window->priv->tab_width_id = 0;
		}

		if (window->priv->language_changed_id)
		{
			g_signal_handler_disconnect (doc, window->priv->language_changed_id);
			window->priv->language_changed_id = 0;
		}
	}

	g_return_if_fail (num_tabs >= 0);
	if (num_tabs == 0)
	{
		set_title (window);

		/* Remove line and col info */
		gedit_statusbar_set_cursor_position (
				GEDIT_STATUSBAR (window->priv->statusbar),
				-1,
				-1);

		gedit_statusbar_clear_overwrite (
				GEDIT_STATUSBAR (window->priv->statusbar));

		/* hide the combos */
		gtk_widget_hide (window->priv->tab_width_combo);
		gtk_widget_hide (window->priv->language_button);
	}

	if (!window->priv->dispose_has_run)
	{
		if ((!window->priv->removing_tabs &&
		    gtk_notebook_get_n_pages (GTK_NOTEBOOK (notebook)) > 0) ||
		    num_tabs == 0)
		{
			update_documents_list_menu (window);
			update_next_prev_doc_sensitivity_per_window (window);
			update_sensitivity_according_to_open_tabs (window,
								   num_notebooks,
								   num_tabs);
		}

		if (num_tabs == 0)
		{
			peas_extension_set_foreach (window->priv->extensions,
					            (PeasExtensionSetForeachFunc) extension_update_state,
					            window);
		}
	}

	update_window_state (window);
	update_can_close (window);

	g_signal_emit (G_OBJECT (window), signals[TAB_REMOVED], 0, tab);
}

static void
on_page_reordered (GeditMultiNotebook *multi,
                   GeditNotebook      *notebook,
                   GtkWidget          *page,
                   gint                page_num,
                   GeditWindow        *window)
{
	update_documents_list_menu (window);
	update_next_prev_doc_sensitivity_per_window (window);

	g_signal_emit (G_OBJECT (window), signals[TABS_REORDERED], 0);
}

static GtkNotebook *
on_notebook_create_window (GeditMultiNotebook *mnb,
                           GtkNotebook        *notebook,
                           GtkWidget          *page,
                           gint                x,
                           gint                y,
                           GeditWindow        *window)
{
	GeditWindow *new_window;
	GtkWidget *new_notebook;

	new_window = clone_window (window);

	gtk_window_move (GTK_WINDOW (new_window), x, y);
	gtk_widget_show (GTK_WIDGET (new_window));

	new_notebook = _gedit_window_get_notebook (GEDIT_WINDOW (new_window));

	return GTK_NOTEBOOK (new_notebook);
}

static void
on_tab_close_request (GeditMultiNotebook *multi,
		      GeditNotebook      *notebook,
		      GeditTab           *tab,
		      GtkWindow          *window)
{
	/* Note: we are destroying the tab before the default handler
	 * seems to be ok, but we need to keep an eye on this. */
	_gedit_cmd_file_close_tab (tab, GEDIT_WINDOW (window));
}

static void
on_show_popup_menu (GeditMultiNotebook *multi,
                    GdkEventButton     *event,
                    GeditTab           *tab,
                    GeditWindow        *window)
{
	GtkWidget *menu;

	if (event == NULL)
		return;

	menu = gedit_notebook_popup_menu_new (window, tab);
	gtk_widget_show (menu);

	gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
			NULL, NULL,
			event->button, event->time);
}

static void
on_notebook_changed (GeditMultiNotebook *mnb,
		     GParamSpec         *pspec,
		     GeditWindow        *window)
{
	update_documents_list_menu (window);
	update_sensitivity_according_to_open_tabs (window,
						   gedit_multi_notebook_get_n_notebooks (mnb),
						   gedit_multi_notebook_get_n_tabs (mnb));
}

static void
on_notebook_removed (GeditMultiNotebook *mnb,
		     GeditNotebook      *notebook,
		     GeditWindow        *window)
{
	update_documents_list_menu (window);
	update_sensitivity_according_to_open_tabs (window,
						   gedit_multi_notebook_get_n_notebooks (mnb),
						   gedit_multi_notebook_get_n_tabs (mnb));
}

static void
side_panel_size_allocate (GtkWidget     *widget,
			  GtkAllocation *allocation,
			  GeditWindow   *window)
{
	window->priv->side_panel_size = allocation->width;
}

static void
bottom_panel_size_allocate (GtkWidget     *widget,
			    GtkAllocation *allocation,
			    GeditWindow   *window)
{
	window->priv->bottom_panel_size = allocation->height;
}

static void
hpaned_restore_position (GtkWidget   *widget,
			 GeditWindow *window)
{
	gint pos;

	gedit_debug_message (DEBUG_WINDOW,
			     "Restoring hpaned position: side panel size %d",
			     window->priv->side_panel_size);

	pos = MAX (100, window->priv->side_panel_size);
	gtk_paned_set_position (GTK_PANED (window->priv->hpaned), pos);

	/* start monitoring the size */
	g_signal_connect (window->priv->side_panel,
			  "size-allocate",
			  G_CALLBACK (side_panel_size_allocate),
			  window);

	/* run this only once */
	g_signal_handlers_disconnect_by_func (widget, hpaned_restore_position, window);
}

static void
vpaned_restore_position (GtkWidget   *widget,
			 GeditWindow *window)
{
	gint pos;
	GtkAllocation allocation;

	gedit_debug_message (DEBUG_WINDOW,
			     "Restoring vpaned position: bottom panel size %d",
			     window->priv->bottom_panel_size);

	gtk_widget_get_allocation (widget, &allocation);
	pos = allocation.height -
	      MAX (50, window->priv->bottom_panel_size);
	gtk_paned_set_position (GTK_PANED (window->priv->vpaned), pos);

	/* start monitoring the size */
	g_signal_connect (window->priv->bottom_panel,
			  "size-allocate",
			  G_CALLBACK (bottom_panel_size_allocate),
			  window);

	/* run this only once */
	g_signal_handlers_disconnect_by_func (widget, vpaned_restore_position, window);
}

static void
side_panel_visibility_changed (GSettings   *settings,
                               const gchar *key,
                               GeditWindow *window)
{
	GtkStyleContext *context;
	gboolean visible;

	context = gtk_widget_get_style_context (window->priv->headerbar);
	visible = g_settings_get_boolean (settings, key);

	gtk_widget_set_visible (window->priv->side_panel, visible);

	/* focus the right widget and set the right styles */
	if (visible)
	{
		gtk_style_context_add_class (context, "gedit-titlebar-right");
		gtk_widget_grab_focus (window->priv->side_panel);
	}
	else
	{
		gtk_style_context_remove_class (context, "gedit-titlebar-right");
		gtk_widget_grab_focus (GTK_WIDGET (window->priv->multi_notebook));
	}
}

static void
setup_side_panel (GeditWindow *window)
{
	GtkWidget *documents_panel;
	GtkWidget *image;

	gedit_debug (DEBUG_WINDOW);

	g_signal_connect (window->priv->ui_settings,
	                  "changed::side-panel-visible",
	                  G_CALLBACK (side_panel_visibility_changed),
	                  window);

	documents_panel = gedit_documents_panel_new (window);
	image = gtk_image_new_from_icon_name ("view-list-symbolic",
	                                      GTK_ICON_SIZE_MENU);
	gtk_widget_show (image);
	gedit_panel_add_item (GEDIT_PANEL (window->priv->side_panel),
			      documents_panel,
			      "GeditWindowDocumentsPanel",
			      _("Documents"),
			      image);
}



static void
bottom_panel_visibility_changed (GSettings   *settings,
				 const gchar *key,
				 GeditWindow *window)
{
	gboolean visible;

	visible = g_settings_get_boolean (settings, key) &&
	          gedit_panel_get_n_items (GEDIT_PANEL (window->priv->bottom_panel)) > 0;

	gtk_widget_set_visible (window->priv->bottom_panel, visible);

	/* focus the right widget */
	if (visible)
	{
		gtk_widget_grab_focus (window->priv->bottom_panel);
	}
	else
	{
		gtk_widget_grab_focus (GTK_WIDGET (window->priv->multi_notebook));
	}
}

static void
bottom_panel_item_removed (GeditPanel  *panel,
			   GtkWidget   *item,
			   GeditWindow *window)
{
	if (gedit_panel_get_n_items (panel) == 0)
	{
		gtk_widget_hide (GTK_WIDGET (panel));
	}
}

static void
bottom_panel_item_added (GeditPanel  *panel,
			 GtkWidget   *item,
			 GeditWindow *window)
{
	/* if it's the first item added, set the menu item
	 * sensitive and if needed show the panel */
	if (gedit_panel_get_n_items (panel) == 1)
	{
		gboolean show;

		show = g_settings_get_boolean (window->priv->ui_settings,
		                               "bottom-panel-visible");
		if (show)
		{
			gtk_widget_show (GTK_WIDGET (panel));
		}
	}
}

static void
setup_bottom_panel (GeditWindow *window)
{
	gedit_debug (DEBUG_WINDOW);

	g_signal_connect_after (window->priv->ui_settings,
				"changed::bottom-panel-visible",
				G_CALLBACK (bottom_panel_visibility_changed),
				window);
}

static void
init_panels_visibility (GeditWindow *window)
{
	gint active_page;
	gboolean side_panel_visible;
	gboolean bottom_panel_visible;

	gedit_debug (DEBUG_WINDOW);

	/* side panel */
	active_page = g_settings_get_int (window->priv->window_settings,
					  GEDIT_SETTINGS_SIDE_PANEL_ACTIVE_PAGE);
	_gedit_panel_set_active_item_by_id (GEDIT_PANEL (window->priv->side_panel),
					    active_page);

	side_panel_visible = g_settings_get_boolean (window->priv->ui_settings,
						    GEDIT_SETTINGS_SIDE_PANEL_VISIBLE);
	bottom_panel_visible = g_settings_get_boolean (window->priv->ui_settings,
						      GEDIT_SETTINGS_BOTTOM_PANEL_VISIBLE);

	if (side_panel_visible)
	{
		gtk_widget_show (window->priv->side_panel);
	}

	/* bottom pane, it can be empty */
	if (gedit_panel_get_n_items (GEDIT_PANEL (window->priv->bottom_panel)) > 0)
	{
		active_page = g_settings_get_int (window->priv->window_settings,
						  GEDIT_SETTINGS_BOTTOM_PANEL_ACTIVE_PAGE);
		_gedit_panel_set_active_item_by_id (GEDIT_PANEL (window->priv->bottom_panel),
						    active_page);

		if (bottom_panel_visible)
		{
			gtk_widget_show (window->priv->bottom_panel);
		}
	}

	/* start track sensitivity after the initial state is set */
	window->priv->bottom_panel_item_removed_handler_id =
		g_signal_connect (window->priv->bottom_panel,
				  "item_removed",
				  G_CALLBACK (bottom_panel_item_removed),
				  window);

	g_signal_connect (window->priv->bottom_panel,
			  "item_added",
			  G_CALLBACK (bottom_panel_item_added),
			  window);
}

static void
clipboard_owner_change (GtkClipboard        *clipboard,
			GdkEventOwnerChange *event,
			GeditWindow         *window)
{
	set_paste_sensitivity_according_to_clipboard (window,
						      clipboard);
}

static void
window_realized (GtkWidget *window,
		 gpointer  *data)
{
	GtkClipboard *clipboard;

	clipboard = gtk_widget_get_clipboard (window,
					      GDK_SELECTION_CLIPBOARD);

	g_signal_connect (clipboard,
			  "owner_change",
			  G_CALLBACK (clipboard_owner_change),
			  window);
}

static void
window_unrealized (GtkWidget *window,
		   gpointer  *data)
{
	GtkClipboard *clipboard;

	clipboard = gtk_widget_get_clipboard (window,
					      GDK_SELECTION_CLIPBOARD);

	g_signal_handlers_disconnect_by_func (clipboard,
					      G_CALLBACK (clipboard_owner_change),
					      window);
}

static void
check_window_is_active (GeditWindow *window,
			GParamSpec *property,
			gpointer useless)
{
	if (window->priv->window_state & GDK_WINDOW_STATE_FULLSCREEN)
	{
		gtk_widget_set_visible (window->priv->fullscreen_controls,
					gtk_window_is_active (GTK_WINDOW (window)));
	}
}

static void
extension_added (PeasExtensionSet *extensions,
		 PeasPluginInfo   *info,
		 PeasExtension    *exten,
		 GeditWindow      *window)
{
	gedit_window_activatable_activate (GEDIT_WINDOW_ACTIVATABLE (exten));
}

static void
extension_removed (PeasExtensionSet *extensions,
		   PeasPluginInfo   *info,
		   PeasExtension    *exten,
		   GeditWindow      *window)
{
	gedit_window_activatable_deactivate (GEDIT_WINDOW_ACTIVATABLE (exten));

	/* Ensure update of ui manager, because we suspect it does something
	 * with expected static strings in the type module (when unloaded the
	 * strings don't exist anymore, and ui manager updates in an idle
	 * func) */
	gtk_ui_manager_ensure_update (window->priv->manager);
}

static GActionEntry win_entries[] = {
	{ "open", _gedit_cmd_file_open },
	{ "new_tab", _gedit_cmd_file_new },
	{ "save", _gedit_cmd_file_save },
	{ "save_as", _gedit_cmd_file_save_as },
	{ "print", _gedit_cmd_file_print },
	{ "revert", _gedit_cmd_file_revert },
	{ "close", _gedit_cmd_file_close },
	{ "fullscreen", _gedit_cmd_view_toggle_fullscreen_mode },
	{ "leave_fullscreen", _gedit_cmd_view_leave_fullscreen_mode },
	{ "find", _gedit_cmd_search_find },
	{ "find_next", _gedit_cmd_search_find_next },
	{ "find_prev", _gedit_cmd_search_find_prev },
	{ "replace", _gedit_cmd_search_replace },
	{ "clear_highlight", _gedit_cmd_search_clear_highlight },
	{ "goto_line", _gedit_cmd_search_goto_line },
	{ "save_all", _gedit_cmd_file_save_all },
	{ "close_all", _gedit_cmd_file_close_all },
	{ "new_tab_group", _gedit_cmd_documents_new_tab_group },
	{ "previous_tab_group", _gedit_cmd_documents_previous_tab_group },
	{ "next_tab_group", _gedit_cmd_documents_next_tab_group },
	{ "previous_document", _gedit_cmd_documents_previous_document },
	{ "next_document", _gedit_cmd_documents_next_document },
	{ "move_to_new_window", _gedit_cmd_documents_move_to_new_window },
	{ "undo", _gedit_cmd_edit_undo },
	{ "redo", _gedit_cmd_edit_redo },
	{ "cut", _gedit_cmd_edit_cut },
	{ "copy", _gedit_cmd_edit_copy },
	{ "paste", _gedit_cmd_edit_paste },
	{ "delete", _gedit_cmd_edit_delete },
	{ "select_all", _gedit_cmd_edit_select_all },
	{ "highlight_mode", _gedit_cmd_view_highlight_mode }
};

static void
gedit_window_init (GeditWindow *window)
{
	GtkTargetList *tl;

	gedit_debug (DEBUG_WINDOW);

	window->priv = gedit_window_get_instance_private (window);

	window->priv->removing_tabs = FALSE;
	window->priv->state = GEDIT_WINDOW_STATE_NORMAL;
	window->priv->inhibition_cookie = 0;
	window->priv->dispose_has_run = FALSE;
	window->priv->fullscreen_controls = NULL;
	window->priv->fullscreen_animation_timeout_id = 0;
	window->priv->direct_save_uri = NULL;
	window->priv->editor_settings = g_settings_new ("org.gnome.gedit.preferences.editor");
	window->priv->ui_settings = g_settings_new ("org.gnome.gedit.preferences.ui");

	/* window settings are applied only once the window is closed. We do not
	   want to keep writing to disk when the window is dragged around */
	window->priv->window_settings = g_settings_new ("org.gnome.gedit.state.window");
	g_settings_delay (window->priv->window_settings);

	window->priv->message_bus = gedit_message_bus_new ();

	gtk_widget_init_template (GTK_WIDGET (window));

	g_action_map_add_action_entries (G_ACTION_MAP (window),
	                                 win_entries,
	                                 G_N_ELEMENTS (win_entries),
	                                 window);

	window->priv->window_group = gtk_window_group_new ();
	gtk_window_group_add_window (window->priv->window_group, GTK_WINDOW (window));

	/* Add menu bar and toolbar bar */
	// FIXME: kill this, right now it is just not added to the window
	create_menu_bar_and_toolbar (window);

	g_object_bind_property (window->priv->side_panel,
	                        "visible",
	                        window->priv->side_headerbar,
	                        "visible",
	                        G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
	g_object_bind_property (window->priv->side_panel,
	                        "active-item-label",
	                        window->priv->side_headerbar,
	                        "title",
	                        G_BINDING_DEFAULT);
	g_object_bind_property (window->priv->titlebar_paned,
	                        "position",
	                        window->priv->hpaned,
	                        "position",
	                        G_BINDING_DEFAULT | G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

	setup_headerbar_open_button (window);

	/* Setup status bar */
	setup_statusbar (window);

	/* Setup main area */
	g_signal_connect (window->priv->multi_notebook,
			  "notebook-removed",
			  G_CALLBACK (on_notebook_removed),
			  window);
	g_signal_connect (window->priv->multi_notebook,
			  "notify::active-notebook",
			  G_CALLBACK (on_notebook_changed),
			  window);

	g_signal_connect (window->priv->multi_notebook,
			  "tab-added",
			  G_CALLBACK (on_tab_added),
			  window);

	g_signal_connect (window->priv->multi_notebook,
			  "tab-removed",
			  G_CALLBACK (on_tab_removed),
			  window);

	g_signal_connect (window->priv->multi_notebook,
			  "switch-tab",
			  G_CALLBACK (tab_switched),
			  window);

	g_signal_connect (window->priv->multi_notebook,
			  "tab-close-request",
			  G_CALLBACK (on_tab_close_request),
			  window);

	g_signal_connect (window->priv->multi_notebook,
			  "page-reordered",
			  G_CALLBACK (on_page_reordered),
			  window);

	g_signal_connect (window->priv->multi_notebook,
	                  "create-window",
	                  G_CALLBACK (on_notebook_create_window),
	                  window);

	g_signal_connect (window->priv->multi_notebook,
			  "show-popup-menu",
			  G_CALLBACK (on_show_popup_menu),
			  window);

	/* side and bottom panels */
	setup_side_panel (window);
	setup_bottom_panel (window);

	/* panels' state must be restored after panels have been mapped,
	 * since the bottom panel position depends on the size of the vpaned. */
	window->priv->side_panel_size = g_settings_get_int (window->priv->window_settings,
							    GEDIT_SETTINGS_SIDE_PANEL_SIZE);
	window->priv->bottom_panel_size = g_settings_get_int (window->priv->window_settings,
							      GEDIT_SETTINGS_BOTTOM_PANEL_SIZE);

	g_signal_connect_after (window->priv->hpaned,
				"map",
				G_CALLBACK (hpaned_restore_position),
				window);
	g_signal_connect_after (window->priv->vpaned,
				"map",
				G_CALLBACK (vpaned_restore_position),
				window);

	/* Drag and drop support */
	gtk_drag_dest_set (GTK_WIDGET (window),
			   GTK_DEST_DEFAULT_MOTION |
			   GTK_DEST_DEFAULT_HIGHLIGHT |
			   GTK_DEST_DEFAULT_DROP,
			   drop_types,
			   G_N_ELEMENTS (drop_types),
			   GDK_ACTION_COPY);

	/* Add uri targets */
	tl = gtk_drag_dest_get_target_list (GTK_WIDGET (window));

	if (tl == NULL)
	{
		tl = gtk_target_list_new (drop_types, G_N_ELEMENTS (drop_types));
		gtk_drag_dest_set_target_list (GTK_WIDGET (window), tl);
		gtk_target_list_unref (tl);
	}

	gtk_target_list_add_uri_targets (tl, TARGET_URI_LIST);

	/* connect instead of override, so that we can
	 * share the cb code with the view */
	g_signal_connect (window,
			  "drag_data_received",
	                  G_CALLBACK (drag_data_received_cb),
	                  NULL);
	g_signal_connect (window,
			  "drag_drop",
	                  G_CALLBACK (drag_drop_cb),
	                  NULL);

	/* we can get the clipboard only after the widget
	 * is realized */
	g_signal_connect (window,
			  "realize",
			  G_CALLBACK (window_realized),
			  NULL);
	g_signal_connect (window,
			  "unrealize",
			  G_CALLBACK (window_unrealized),
			  NULL);

	/* Check if the window is active for fullscreen */
	g_signal_connect (window,
			  "notify::is-active",
			  G_CALLBACK (check_window_is_active),
			  NULL);

	gedit_debug_message (DEBUG_WINDOW, "Update plugins ui");

	window->priv->extensions = peas_extension_set_new (PEAS_ENGINE (gedit_plugins_engine_get_default ()),
							   GEDIT_TYPE_WINDOW_ACTIVATABLE,
							   "window", window,
							   NULL);
	g_signal_connect (window->priv->extensions,
			  "extension-added",
			  G_CALLBACK (extension_added),
			  window);
	g_signal_connect (window->priv->extensions,
			  "extension-removed",
			  G_CALLBACK (extension_removed),
			  window);
	peas_extension_set_foreach (window->priv->extensions,
	                            (PeasExtensionSetForeachFunc) extension_added,
	                            window);


	/* set visibility of panels.
	 * This needs to be done after plugins activatation */
	init_panels_visibility (window);

	/* When we initiate we have 1 notebook and 1 tab */
	update_sensitivity_according_to_open_tabs (window, 1, 1);

	gedit_debug_message (DEBUG_WINDOW, "END");
}

/**
 * gedit_window_get_active_view:
 * @window: a #GeditWindow
 *
 * Gets the active #GeditView.
 *
 * Returns: (transfer none): the active #GeditView
 */
GeditView *
gedit_window_get_active_view (GeditWindow *window)
{
	GeditTab *tab;
	GeditView *view;

	g_return_val_if_fail (GEDIT_IS_WINDOW (window), NULL);

	tab = gedit_window_get_active_tab (window);

	if (tab == NULL)
		return NULL;

	view = gedit_tab_get_view (tab);

	return view;
}

/**
 * gedit_window_get_active_document:
 * @window: a #GeditWindow
 *
 * Gets the active #GeditDocument.
 *
 * Returns: (transfer none): the active #GeditDocument
 */
GeditDocument *
gedit_window_get_active_document (GeditWindow *window)
{
	GeditView *view;

	g_return_val_if_fail (GEDIT_IS_WINDOW (window), NULL);

	view = gedit_window_get_active_view (window);
	if (view == NULL)
		return NULL;

	return GEDIT_DOCUMENT (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)));
}

GtkWidget *
_gedit_window_get_multi_notebook (GeditWindow *window)
{
	g_return_val_if_fail (GEDIT_IS_WINDOW (window), NULL);

	return GTK_WIDGET (window->priv->multi_notebook);
}

GtkWidget *
_gedit_window_get_notebook (GeditWindow *window)
{
	g_return_val_if_fail (GEDIT_IS_WINDOW (window), NULL);

	return GTK_WIDGET (gedit_multi_notebook_get_active_notebook (window->priv->multi_notebook));
}

static GeditTab *
process_create_tab (GeditWindow *window,
                    GtkWidget   *notebook,
                    GeditTab    *tab,
                    gboolean     jump_to)
{
	if (tab == NULL)
	{
		return NULL;
	}

	gedit_debug (DEBUG_WINDOW);

	gtk_widget_show (GTK_WIDGET (tab));
	gedit_notebook_add_tab (GEDIT_NOTEBOOK (notebook),
	                        tab,
	                        -1,
	                        jump_to);

	if (!gtk_widget_get_visible (GTK_WIDGET (window)))
	{
		gtk_window_present (GTK_WINDOW (window));
	}

	return tab;
}

/**
 * gedit_window_create_tab:
 * @window: a #GeditWindow
 * @jump_to: %TRUE to set the new #GeditTab as active
 *
 * Creates a new #GeditTab and adds the new tab to the #GeditNotebook.
 * In case @jump_to is %TRUE the #GeditNotebook switches to that new #GeditTab.
 *
 * Returns: (transfer none): a new #GeditTab
 */
GeditTab *
gedit_window_create_tab (GeditWindow *window,
			 gboolean     jump_to)
{
	GtkWidget *notebook;
	GeditTab *tab;

	g_return_val_if_fail (GEDIT_IS_WINDOW (window), NULL);

	gedit_debug (DEBUG_WINDOW);

	notebook = _gedit_window_get_notebook (window);
	tab = GEDIT_TAB (_gedit_tab_new ());
	gtk_widget_show (GTK_WIDGET (tab));

	return process_create_tab (window, notebook, tab, jump_to);
}

/**
 * gedit_window_create_tab_from_location:
 * @window: a #GeditWindow
 * @location: the location of the document
 * @encoding: (allow-none): a #GeditEncoding, or %NULL
 * @line_pos: the line position to visualize
 * @column_pos: the column position to visualize
 * @create: %TRUE to create a new document in case @uri does exist
 * @jump_to: %TRUE to set the new #GeditTab as active
 *
 * Creates a new #GeditTab loading the document specified by @uri.
 * In case @jump_to is %TRUE the #GeditNotebook swithes to that new #GeditTab.
 * Whether @create is %TRUE, creates a new empty document if location does
 * not refer to an existing file
 *
 * Returns: (transfer none): a new #GeditTab
 */
GeditTab *
gedit_window_create_tab_from_location (GeditWindow         *window,
				       GFile               *location,
				       const GeditEncoding *encoding,
				       gint                 line_pos,
				       gint                 column_pos,
				       gboolean             create,
				       gboolean             jump_to)
{
	GtkWidget *notebook;
	GtkWidget *tab;

	g_return_val_if_fail (GEDIT_IS_WINDOW (window), NULL);
	g_return_val_if_fail (G_IS_FILE (location), NULL);

	gedit_debug (DEBUG_WINDOW);

	notebook = _gedit_window_get_notebook (window);
	tab = _gedit_tab_new_from_location (location,
	                                    encoding,
	                                    line_pos,
	                                    column_pos,
	                                    create);

	return process_create_tab (window, notebook, GEDIT_TAB (tab), jump_to);
}

/**
 * gedit_window_create_tab_from_stream:
 * @window: a #GeditWindow
 * @stream: a #GInputStream
 * @encoding: (allow-none): a #GeditEncoding, or %NULL
 * @line_pos: the line position to visualize
 * @column_pos: the column position to visualize
 * @jump_to: %TRUE to set the new #GeditTab as active
 *
 * Returns: (transfer none): a new #GeditTab
 */
GeditTab *
gedit_window_create_tab_from_stream (GeditWindow         *window,
                                     GInputStream        *stream,
                                     const GeditEncoding *encoding,
                                     gint                 line_pos,
                                     gint                 column_pos,
                                     gboolean             jump_to)
{
	GtkWidget *notebook;
	GtkWidget *tab;

	gedit_debug (DEBUG_WINDOW);

	g_return_val_if_fail (GEDIT_IS_WINDOW (window), NULL);
	g_return_val_if_fail (G_IS_INPUT_STREAM (stream), NULL);

	notebook = _gedit_window_get_notebook (window);
	tab = _gedit_tab_new_from_stream (stream,
	                                  encoding,
	                                  line_pos,
	                                  column_pos);

	return process_create_tab (window, notebook, GEDIT_TAB (tab), jump_to);
}

/**
 * gedit_window_get_active_tab:
 * @window: a GeditWindow
 *
 * Gets the active #GeditTab in the @window.
 *
 * Returns: (transfer none): the active #GeditTab in the @window.
 */
GeditTab *
gedit_window_get_active_tab (GeditWindow *window)
{
	g_return_val_if_fail (GEDIT_IS_WINDOW (window), NULL);

	return (window->priv->multi_notebook == NULL) ? NULL :
			gedit_multi_notebook_get_active_tab (window->priv->multi_notebook);
}

static void
add_document (GeditTab  *tab,
	      GList    **res)
{
	GeditDocument *doc;

	doc = gedit_tab_get_document (tab);

	*res = g_list_prepend (*res, doc);
}

/**
 * gedit_window_get_documents:
 * @window: a #GeditWindow
 *
 * Gets a newly allocated list with all the documents in the window.
 * This list must be freed.
 *
 * Returns: (element-type Gedit.Document) (transfer container): a newly
 * allocated list with all the documents in the window
 */
GList *
gedit_window_get_documents (GeditWindow *window)
{
	GList *res = NULL;

	g_return_val_if_fail (GEDIT_IS_WINDOW (window), NULL);

	gedit_multi_notebook_foreach_tab (window->priv->multi_notebook,
					  (GtkCallback)add_document,
					  &res);

	res = g_list_reverse (res);

	return res;
}

static void
add_view (GeditTab  *tab,
	  GList    **res)
{
	GeditView *view;

	view = gedit_tab_get_view (tab);

	*res = g_list_prepend (*res, view);
}

/**
 * gedit_window_get_views:
 * @window: a #GeditWindow
 *
 * Gets a list with all the views in the window. This list must be freed.
 *
 * Returns: (element-type Gedit.View) (transfer container): a newly allocated
 * list with all the views in the window
 */
GList *
gedit_window_get_views (GeditWindow *window)
{
	GList *res = NULL;

	g_return_val_if_fail (GEDIT_IS_WINDOW (window), NULL);

	gedit_multi_notebook_foreach_tab (window->priv->multi_notebook,
					  (GtkCallback)add_view,
					  &res);

	res = g_list_reverse (res);

	return res;
}

/**
 * gedit_window_close_tab:
 * @window: a #GeditWindow
 * @tab: the #GeditTab to close
 *
 * Closes the @tab.
 */
void
gedit_window_close_tab (GeditWindow *window,
			GeditTab    *tab)
{
	GList *tabs = NULL;

	g_return_if_fail (GEDIT_IS_WINDOW (window));
	g_return_if_fail (GEDIT_IS_TAB (tab));
	g_return_if_fail ((gedit_tab_get_state (tab) != GEDIT_TAB_STATE_SAVING) &&
			  (gedit_tab_get_state (tab) != GEDIT_TAB_STATE_SHOWING_PRINT_PREVIEW));

	tabs = g_list_append (tabs, tab);
	gedit_multi_notebook_close_tabs (window->priv->multi_notebook, tabs);
	g_list_free (tabs);
}

/**
 * gedit_window_close_all_tabs:
 * @window: a #GeditWindow
 *
 * Closes all opened tabs.
 */
void
gedit_window_close_all_tabs (GeditWindow *window)
{
	g_return_if_fail (GEDIT_IS_WINDOW (window));
	g_return_if_fail (!(window->priv->state & GEDIT_WINDOW_STATE_SAVING));

	window->priv->removing_tabs = TRUE;

	gedit_multi_notebook_close_all_tabs (window->priv->multi_notebook);

	window->priv->removing_tabs = FALSE;
}

/**
 * gedit_window_close_tabs:
 * @window: a #GeditWindow
 * @tabs: (element-type Gedit.Tab): a list of #GeditTab
 *
 * Closes all tabs specified by @tabs.
 */
void
gedit_window_close_tabs (GeditWindow *window,
			 const GList *tabs)
{
	g_return_if_fail (GEDIT_IS_WINDOW (window));
	g_return_if_fail (!(window->priv->state & GEDIT_WINDOW_STATE_SAVING));

	window->priv->removing_tabs = TRUE;

	gedit_multi_notebook_close_tabs (window->priv->multi_notebook, tabs);

	window->priv->removing_tabs = FALSE;
}

GeditWindow *
_gedit_window_move_tab_to_new_window (GeditWindow *window,
				      GeditTab    *tab)
{
	GeditWindow *new_window;
	GeditNotebook *old_notebook;
	GeditNotebook *new_notebook;

	g_return_val_if_fail (GEDIT_IS_WINDOW (window), NULL);
	g_return_val_if_fail (GEDIT_IS_TAB (tab), NULL);
	g_return_val_if_fail (gedit_multi_notebook_get_n_notebooks (
	                        window->priv->multi_notebook) > 1 ||
	                      gedit_multi_notebook_get_n_tabs (
	                        window->priv->multi_notebook) > 1,
	                      NULL);

	new_window = clone_window (window);

	old_notebook = gedit_multi_notebook_get_active_notebook (window->priv->multi_notebook);
	new_notebook = gedit_multi_notebook_get_active_notebook (new_window->priv->multi_notebook);

	gedit_notebook_move_tab (old_notebook,
				 new_notebook,
				 tab,
				 -1);

	gtk_widget_show (GTK_WIDGET (new_window));

	return new_window;
}

void
_gedit_window_move_tab_to_new_tab_group (GeditWindow *window,
                                         GeditTab    *tab)
{
	g_return_if_fail (GEDIT_IS_WINDOW (window));
	g_return_if_fail (GEDIT_IS_TAB (tab));

	gedit_multi_notebook_add_new_notebook_with_tab (window->priv->multi_notebook,
	                                                tab);
}

/**
 * gedit_window_set_active_tab:
 * @window: a #GeditWindow
 * @tab: a #GeditTab
 *
 * Switches to the tab that matches with @tab.
 */
void
gedit_window_set_active_tab (GeditWindow *window,
			     GeditTab    *tab)
{
	g_return_if_fail (GEDIT_IS_WINDOW (window));

	gedit_multi_notebook_set_active_tab (window->priv->multi_notebook,
					     tab);
}

/**
 * gedit_window_get_group:
 * @window: a #GeditWindow
 *
 * Gets the #GtkWindowGroup in which @window resides.
 *
 * Returns: (transfer none): the #GtkWindowGroup
 */
GtkWindowGroup *
gedit_window_get_group (GeditWindow *window)
{
	g_return_val_if_fail (GEDIT_IS_WINDOW (window), NULL);

	return window->priv->window_group;
}

gboolean
_gedit_window_is_removing_tabs (GeditWindow *window)
{
	g_return_val_if_fail (GEDIT_IS_WINDOW (window), FALSE);

	return window->priv->removing_tabs;
}

/**
 * gedit_window_get_ui_manager:
 * @window: a #GeditWindow
 *
 * Gets the #GtkUIManager associated with the @window.
 *
 * Returns: (transfer none): the #GtkUIManager of the @window.
 */
GtkUIManager *
gedit_window_get_ui_manager (GeditWindow *window)
{
	g_return_val_if_fail (GEDIT_IS_WINDOW (window), NULL);

	return window->priv->manager;
}

/**
 * gedit_window_get_side_panel:
 * @window: a #GeditWindow
 *
 * Gets the side #GeditPanel of the @window.
 *
 * Returns: (transfer none): the side #GeditPanel.
 */
GeditPanel *
gedit_window_get_side_panel (GeditWindow *window)
{
	g_return_val_if_fail (GEDIT_IS_WINDOW (window), NULL);

	return GEDIT_PANEL (window->priv->side_panel);
}

/**
 * gedit_window_get_bottom_panel:
 * @window: a #GeditWindow
 *
 * Gets the bottom #GeditPanel of the @window.
 *
 * Returns: (transfer none): the bottom #GeditPanel.
 */
GeditPanel *
gedit_window_get_bottom_panel (GeditWindow *window)
{
	g_return_val_if_fail (GEDIT_IS_WINDOW (window), NULL);

	return GEDIT_PANEL (window->priv->bottom_panel);
}

/**
 * gedit_window_get_statusbar:
 * @window: a #GeditWindow
 *
 * Gets the #GeditStatusbar of the @window.
 *
 * Returns: (transfer none): the #GeditStatusbar of the @window.
 */
GtkWidget *
gedit_window_get_statusbar (GeditWindow *window)
{
	g_return_val_if_fail (GEDIT_IS_WINDOW (window), 0);

	return window->priv->statusbar;
}

/**
 * gedit_window_get_state:
 * @window: a #GeditWindow
 *
 * Retrieves the state of the @window.
 *
 * Returns: the current #GeditWindowState of the @window.
 */
GeditWindowState
gedit_window_get_state (GeditWindow *window)
{
	g_return_val_if_fail (GEDIT_IS_WINDOW (window), GEDIT_WINDOW_STATE_NORMAL);

	return window->priv->state;
}

GFile *
_gedit_window_get_default_location (GeditWindow *window)
{
	g_return_val_if_fail (GEDIT_IS_WINDOW (window), NULL);

	return window->priv->default_location != NULL ?
		g_object_ref (window->priv->default_location) : NULL;
}

void
_gedit_window_set_default_location (GeditWindow *window,
				    GFile       *location)
{
	GFile *dir;

	g_return_if_fail (GEDIT_IS_WINDOW (window));
	g_return_if_fail (G_IS_FILE (location));

	dir = g_file_get_parent (location);
	g_return_if_fail (dir != NULL);

	if (window->priv->default_location != NULL)
		g_object_unref (window->priv->default_location);

	window->priv->default_location = dir;
}

static void
add_unsaved_doc (GeditTab *tab,
		 GList   **res)
{
	if (!_gedit_tab_get_can_close (tab))
	{
		GeditDocument *doc;

		doc = gedit_tab_get_document (tab);
		*res = g_list_prepend (*res, doc);
	}
}

/**
 * gedit_window_get_unsaved_documents:
 * @window: a #GeditWindow
 *
 * Gets the list of documents that need to be saved before closing the window.
 *
 * Returns: (element-type Gedit.Document) (transfer container): a list of
 * #GeditDocument that need to be saved before closing the window
 */
GList *
gedit_window_get_unsaved_documents (GeditWindow *window)
{
	GList *res = NULL;

	g_return_val_if_fail (GEDIT_IS_WINDOW (window), NULL);

	gedit_multi_notebook_foreach_tab (window->priv->multi_notebook,
					  (GtkCallback)add_unsaved_doc,
					  &res);

	return g_list_reverse (res);
}

GList *
_gedit_window_get_all_tabs (GeditWindow *window)
{
	g_return_val_if_fail (GEDIT_IS_WINDOW (window), NULL);

	return gedit_multi_notebook_get_all_tabs (window->priv->multi_notebook);
}

static void
hide_notebook_tabs_on_fullscreen (GtkNotebook	*notebook,
				  GParamSpec	*pspec,
				  GeditWindow	*window)
{
	gtk_notebook_set_show_tabs (notebook, FALSE);
}

static void
hide_notebook_tabs (GtkNotebook *notebook,
		    GeditWindow *window)
{
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (notebook), FALSE);
	g_signal_connect (notebook, "notify::show-tabs",
			  G_CALLBACK (hide_notebook_tabs_on_fullscreen),
			  window);
}

void
_gedit_window_fullscreen (GeditWindow *window)
{
	g_return_if_fail (GEDIT_IS_WINDOW (window));

	if (_gedit_window_is_fullscreen (window))
		return;

	/* Go to fullscreen mode and hide bars */
	gtk_window_fullscreen (GTK_WINDOW (&window->window));

	gedit_multi_notebook_foreach_notebook (window->priv->multi_notebook,
					       (GtkCallback)hide_notebook_tabs,
					       window);

	gtk_widget_hide (window->priv->statusbar);

	fullscreen_controls_setup (window);
	fullscreen_controls_show (window);
}

static void
show_notebook_tabs (GtkNotebook *notebook,
		    GeditWindow *window)
{
	g_signal_handlers_disconnect_by_func (notebook,
					      hide_notebook_tabs_on_fullscreen,
					      window);
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (notebook), TRUE);
}

void
_gedit_window_unfullscreen (GeditWindow *window)
{
	g_return_if_fail (GEDIT_IS_WINDOW (window));

	if (!_gedit_window_is_fullscreen (window))
		return;

	/* Unfullscreen and show bars */
	gtk_window_unfullscreen (GTK_WINDOW (&window->window));

	gedit_multi_notebook_foreach_notebook (window->priv->multi_notebook,
					       (GtkCallback)show_notebook_tabs,
					       window);

	if (g_settings_get_boolean (window->priv->ui_settings, "statusbar-visible"))
	{
		gtk_widget_show (window->priv->statusbar);
	}

	gtk_widget_hide (window->priv->fullscreen_controls);
}

gboolean
_gedit_window_is_fullscreen (GeditWindow *window)
{
	g_return_val_if_fail (GEDIT_IS_WINDOW (window), FALSE);

	return window->priv->window_state & GDK_WINDOW_STATE_FULLSCREEN;
}

/**
 * gedit_window_get_tab_from_location:
 * @window: a #GeditWindow
 * @location: a #GFile
 *
 * Gets the #GeditTab that matches with the given @location.
 *
 * Returns: (transfer none): the #GeditTab that matches with the given @location.
 */
GeditTab *
gedit_window_get_tab_from_location (GeditWindow *window,
				    GFile       *location)
{
	GList *tabs;
	GList *l;
	GeditTab *ret = NULL;

	g_return_val_if_fail (GEDIT_IS_WINDOW (window), NULL);
	g_return_val_if_fail (G_IS_FILE (location), NULL);

	tabs = gedit_multi_notebook_get_all_tabs (window->priv->multi_notebook);

	for (l = tabs; l != NULL; l = g_list_next (l))
	{
		GeditDocument *d;
		GeditTab *t;
		GFile *f;

		t = GEDIT_TAB (l->data);
		d = gedit_tab_get_document (t);

		f = gedit_document_get_location (d);

		if ((f != NULL))
		{
			gboolean found = g_file_equal (location, f);

			g_object_unref (f);

			if (found)
			{
				ret = t;
				break;
			}
		}
	}

	g_list_free (tabs);

	return ret;
}

/**
 * gedit_window_get_message_bus:
 * @window: a #GeditWindow
 *
 * Gets the #GeditMessageBus associated with @window. The returned reference
 * is owned by the window and should not be unreffed.
 *
 * Return value: (transfer none): the #GeditMessageBus associated with @window
 */
GeditMessageBus *
gedit_window_get_message_bus (GeditWindow *window)
{
	g_return_val_if_fail (GEDIT_IS_WINDOW (window), NULL);

	return window->priv->message_bus;
}

/**
 * _gedit_window_get_gear_menu:
 * @window: a #GeditWindow.
 *
 * Gets the gear menu.
 *
 * Returns: (transfer none): the #GMenuModel of the gear menu button.
 */
GMenuModel *
_gedit_window_get_gear_menu (GeditWindow *window)
{
	g_return_val_if_fail (GEDIT_IS_WINDOW (window), NULL);

	return window->priv->gear_menu;
}

/* ex:set ts=8 noet: */
