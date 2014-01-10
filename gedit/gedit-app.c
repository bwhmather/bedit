/*
 * gedit-app.c
 * This file is part of gedit
 *
 * Copyright (C) 2005-2006 - Paolo Maggi
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

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <locale.h>

#include <glib/gi18n.h>
#include <libpeas/peas-extension-set.h>
#include <gtksourceview/gtksource.h>

#ifdef ENABLE_INTROSPECTION
#include <girepository.h>
#endif

#include "gedit-app.h"
#include "gedit-commands.h"
#include "gedit-notebook.h"
#include "gedit-debug.h"
#include "gedit-utils.h"
#include "gedit-enum-types.h"
#include "gedit-dirs.h"
#include "gedit-settings.h"
#include "gedit-app-activatable.h"
#include "gedit-plugins-engine.h"
#include "gedit-commands.h"
#include "gedit-preferences-dialog.h"

#ifndef ENABLE_GVFS_METADATA
#include "gedit-metadata-manager.h"
#define METADATA_FILE "gedit-metadata.xml"
#endif

#define GEDIT_PAGE_SETUP_FILE		"gedit-page-setup"
#define GEDIT_PRINT_SETTINGS_FILE	"gedit-print-settings"

/* Properties */
enum
{
	PROP_0,
	PROP_LOCKDOWN
};

struct _GeditAppPrivate
{
	GeditPluginsEngine *engine;

	GeditLockdownMask  lockdown;

	GtkPageSetup      *page_setup;
	GtkPrintSettings  *print_settings;

	GObject           *settings;
	GSettings         *ui_settings;
	GSettings         *window_settings;

	PeasExtensionSet  *extensions;
};

static gboolean help = FALSE;
static gboolean version = FALSE;
static gboolean list_encodings = FALSE;
static gchar *encoding_charset = NULL;
static gboolean new_window = FALSE;
static gboolean new_document = FALSE;
static gchar *geometry = NULL;
static gboolean wait = FALSE;
static gboolean gapplication_service = FALSE;
static gboolean standalone = FALSE;
static gchar **remaining_args = NULL;
static const GeditEncoding *encoding = NULL;
static GInputStream *stdin_stream = NULL;
static GSList *file_list = NULL;
static gint line_position = 0;
static gint column_position = 0;
static GApplicationCommandLine *command_line = NULL;

static const GOptionEntry options[] =
{
	/* Help */
	{
		"help", '?', 0, G_OPTION_ARG_NONE, &help,
		N_("Show the application's help"), NULL
	},

	/* Version */
	{
		"version", 'V', 0, G_OPTION_ARG_NONE, &version,
		N_("Show the application's version"), NULL
	},

	/* List available encodings */
	{
		"list-encodings", '\0', 0, G_OPTION_ARG_NONE, &list_encodings,
		N_("Display list of possible values for the encoding option"),
		NULL
	},

	/* Encoding */
	{
		"encoding", '\0', 0, G_OPTION_ARG_STRING,
		&encoding_charset,
		N_("Set the character encoding to be used to open the files listed on the command line"),
		N_("ENCODING")
	},

	/* Open a new window */
	{
		"new-window", '\0', 0, G_OPTION_ARG_NONE,
		&new_window,
		N_("Create a new top-level window in an existing instance of gedit"),
		NULL
	},

	/* Create a new empty document */
	{
		"new-document", '\0', 0, G_OPTION_ARG_NONE,
		&new_document,
		N_("Create a new document in an existing instance of gedit"),
		NULL
	},

	/* Window geometry */
	{
		"geometry", 'g', 0, G_OPTION_ARG_STRING,
		&geometry,
		N_("Set the size and position of the window (WIDTHxHEIGHT+X+Y)"),
		N_("GEOMETRY")
	},

	/* GApplication service mode */
	{
		"gapplication-service", '\0', 0, G_OPTION_ARG_NONE,
		&gapplication_service,
		N_("Enter GApplication service mode"),
		NULL
	},

	/* Wait for closing documents */
	{
		"wait", 'w', 0, G_OPTION_ARG_NONE,
		&wait,
		N_("Open files and block process until files are closed"),
		NULL
	},

	/* New instance */
	{
		"standalone", 's', 0, G_OPTION_ARG_NONE,
		&standalone,
		N_("Run gedit in standalone mode"),
		NULL
	},

	/* collects file arguments */
	{
		G_OPTION_REMAINING, '\0', 0, G_OPTION_ARG_FILENAME_ARRAY,
		&remaining_args,
		NULL,
		N_("[FILE...] [+LINE[:COLUMN]]")
	},

	{NULL}
};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GeditApp, gedit_app, GTK_TYPE_APPLICATION)

static void
gedit_app_dispose (GObject *object)
{
	GeditApp *app = GEDIT_APP (object);

	g_clear_object (&app->priv->ui_settings);
	g_clear_object (&app->priv->window_settings);
	g_clear_object (&app->priv->settings);

	g_clear_object (&app->priv->page_setup);
	g_clear_object (&app->priv->print_settings);

	/* Note that unreffing the extensions will automatically remove
	   all extensions which in turn will deactivate the extension */
	g_clear_object (&app->priv->extensions);

	g_clear_object (&app->priv->engine);

	G_OBJECT_CLASS (gedit_app_parent_class)->dispose (object);
}

static void
gedit_app_get_property (GObject    *object,
			guint       prop_id,
			GValue     *value,
			GParamSpec *pspec)
{
	GeditApp *app = GEDIT_APP (object);

	switch (prop_id)
	{
		case PROP_LOCKDOWN:
			g_value_set_flags (value, gedit_app_get_lockdown (app));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static gchar *
gedit_app_help_link_id_impl (GeditApp    *app,
                             const gchar *name,
                             const gchar *link_id)
{
	if (link_id)
	{
		return g_strdup_printf ("help:%s/%s", name, link_id);
	}
	else
	{
		return g_strdup_printf ("help:%s", name);
	}
}

static gboolean
gedit_app_show_help_impl (GeditApp    *app,
                          GtkWindow   *parent,
                          const gchar *name,
                          const gchar *link_id)
{
	GError *error = NULL;
	gboolean ret;
	gchar *link;

	if (name == NULL)
	{
		name = "gedit";
	}
	else if (strcmp (name, "gedit.xml") == 0)
	{
		g_warning ("%s: Using \"gedit.xml\" for the help name is deprecated, use \"gedit\" or simply NULL instead", G_STRFUNC);
		name = "gedit";
	}

	link = GEDIT_APP_GET_CLASS (app)->help_link_id (app, name, link_id);

	ret = gtk_show_uri (gtk_widget_get_screen (GTK_WIDGET (parent)),
	                    link,
	                    GDK_CURRENT_TIME,
	                    &error);

	g_free (link);

	if (error != NULL)
	{
		GtkWidget *dialog;

		dialog = gtk_message_dialog_new (parent,
						 GTK_DIALOG_DESTROY_WITH_PARENT,
						 GTK_MESSAGE_ERROR,
						 GTK_BUTTONS_CLOSE,
						 _("There was an error displaying the help."));

		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
							  "%s", error->message);

		g_signal_connect (G_OBJECT (dialog),
				  "response",
				  G_CALLBACK (gtk_widget_destroy),
				  NULL);

		gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

		gtk_widget_show (dialog);

		g_error_free (error);
	}

	return ret;
}

static void
gedit_app_set_window_title_impl (GeditApp    *app,
                                 GeditWindow *window,
                                 const gchar *title)
{
	gtk_window_set_title (GTK_WINDOW (window), title);
}

static void
new_window_activated (GSimpleAction *action,
                      GVariant      *parameter,
                      gpointer       user_data)
{
	GeditApp *app;
	GeditWindow *window;

	app = GEDIT_APP (user_data);
	window = gedit_app_create_window (app, NULL);

	gedit_debug_message (DEBUG_APP, "Show window");
	gtk_widget_show (GTK_WIDGET (window));

	gedit_debug_message (DEBUG_APP, "Create tab");
	gedit_window_create_tab (window, TRUE);

	gtk_window_present (GTK_WINDOW (window));
}

static void
preferences_activated (GSimpleAction  *action,
                       GVariant       *parameter,
                       gpointer        user_data)
{
	GtkApplication *app;
	GeditWindow *window;

	app = GTK_APPLICATION (user_data);
	window = GEDIT_WINDOW (gtk_application_get_active_window (app));

	gedit_show_preferences_dialog (window);
}

static void
help_activated (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       user_data)
{
	GtkApplication *app;
	GeditWindow *window;

	app = GTK_APPLICATION (user_data);
	window = GEDIT_WINDOW (gtk_application_get_active_window (app));

	_gedit_cmd_help_contents (NULL, window);
}

static void
about_activated (GSimpleAction  *action,
                 GVariant       *parameter,
                 gpointer        user_data)
{
	GtkApplication *app;
	GeditWindow *window;

	app = GTK_APPLICATION (user_data);
	window = GEDIT_WINDOW (gtk_application_get_active_window (app));

	_gedit_cmd_help_about (NULL, window);
}

static void
quit_activated (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       user_data)
{
	_gedit_cmd_file_quit (NULL, NULL, NULL);
}

static GActionEntry app_entries[] = {
	{ "new-window", new_window_activated, NULL, NULL, NULL },
	{ "preferences", preferences_activated, NULL, NULL, NULL },
	{ "help", help_activated, NULL, NULL, NULL },
	{ "about", about_activated, NULL, NULL, NULL },
	{ "quit", quit_activated, NULL, NULL, NULL }
};

static void
extension_added (PeasExtensionSet *extensions,
		 PeasPluginInfo   *info,
		 PeasExtension    *exten,
		 GeditApp         *app)
{
	gedit_app_activatable_activate (GEDIT_APP_ACTIVATABLE (exten));
}

static void
extension_removed (PeasExtensionSet *extensions,
		   PeasPluginInfo   *info,
		   PeasExtension    *exten,
		   GeditApp         *app)
{
	gedit_app_activatable_deactivate (GEDIT_APP_ACTIVATABLE (exten));
}

static void
gedit_app_startup (GApplication *application)
{
	GeditApp *app = GEDIT_APP (application);
	GtkSourceStyleSchemeManager *manager;
	const gchar *dir;
	gchar *icon_dir;
#ifndef ENABLE_GVFS_METADATA
	const gchar *cache_dir;
	gchar *metadata_filename;
#endif
	GError *error = NULL;
	GFile *css_file;
	GtkCssProvider *provider;

	G_APPLICATION_CLASS (gedit_app_parent_class)->startup (application);

	/* Setup debugging */
	gedit_debug_init ();
	gedit_debug_message (DEBUG_APP, "Startup");

	gedit_dirs_init ();

	/* Setup locale/gettext */
	setlocale (LC_ALL, "");

	dir = gedit_dirs_get_gedit_locale_dir ();
	bindtextdomain (GETTEXT_PACKAGE, dir);

	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gedit_debug_message (DEBUG_APP, "Set icon");

	dir = gedit_dirs_get_gedit_data_dir ();
	icon_dir = g_build_filename (dir, "icons", NULL);

	gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (), icon_dir);
	g_free (icon_dir);

#ifndef ENABLE_GVFS_METADATA
	/* Setup metadata-manager */
	cache_dir = gedit_dirs_get_user_cache_dir ();

	metadata_filename = g_build_filename (cache_dir, METADATA_FILE, NULL);

	gedit_metadata_manager_init (metadata_filename);

	g_free (metadata_filename);
#endif

	/* Load settings */
	app->priv->settings = gedit_settings_new ();
	app->priv->ui_settings = g_settings_new ("org.gnome.gedit.preferences.ui");
	app->priv->window_settings = g_settings_new ("org.gnome.gedit.state.window");

	/* initial lockdown state */
	app->priv->lockdown = gedit_settings_get_lockdown (GEDIT_SETTINGS (app->priv->settings));

	g_action_map_add_action_entries (G_ACTION_MAP (app),
	                                 app_entries,
	                                 G_N_ELEMENTS (app_entries),
	                                 app);

	/* app menu */
	if (_gedit_app_has_app_menu (app))
	{
		GtkBuilder *builder;

		builder = gtk_builder_new ();
		if (!gtk_builder_add_from_resource (builder,
		                                    "/org/gnome/gedit/ui/gedit-menu.ui",
		                                    &error))
		{
			g_warning ("loading menu builder file: %s", error->message);
			g_error_free (error);
		}
		else
		{
			GMenuModel *app_menu;

			app_menu = G_MENU_MODEL (gtk_builder_get_object (builder, "appmenu"));
			gtk_application_set_app_menu (GTK_APPLICATION (application),
			                              app_menu);
		}

		g_object_unref (builder);
	}

	/* Accelerators */
	gtk_application_add_accelerator (GTK_APPLICATION (application),
	                                 "<Primary>Q", "app.quit", NULL);
	gtk_application_add_accelerator (GTK_APPLICATION (application),
	                                 "F1", "app.help", NULL);

	gtk_application_add_accelerator (GTK_APPLICATION (application),
	                                 "<Primary>O", "win.open", NULL);
	gtk_application_add_accelerator (GTK_APPLICATION (application),
	                                 "<Primary>S", "win.save", NULL);
	gtk_application_add_accelerator (GTK_APPLICATION (application),
	                                 "<Primary><Shift>S", "win.save_as", NULL);
	gtk_application_add_accelerator (GTK_APPLICATION (application),
	                                 "<Primary><Shift>L", "win.save_all", NULL);
	gtk_application_add_accelerator (GTK_APPLICATION (application),
	                                 "<Primary>T", "win.new_tab", NULL);
	gtk_application_add_accelerator (GTK_APPLICATION (application),
	                                 "<Primary>W", "win.close", NULL);
	gtk_application_add_accelerator (GTK_APPLICATION (application),
	                                 "<Primary><Shift>W", "win.close_all", NULL);
	gtk_application_add_accelerator (GTK_APPLICATION (application),
	                                 "<Primary>P", "win.print", NULL);
	gtk_application_add_accelerator (GTK_APPLICATION (application),
	                                 "<Primary>F", "win.find", NULL);
	gtk_application_add_accelerator (GTK_APPLICATION (application),
	                                 "<Primary>G", "win.find_next", NULL);
	gtk_application_add_accelerator (GTK_APPLICATION (application),
	                                 "<Primary><Shift>G", "win.find_prev", NULL);
	gtk_application_add_accelerator (GTK_APPLICATION (application),
	                                 "<Primary>H", "win.replace", NULL);
	gtk_application_add_accelerator (GTK_APPLICATION (application),
	                                 "<Primary><Shift>K", "win.clear_highlight", NULL);
	gtk_application_add_accelerator (GTK_APPLICATION (application),
	                                 "<Primary>I", "win.goto_line", NULL);
	gtk_application_add_accelerator (GTK_APPLICATION (application),
	                                 "F9", "win.side_panel", NULL);
	gtk_application_add_accelerator (GTK_APPLICATION (application),
	                                 "<Primary>F9", "win.bottom_panel", NULL);
	gtk_application_add_accelerator (GTK_APPLICATION (application),
	                                 "F11", "win.fullscreen", NULL);
	gtk_application_add_accelerator (GTK_APPLICATION (application),
	                                 "<Primary><Alt>N", "win.new_tab_group", NULL);
	gtk_application_add_accelerator (GTK_APPLICATION (application),
	                                 "<Primary><Shift><Alt>Page_Up",
	                                 "win.previous_tab_group", NULL);
	gtk_application_add_accelerator (GTK_APPLICATION (application),
	                                 "<Primary><Shift><Alt>Page_Down",
	                                 "win.next_tab_group", NULL);
	gtk_application_add_accelerator (GTK_APPLICATION (application),
	                                 "<Primary><Alt>Page_Up",
	                                 "win.previous_document", NULL);
	gtk_application_add_accelerator (GTK_APPLICATION (application),
	                                 "<Primary><Alt>Page_Down",
	                                 "win.next_document", NULL);

	/* Load custom css */
	error = NULL;
	css_file = g_file_new_for_uri ("resource:///org/gnome/gedit/ui/gedit-style.css");
	provider = gtk_css_provider_new ();
	if (gtk_css_provider_load_from_file (provider, css_file, &error))
	{
		gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
		                                           GTK_STYLE_PROVIDER (provider),
		                                           GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	}
	else
	{
		g_warning ("Could not load css provider: %s", error->message);
		g_error_free (error);
	}

	/*
	 * We use the default gtksourceview style scheme manager so that plugins
	 * can obtain it easily without a gedit specific api, but we need to
	 * add our search path at startup before the manager is actually used.
	 */
	manager = gtk_source_style_scheme_manager_get_default ();
	gtk_source_style_scheme_manager_append_search_path (manager,
	                                                    gedit_dirs_get_user_styles_dir ());

	app->priv->engine = gedit_plugins_engine_get_default ();
	app->priv->extensions = peas_extension_set_new (PEAS_ENGINE (app->priv->engine),
	                                                GEDIT_TYPE_APP_ACTIVATABLE,
	                                                "app", app,
	                                                NULL);

	g_signal_connect (app->priv->extensions,
	                  "extension-added",
	                  G_CALLBACK (extension_added),
	                  app);

	g_signal_connect (app->priv->extensions,
	                  "extension-removed",
	                  G_CALLBACK (extension_removed),
	                  app);

	peas_extension_set_foreach (app->priv->extensions,
	                            (PeasExtensionSetForeachFunc) extension_added,
	                            app);
}

static gboolean
is_in_viewport (GtkWindow    *window,
		GdkScreen    *screen,
		gint          workspace,
		gint          viewport_x,
		gint          viewport_y)
{
	GdkScreen *s;
	GdkDisplay *display;
	GdkWindow *gdkwindow;
	const gchar *cur_name;
	const gchar *name;
	gint cur_n;
	gint n;
	gint ws;
	gint sc_width, sc_height;
	gint x, y, width, height;
	gint vp_x, vp_y;

	/* Check for screen and display match */
	display = gdk_screen_get_display (screen);
	cur_name = gdk_display_get_name (display);
	cur_n = gdk_screen_get_number (screen);

	s = gtk_window_get_screen (window);
	display = gdk_screen_get_display (s);
	name = gdk_display_get_name (display);
	n = gdk_screen_get_number (s);

	if (strcmp (cur_name, name) != 0 || cur_n != n)
	{
		return FALSE;
	}

	/* Check for workspace match */
	ws = gedit_utils_get_window_workspace (window);
	if (ws != workspace && ws != GEDIT_ALL_WORKSPACES)
	{
		return FALSE;
	}

	/* Check for viewport match */
	gdkwindow = gtk_widget_get_window (GTK_WIDGET (window));
	gdk_window_get_position (gdkwindow, &x, &y);
	width = gdk_window_get_width (gdkwindow);
	height = gdk_window_get_height (gdkwindow);
	gedit_utils_get_current_viewport (screen, &vp_x, &vp_y);
	x += vp_x;
	y += vp_y;

	sc_width = gdk_screen_get_width (screen);
	sc_height = gdk_screen_get_height (screen);

	return x + width * .25 >= viewport_x &&
	       x + width * .75 <= viewport_x + sc_width &&
	       y >= viewport_y &&
	       y + height <= viewport_y + sc_height;
}

static GeditWindow *
get_active_window (GtkApplication *app)
{
	GdkScreen *screen;
	guint workspace;
	gint viewport_x, viewport_y;
	GList *windows, *l;

	screen = gdk_screen_get_default ();

	workspace = gedit_utils_get_current_workspace (screen);
	gedit_utils_get_current_viewport (screen, &viewport_x, &viewport_y);

	/* Gtk documentation says the window list is always in MRU order */
	windows = gtk_application_get_windows (app);
	for (l = windows; l != NULL; l = l->next)
	{
		GtkWindow *window = l->data;

		if (is_in_viewport (window, screen, workspace, viewport_x, viewport_y))
		{
			return GEDIT_WINDOW (window);
		}
	}

	return NULL;
}

static void
set_command_line_wait (GeditTab *tab)
{
	g_object_set_data_full (G_OBJECT (tab),
	                        "GeditTabCommandLineWait",
	                        g_object_ref (command_line),
	                        (GDestroyNotify)g_object_unref);
}

static void
gedit_app_activate (GApplication *application)
{
	GeditWindow *window = NULL;
	GeditTab *tab;
	gboolean doc_created = FALSE;

	if (!new_window)
	{
		window = get_active_window (GTK_APPLICATION (application));
	}

	if (window == NULL)
	{
		gedit_debug_message (DEBUG_APP, "Create main window");
		window = gedit_app_create_window (GEDIT_APP (application), NULL);

		gedit_debug_message (DEBUG_APP, "Show window");
		gtk_widget_show (GTK_WIDGET (window));
	}

	if (geometry)
	{
		gtk_window_parse_geometry (GTK_WINDOW (window),
		                           geometry);
	}

	if (stdin_stream)
	{
		gedit_debug_message (DEBUG_APP, "Load stdin");

		tab = gedit_window_create_tab_from_stream (window,
		                                           stdin_stream,
		                                           encoding,
		                                           line_position,
		                                           column_position,
		                                           TRUE);
		doc_created = tab != NULL;

		if (doc_created && command_line)
		{
			set_command_line_wait (tab);
		}
		g_input_stream_close (stdin_stream, NULL, NULL);
	}

	if (file_list != NULL)
	{
		GSList *loaded;

		gedit_debug_message (DEBUG_APP, "Load files");
		loaded = _gedit_cmd_load_files_from_prompt (window,
		                                            file_list,
		                                            encoding,
		                                            line_position,
		                                            column_position);

		doc_created = doc_created || loaded != NULL;

		if (command_line)
		{
			g_slist_foreach (loaded, (GFunc)set_command_line_wait, NULL);
		}
		g_slist_free (loaded);
	}

	if (!doc_created || new_document)
	{
		gedit_debug_message (DEBUG_APP, "Create tab");
		tab = gedit_window_create_tab (window, TRUE);

		if (command_line)
		{
			set_command_line_wait (tab);
		}
	}

	gtk_window_present (GTK_WINDOW (window));
}

static GOptionContext *
get_option_context (void)
{
	GOptionContext *context;

	context = g_option_context_new (_("- Edit text files"));
	g_option_context_add_main_entries (context, options, GETTEXT_PACKAGE);
	g_option_context_add_group (context, gtk_get_option_group (FALSE));

#ifdef ENABLE_INTROSPECTION
	g_option_context_add_group (context, g_irepository_get_option_group ());
#endif

	return context;
}

static void
clear_options (void)
{
	g_free (encoding_charset);
	g_strfreev (remaining_args);
	g_free (geometry);
	g_clear_object (&stdin_stream);
	g_slist_free_full (file_list, g_object_unref);

	help = FALSE;
	version = FALSE;
	list_encodings = FALSE;
	encoding_charset = NULL;
	new_window = FALSE;
	new_document = FALSE;
	geometry = NULL;
	wait = FALSE;
	standalone = FALSE;
	remaining_args = NULL;
	encoding = NULL;
	file_list = NULL;
	line_position = 0;
	column_position = 0;
	command_line = NULL;
}

static void
get_line_column_position (const gchar *arg,
                          gint        *line,
                          gint        *column)
{
	gchar **split;

	split = g_strsplit (arg, ":", 2);

	if (split != NULL)
	{
		if (split[0] != NULL)
		{
			*line = atoi (split[0]);
		}

		if (split[1] != NULL)
		{
			*column = atoi (split[1]);
		}
	}

	g_strfreev (split);
}

static gint
gedit_app_command_line (GApplication            *application,
                        GApplicationCommandLine *cl)
{
	gchar **arguments;
	GOptionContext *context;
	GError *error = NULL;

	arguments = g_application_command_line_get_arguments (cl, NULL);

	context = get_option_context ();

	/* Avoid exit() on the main instance */
	g_option_context_set_help_enabled (context, FALSE);

	if (!g_option_context_parse_strv (context, &arguments, &error))
	{
		/* We should never get here since parsing would have
		 * failed on the client side... */
		g_application_command_line_printerr (cl,
		                                     _("%s\nRun '%s --help' to see a full list of available command line options.\n"),
		                                     error->message, arguments[0]);

		g_error_free (error);
		g_application_command_line_set_exit_status (cl, 1);
	}
	else
	{
		if (wait)
		{
			command_line = cl;
		}

		/* Parse encoding */
		if (encoding_charset)
		{
			encoding = gedit_encoding_get_from_charset (encoding_charset);

			if (encoding == NULL)
			{
				g_application_command_line_printerr (cl,
				                                     _("%s: invalid encoding."),
				                                     encoding_charset);
			}

			g_free (encoding_charset);
		}

		/* Parse filenames */
		if (remaining_args)
		{
			gint i;

			for (i = 0; remaining_args[i]; i++)
			{
				if (*remaining_args[i] == '+')
				{
					if (*(remaining_args[i] + 1) == '\0')
					{
						/* goto the last line of the document */
						line_position = G_MAXINT;
						column_position = 0;
					}
					else
					{
						get_line_column_position (remaining_args[i] + 1,
							                  &line_position,
							                  &column_position);
					}
				}
				else if (*remaining_args[i] == '-' && *(remaining_args[i] + 1) == '\0')
				{
					stdin_stream = g_application_command_line_get_stdin (cl);
				}
				else
				{
					GFile *file;

					file = g_application_command_line_create_file_for_arg (cl, remaining_args[i]);
					file_list = g_slist_prepend (file_list, file);
				}
			}

			file_list = g_slist_reverse (file_list);
		}

		g_application_activate (application);
	}

	g_option_context_free (context);
	g_strfreev (arguments);
	clear_options ();

	return 0;
}

static gboolean
gedit_app_local_command_line (GApplication   *application,
                              gchar        ***arguments,
                              gint           *exit_status)
{
	GOptionContext *context;
	GError *error = NULL;
	gboolean ret = FALSE;

	/* Handle some of the option without contacting the main instance */
	context = get_option_context ();

	if (!g_option_context_parse_strv (context, arguments, &error))
	{
		g_printerr (_("%s\nRun '%s --help' to see a full list of available command line options.\n"),
		           error->message, (*arguments)[0]);

		g_error_free (error);
		*exit_status = 1;
		ret = TRUE;
	}
	else if (gapplication_service)
	{
		GApplicationFlags old_flags;

		old_flags = g_application_get_flags (application);
		g_application_set_flags (application, old_flags | G_APPLICATION_IS_SERVICE);
	}
	else if (version)
	{
		g_print ("%s - Version %s\n", g_get_application_name (), VERSION);
		ret = TRUE;
	}
	else if (list_encodings)
	{
		gint i = 0;
		const GeditEncoding *enc;

		while ((enc = gedit_encoding_get_from_index (i)) != NULL)
		{
			g_print ("%s\n", gedit_encoding_get_charset (enc));

			++i;
		}

		ret = TRUE;
	}
	else if (standalone)
	{
		GApplicationFlags old_flags;

		old_flags = g_application_get_flags (application);
		g_application_set_flags (application, old_flags | G_APPLICATION_NON_UNIQUE);
	}

	g_option_context_free (context);
	clear_options ();

	return ret ? ret : G_APPLICATION_CLASS (gedit_app_parent_class)->local_command_line (application, arguments, exit_status);
}

static gboolean
ensure_user_config_dir (void)
{
	const gchar *config_dir;
	gboolean ret = TRUE;
	gint res;

	config_dir = gedit_dirs_get_user_config_dir ();
	if (config_dir == NULL)
	{
		g_warning ("Could not get config directory\n");
		return FALSE;
	}

	res = g_mkdir_with_parents (config_dir, 0755);
	if (res < 0)
	{
		g_warning ("Could not create config directory\n");
		ret = FALSE;
	}

	return ret;
}

static void
save_accels (void)
{
	gchar *filename;

	filename = g_build_filename (gedit_dirs_get_user_config_dir (),
				     "accels",
				     NULL);
	if (filename != NULL)
	{
		gedit_debug_message (DEBUG_APP, "Saving keybindings in %s\n", filename);
		gtk_accel_map_save (filename);
		g_free (filename);
	}
}

static gchar *
get_page_setup_file (void)
{
	const gchar *config_dir;
	gchar *setup = NULL;

	config_dir = gedit_dirs_get_user_config_dir ();

	if (config_dir != NULL)
	{
		setup = g_build_filename (config_dir,
					  GEDIT_PAGE_SETUP_FILE,
					  NULL);
	}

	return setup;
}

static void
save_page_setup (GeditApp *app)
{
	gchar *filename;
	GError *error = NULL;

	if (app->priv->page_setup == NULL)
		return;

	filename = get_page_setup_file ();

	gtk_page_setup_to_file (app->priv->page_setup,
				filename,
				&error);
	if (error)
	{
		g_warning ("%s", error->message);
		g_error_free (error);
	}

	g_free (filename);
}

static gchar *
get_print_settings_file (void)
{
	const gchar *config_dir;
	gchar *settings = NULL;

	config_dir = gedit_dirs_get_user_config_dir ();

	if (config_dir != NULL)
	{
		settings = g_build_filename (config_dir,
					     GEDIT_PRINT_SETTINGS_FILE,
					     NULL);
	}

	return settings;
}

static void
save_print_settings (GeditApp *app)
{
	gchar *filename;
	GError *error = NULL;

	if (app->priv->print_settings == NULL)
		return;

	filename = get_print_settings_file ();

	gtk_print_settings_to_file (app->priv->print_settings,
				    filename,
				    &error);
	if (error)
	{
		g_warning ("%s", error->message);
		g_error_free (error);
	}

	g_free (filename);
}

static void
gedit_app_shutdown (GApplication *app)
{
	gedit_debug_message (DEBUG_APP, "Quitting\n");

	/* Last window is gone... save some settings and exit */
	ensure_user_config_dir ();

	save_accels ();
	save_page_setup (GEDIT_APP (app));
	save_print_settings (GEDIT_APP (app));

#ifndef ENABLE_GVFS_METADATA
	gedit_metadata_manager_shutdown ();
#endif

	gedit_dirs_shutdown ();

	G_APPLICATION_CLASS (gedit_app_parent_class)->shutdown (app);
}

static void
load_accels (void)
{
	gchar *filename;

	filename = g_build_filename (gedit_dirs_get_user_config_dir (),
				     "accels",
				     NULL);
	if (filename != NULL)
	{
		gedit_debug_message (DEBUG_APP, "Loading keybindings from %s\n", filename);
		gtk_accel_map_load (filename);
		g_free (filename);
	}
}

static void
gedit_app_constructed (GObject *object)
{
	G_OBJECT_CLASS (gedit_app_parent_class)->constructed (object);

	load_accels ();
}

static gboolean
window_delete_event (GeditWindow *window,
                     GdkEvent    *event,
                     GeditApp    *app)
{
	GeditWindowState ws;

	ws = gedit_window_get_state (window);

	if (ws &
	    (GEDIT_WINDOW_STATE_SAVING | GEDIT_WINDOW_STATE_PRINTING))
	{
		return TRUE;
	}

	_gedit_cmd_file_quit (NULL, NULL, window);

	/* Do not destroy the window */
	return TRUE;
}

static GeditWindow *
gedit_app_create_window_impl (GeditApp *app)
{
	GeditWindow *window;

	window = g_object_new (GEDIT_TYPE_WINDOW, "application", app, NULL);

	gedit_debug_message (DEBUG_APP, "Window created");

	g_signal_connect (window,
			  "delete_event",
			  G_CALLBACK (window_delete_event),
			  app);

	return window;
}

static void
gedit_app_class_init (GeditAppClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GApplicationClass *app_class = G_APPLICATION_CLASS (klass);

	object_class->dispose = gedit_app_dispose;
	object_class->get_property = gedit_app_get_property;
	object_class->constructed = gedit_app_constructed;

	app_class->startup = gedit_app_startup;
	app_class->activate = gedit_app_activate;
	app_class->command_line = gedit_app_command_line;
	app_class->local_command_line = gedit_app_local_command_line;
	app_class->shutdown = gedit_app_shutdown;

	klass->show_help = gedit_app_show_help_impl;
	klass->help_link_id = gedit_app_help_link_id_impl;
	klass->set_window_title = gedit_app_set_window_title_impl;
	klass->create_window = gedit_app_create_window_impl;

	g_object_class_install_property (object_class,
					 PROP_LOCKDOWN,
					 g_param_spec_flags ("lockdown",
							     "Lockdown",
							     "The lockdown mask",
							     GEDIT_TYPE_LOCKDOWN_MASK,
							     0,
							     G_PARAM_READABLE |
							     G_PARAM_STATIC_STRINGS));
}

static void
load_page_setup (GeditApp *app)
{
	gchar *filename;
	GError *error = NULL;

	g_return_if_fail (app->priv->page_setup == NULL);

	filename = get_page_setup_file ();

	app->priv->page_setup = gtk_page_setup_new_from_file (filename,
							      &error);
	if (error)
	{
		/* Ignore file not found error */
		if (error->domain != G_FILE_ERROR ||
		    error->code != G_FILE_ERROR_NOENT)
		{
			g_warning ("%s", error->message);
		}

		g_error_free (error);
	}

	g_free (filename);

	/* fall back to default settings */
	if (app->priv->page_setup == NULL)
		app->priv->page_setup = gtk_page_setup_new ();
}

static void
load_print_settings (GeditApp *app)
{
	gchar *filename;
	GError *error = NULL;

	g_return_if_fail (app->priv->print_settings == NULL);

	filename = get_print_settings_file ();

	app->priv->print_settings = gtk_print_settings_new_from_file (filename,
								      &error);
	if (error)
	{
		/* Ignore file not found error */
		if (error->domain != G_FILE_ERROR ||
		    error->code != G_FILE_ERROR_NOENT)
		{
			g_warning ("%s", error->message);
		}

		g_error_free (error);
	}

	g_free (filename);

	/* fall back to default settings */
	if (app->priv->print_settings == NULL)
		app->priv->print_settings = gtk_print_settings_new ();
}

static void
gedit_app_init (GeditApp *app)
{
	app->priv = gedit_app_get_instance_private (app);

	g_set_application_name ("gedit");
	gtk_window_set_default_icon_name ("accessories-text-editor");
}

/* Generates a unique string for a window role */
static gchar *
gen_role (void)
{
	GTimeVal result;
	static gint serial;

	g_get_current_time (&result);

	return g_strdup_printf ("gedit-window-%ld-%ld-%d-%s",
				result.tv_sec,
				result.tv_usec,
				serial++,
				g_get_host_name ());
}

/**
 * gedit_app_create_window:
 * @app: the #GeditApp
 * @screen: (allow-none):
 *
 * Create a new #GeditWindow part of @app.
 *
 * Return value: (transfer none): the new #GeditWindow
 */
GeditWindow *
gedit_app_create_window (GeditApp  *app,
			 GdkScreen *screen)
{
	GeditWindow *window;
	gchar *role;
	GdkWindowState state;
	gint w, h;

	gedit_debug (DEBUG_APP);

	window = GEDIT_APP_GET_CLASS (app)->create_window (app);

	if (screen != NULL)
	{
		gtk_window_set_screen (GTK_WINDOW (window), screen);
	}

	role = gen_role ();
	gtk_window_set_role (GTK_WINDOW (window), role);
	g_free (role);

	state = g_settings_get_int (app->priv->window_settings,
				    GEDIT_SETTINGS_WINDOW_STATE);

	g_settings_get (app->priv->window_settings,
			GEDIT_SETTINGS_WINDOW_SIZE,
			"(ii)", &w, &h);

	gtk_window_set_default_size (GTK_WINDOW (window), w, h);

	if ((state & GDK_WINDOW_STATE_MAXIMIZED) != 0)
	{
		gtk_window_maximize (GTK_WINDOW (window));
	}
	else
	{
		gtk_window_unmaximize (GTK_WINDOW (window));
	}

	if ((state & GDK_WINDOW_STATE_STICKY ) != 0)
	{
		gtk_window_stick (GTK_WINDOW (window));
	}
	else
	{
		gtk_window_unstick (GTK_WINDOW (window));
	}

	return window;
}

/**
 * gedit_app_get_documents:
 * @app: the #GeditApp
 *
 * Returns all the documents currently open in #GeditApp.
 *
 * Return value: (element-type Gedit.Document) (transfer container):
 * a newly allocated list of #GeditDocument objects
 */
GList *
gedit_app_get_documents	(GeditApp *app)
{
	GList *res = NULL;
	GList *windows, *l;

	g_return_val_if_fail (GEDIT_IS_APP (app), NULL);

	windows = gtk_application_get_windows (GTK_APPLICATION (app));
	for (l = windows; l != NULL; l = g_list_next (l))
	{
		res = g_list_concat (res,
				     gedit_window_get_documents (GEDIT_WINDOW (l->data)));
	}

	return res;
}

/**
 * gedit_app_get_views:
 * @app: the #GeditApp
 *
 * Returns all the views currently present in #GeditApp.
 *
 * Return value: (element-type Gedit.View) (transfer container):
 * a newly allocated list of #GeditView objects
 */
GList *
gedit_app_get_views (GeditApp *app)
{
	GList *res = NULL;
	GList *windows, *l;

	g_return_val_if_fail (GEDIT_IS_APP (app), NULL);

	windows = gtk_application_get_windows (GTK_APPLICATION (app));
	for (l = windows; l != NULL; l = g_list_next (l))
	{
		res = g_list_concat (res,
				     gedit_window_get_views (GEDIT_WINDOW (l->data)));
	}

	return res;
}

/**
 * gedit_app_get_lockdown:
 * @app: a #GeditApp
 *
 * Gets the lockdown mask (see #GeditLockdownMask) for the application.
 * The lockdown mask determines which functions are locked down using
 * the GNOME-wise lockdown GConf keys.
 **/
GeditLockdownMask
gedit_app_get_lockdown (GeditApp *app)
{
	g_return_val_if_fail (GEDIT_IS_APP (app), GEDIT_LOCKDOWN_ALL);

	return app->priv->lockdown;
}

gboolean
gedit_app_show_help (GeditApp    *app,
                     GtkWindow   *parent,
                     const gchar *name,
                     const gchar *link_id)
{
	g_return_val_if_fail (GEDIT_IS_APP (app), FALSE);
	g_return_val_if_fail (parent == NULL || GTK_IS_WINDOW (parent), FALSE);

	return GEDIT_APP_GET_CLASS (app)->show_help (app, parent, name, link_id);
}

void
gedit_app_set_window_title (GeditApp    *app,
                            GeditWindow *window,
                            const gchar *title)
{
	g_return_if_fail (GEDIT_IS_APP (app));
	g_return_if_fail (GEDIT_IS_WINDOW (window));

	GEDIT_APP_GET_CLASS (app)->set_window_title (app, window, title);
}

gboolean
gedit_app_process_window_event (GeditApp    *app,
                                GeditWindow *window,
                                GdkEvent    *event)
{
	g_return_val_if_fail (GEDIT_IS_APP (app), FALSE);
	g_return_val_if_fail (GEDIT_IS_WINDOW (window), FALSE);

	if (GEDIT_APP_GET_CLASS (app)->process_window_event)
	{
		return GEDIT_APP_GET_CLASS (app)->process_window_event (app, window, event);
	}

    return FALSE;
}

static void
app_lockdown_changed (GeditApp *app)
{
	GList *windows, *l;

	windows = gtk_application_get_windows (GTK_APPLICATION (app));
	for (l = windows; l != NULL; l = g_list_next (l))
	{
		_gedit_window_set_lockdown (GEDIT_WINDOW (l->data),
		                            app->priv->lockdown);
	}

	g_object_notify (G_OBJECT (app), "lockdown");
}

void
_gedit_app_set_lockdown (GeditApp          *app,
			 GeditLockdownMask  lockdown)
{
	g_return_if_fail (GEDIT_IS_APP (app));

	app->priv->lockdown = lockdown;

	app_lockdown_changed (app);
}

void
_gedit_app_set_lockdown_bit (GeditApp          *app,
			     GeditLockdownMask  bit,
			     gboolean           value)
{
	g_return_if_fail (GEDIT_IS_APP (app));

	if (value)
		app->priv->lockdown |= bit;
	else
		app->priv->lockdown &= ~bit;

	app_lockdown_changed (app);
}

/* Returns a copy */
GtkPageSetup *
_gedit_app_get_default_page_setup (GeditApp *app)
{
	g_return_val_if_fail (GEDIT_IS_APP (app), NULL);

	if (app->priv->page_setup == NULL)
		load_page_setup (app);

	return gtk_page_setup_copy (app->priv->page_setup);
}

void
_gedit_app_set_default_page_setup (GeditApp     *app,
				   GtkPageSetup *page_setup)
{
	g_return_if_fail (GEDIT_IS_APP (app));
	g_return_if_fail (GTK_IS_PAGE_SETUP (page_setup));

	if (app->priv->page_setup != NULL)
		g_object_unref (app->priv->page_setup);

	app->priv->page_setup = g_object_ref (page_setup);
}

/* Returns a copy */
GtkPrintSettings *
_gedit_app_get_default_print_settings (GeditApp *app)
{
	g_return_val_if_fail (GEDIT_IS_APP (app), NULL);

	if (app->priv->print_settings == NULL)
		load_print_settings (app);

	return gtk_print_settings_copy (app->priv->print_settings);
}

void
_gedit_app_set_default_print_settings (GeditApp         *app,
				       GtkPrintSettings *settings)
{
	g_return_if_fail (GEDIT_IS_APP (app));
	g_return_if_fail (GTK_IS_PRINT_SETTINGS (settings));

	if (app->priv->print_settings != NULL)
		g_object_unref (app->priv->print_settings);

	app->priv->print_settings = g_object_ref (settings);
}

GObject *
_gedit_app_get_settings (GeditApp *app)
{
	g_return_val_if_fail (GEDIT_IS_APP (app), NULL);

	return app->priv->settings;
}

gboolean
_gedit_app_has_app_menu (GeditApp *app)
{
	GtkSettings *gtk_settings;
	gboolean show_app_menu;
	gboolean show_menubar;

	g_return_val_if_fail (GEDIT_IS_APP (app), FALSE);

	/* We have three cases:
	 * - GNOME 3: show-app-menu true, show-menubar false -> use the app menu
	 * - Unity, OSX: show-app-menu and show-menubar true -> use the normal menu
	 * - Other WM, Windows: show-app-menu and show-menubar false -> use the normal menu
	 */
	gtk_settings = gtk_settings_get_default ();
	g_object_get (G_OBJECT (gtk_settings),
	              "gtk-shell-shows-app-menu", &show_app_menu,
	              "gtk-shell-shows-menubar", &show_menubar,
	              NULL);

	return show_app_menu && !show_menubar;
}

/* ex:set ts=8 noet: */
