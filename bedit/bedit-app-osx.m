/*
 * bedit-app-osx.c
 * This file is part of bedit
 *
 * Copyright (C) 2010 - Jesse van den Kieboom
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
 * along with bedit; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#include "bedit-app-osx.h"

#include <gdk/gdkquartz.h>
#include <string.h>
#include <glib/gi18n.h>

#include "bedit-app-private.h"
#include "bedit-dirs.h"
#include "bedit-debug.h"
#include "bedit-commands.h"
#include "bedit-commands-private.h"
#include "bedit-recent.h"

static BeditWindow *
ensure_window (BeditAppOSX *app,
               gboolean     with_empty_document)
{
	GList *windows;
	BeditWindow *ret = NULL;

	windows = gtk_application_get_windows (GTK_APPLICATION (app));

	while (windows)
	{
		GtkWindow *window;
		GdkWindow *win;
		NSWindow *nswin;

		window = windows->data;
		windows = g_list_next (windows);

		if (!gtk_widget_get_realized (GTK_WIDGET (window)))
		{
			continue;
		}

		if (!GEDIT_IS_WINDOW (window))
		{
			continue;
		}

		win = gtk_widget_get_window (GTK_WIDGET (window));
		nswin = gdk_quartz_window_get_nswindow (win);

		if ([nswin isOnActiveSpace])
		{
			ret = GEDIT_WINDOW (window);
			break;
		}
	}

	if (!ret)
	{
		ret = bedit_app_create_window (GEDIT_APP (app), NULL);
		gtk_widget_show (GTK_WIDGET (ret));
	}

	if (with_empty_document && bedit_window_get_active_document (ret) == NULL)
	{
		bedit_window_create_tab (ret, TRUE);
	}

	gtk_window_present (GTK_WINDOW (ret));
	return ret;
}

@interface BeditAppOSXDelegate : NSObject
{
	BeditAppOSX *app;
	id<NSApplicationDelegate> orig;
}

- (id)initWithApp:(BeditAppOSX *)theApp;
- (void)release;

- (id)forwardingTargetForSelector:(SEL)aSelector;
- (BOOL)respondsToSelector:(SEL)aSelector;

- (BOOL)applicationShouldHandleReopen:(NSApplication *)theApplication hasVisibleWindows:(BOOL)flag;
- (void)applicationWillBecomeActive:(NSNotification *)aNotification;
- (void)application:(NSApplication *)sender openFiles:(NSArray *)filenames;

@end

@implementation BeditAppOSXDelegate
- (id)initWithApp:(BeditAppOSX *)theApp
{
	[super init];
	app = theApp;

	orig = [NSApp delegate];
	[NSApp setDelegate:self];

	return self;
}

- (void)release
{
	[NSApp setDelegate:orig];
	[super release];
}

- (id)forwardingTargetForSelector:(SEL)aSelector
{
	return orig;
}

- (BOOL)respondsToSelector:(SEL)aSelector
{
    return [super respondsToSelector:aSelector] || [orig respondsToSelector:aSelector];
}

- (BOOL)applicationShouldHandleReopen:(NSApplication *)theApplication hasVisibleWindows:(BOOL)flag
{
	ensure_window (app, TRUE);
	return NO;
}

- (void)applicationWillBecomeActive:(NSNotification *)aNotification
{
	ensure_window (app, TRUE);
}

- (void)application:(NSApplication *)sender openFiles:(NSArray *)filenames
{
	ensure_window (app, FALSE);
	[orig application:sender openFiles:filenames];
}

@end

struct _BeditAppOSX
{
	BeditApp parent_instance;

	BeditMenuExtension *recent_files_menu;
	gulong recent_manager_changed_id;

	BeditAppOSXDelegate *app_delegate;

	GList *recent_actions;
	BeditRecentConfiguration recent_config;
};

G_DEFINE_TYPE (BeditAppOSX, bedit_app_osx, GEDIT_TYPE_APP)

static void
remove_recent_actions (BeditAppOSX *app)
{
	while (app->recent_actions)
	{
		gchar *action_name = app->recent_actions->data;

		g_action_map_remove_action (G_ACTION_MAP (app), action_name);
		g_free (action_name);

		app->recent_actions = g_list_delete_link (app->recent_actions,
		                                          app->recent_actions);
	}
}

static void
bedit_app_osx_finalize (GObject *object)
{
	BeditAppOSX *app = GEDIT_APP_OSX (object);

	g_object_unref (app->recent_files_menu);

	remove_recent_actions (app);

	g_signal_handler_disconnect (app->recent_config.manager,
	                             app->recent_manager_changed_id);

	bedit_recent_configuration_destroy (&app->recent_config);

	[app->app_delegate release];

	G_OBJECT_CLASS (bedit_app_osx_parent_class)->finalize (object);
}

gboolean
bedit_app_osx_show_url (BeditAppOSX *app,
                        const gchar *url)
{
	return [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:[NSString stringWithUTF8String:url]]];
}

static gboolean
bedit_app_osx_show_help_impl (BeditApp    *app,
                              GtkWindow   *parent,
                              const gchar *name,
                              const gchar *link_id)
{
	gboolean ret = FALSE;

	if (name == NULL || g_strcmp0 (name, "bedit") == 0)
	{
		gchar *link;

		if (link_id)
		{
			link = g_strdup_printf ("http://library.gnome.org/users/bedit/stable/%s",
						link_id);
		}
		else
		{
			link = g_strdup ("http://library.gnome.org/users/bedit/stable/");
		}

		ret = bedit_app_osx_show_url (GEDIT_APP_OSX (app), link);
		g_free (link);
	}

	return ret;
}

static void
bedit_app_osx_set_window_title_impl (BeditApp    *app,
                                     BeditWindow *window,
                                     const gchar *title)
{
	NSWindow *native;
	BeditDocument *document;
	GdkWindow *wnd;

	g_return_if_fail (GEDIT_IS_WINDOW (window));

	wnd = gtk_widget_get_window (GTK_WIDGET (window));

	if (wnd == NULL)
	{
		return;
	}

	native = gdk_quartz_window_get_nswindow (wnd);
	document = bedit_window_get_active_document (window);

	if (document)
	{
		bool ismodified;

		if (bedit_document_is_untitled (document))
		{
			[native setRepresentedURL:nil];
		}
		else
		{
			GtkSourceFile *file;
			GFile *location;
			gchar *uri;

			file = bedit_document_get_file (document);
			location = gtk_source_file_get_location (file);

			uri = g_file_get_uri (location);

			NSURL *nsurl = [NSURL URLWithString:[NSString stringWithUTF8String:uri]];

			[native setRepresentedURL:nsurl];
			g_free (uri);
		}

		ismodified = !bedit_document_is_untouched (document);
		[native setDocumentEdited:ismodified];
	}
	else
	{
		[native setRepresentedURL:nil];
		[native setDocumentEdited:false];
	}

	GEDIT_APP_CLASS (bedit_app_osx_parent_class)->set_window_title (app, window, title);
}

typedef struct
{
	BeditAppOSX   *app;
	GtkRecentInfo *info;
} RecentFileInfo;

static void
recent_file_info_free (gpointer  data,
                       GClosure *closure)
{
	RecentFileInfo *info = data;

	g_object_unref (info->app);
	gtk_recent_info_unref (info->info);

	g_slice_free (RecentFileInfo, data);
}

static void
recent_file_activated (GAction        *action,
                       GVariant       *parameter,
                       RecentFileInfo *info)
{
	BeditWindow *window;
	const gchar *uri;
	GFile *file;

	uri = gtk_recent_info_get_uri (info->info);
	file = g_file_new_for_uri (uri);

	window = ensure_window (info->app, FALSE);

	bedit_commands_load_location (GEDIT_WINDOW (window), file, NULL, 0, 0);
	g_object_unref (file);
}

static void
recent_files_menu_populate (BeditAppOSX *app)
{
	GList *items;
	gint i = 0;

	bedit_menu_extension_remove_items (app->recent_files_menu);
	remove_recent_actions (app);

	items = bedit_recent_get_items (&app->recent_config);

	while (items)
	{
		GtkRecentInfo *info = items->data;
		GMenuItem *mitem;
		const gchar *name;
		gchar *acname;
		gchar *acfullname;
		GSimpleAction *action;
		RecentFileInfo *finfo;

		name = gtk_recent_info_get_display_name (info);

		acname = g_strdup_printf ("recent-file-action-%d", ++i);
		action = g_simple_action_new (acname, NULL);

		finfo = g_slice_new (RecentFileInfo);
		finfo->app = g_object_ref (app);
		finfo->info = gtk_recent_info_ref (info);

		g_signal_connect_data (action,
		                       "activate",
		                       G_CALLBACK (recent_file_activated),
		                       finfo,
		                       recent_file_info_free,
		                       0);

		g_action_map_add_action (G_ACTION_MAP (app), G_ACTION (action));
		g_object_unref (action);

		acfullname = g_strdup_printf ("app.%s", acname);

		app->recent_actions = g_list_prepend (app->recent_actions, acname);

		mitem = g_menu_item_new (name, acfullname);
		bedit_menu_extension_append_menu_item (app->recent_files_menu, mitem);

		g_free (acfullname);

		g_object_unref (mitem);
		gtk_recent_info_unref (info);

		items = g_list_delete_link (items, items);
	}
}

static void
recent_manager_changed (GtkRecentManager *manager,
                        BeditAppOSX      *app)
{
	recent_files_menu_populate (app);
}

static void
open_activated (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       userdata)
{
	_bedit_cmd_file_open (NULL, NULL, NULL);
}

static GActionEntry app_entries[] = {
	{ "open", open_activated, NULL, NULL, NULL }
};

static void
update_open_sensitivity (BeditAppOSX *app)
{
	GAction *action;
	gboolean has_windows;

	has_windows = (gtk_application_get_windows (GTK_APPLICATION (app)) != NULL);

	action = g_action_map_lookup_action (G_ACTION_MAP (app), "open");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), !has_windows);
}

static void
bedit_app_osx_startup (GApplication *application)
{
	const gchar *replace_accels[] = {
		"<Primary><Alt>F",
		NULL
	};

	const gchar *open_accels[] = {
		"<Primary>O",
		NULL
	};

	const gchar *fullscreen_accels[] = {
		"<Primary><Control>F",
		NULL
	};

	BeditAppOSX *app = GEDIT_APP_OSX (application);

	G_APPLICATION_CLASS (bedit_app_osx_parent_class)->startup (application);

	app->app_delegate = [[[BeditAppOSXDelegate alloc] initWithApp:app] retain];

	g_action_map_add_action_entries (G_ACTION_MAP (application),
	                                 app_entries,
	                                 G_N_ELEMENTS (app_entries),
	                                 application);

	gtk_application_set_accels_for_action (GTK_APPLICATION (application),
	                                       "win.replace",
	                                       replace_accels);

	gtk_application_set_accels_for_action (GTK_APPLICATION (application),
	                                       "app.open",
	                                       open_accels);

	gtk_application_set_accels_for_action (GTK_APPLICATION (application),
	                                       "win.fullscreen",
	                                       fullscreen_accels);

	bedit_recent_configuration_init_default (&app->recent_config);

	app->recent_files_menu = _bedit_app_extend_menu (GEDIT_APP (application),
	                                                 "recent-files-section");

	app->recent_manager_changed_id = g_signal_connect (app->recent_config.manager,
	                                                   "changed",
	                                                   G_CALLBACK (recent_manager_changed),
	                                                   app);

	recent_files_menu_populate (app);

	g_application_hold (application);
	update_open_sensitivity (app);
}

static void
set_window_allow_fullscreen (BeditWindow *window)
{
	GdkWindow *wnd;
	NSWindow *native;

	wnd = gtk_widget_get_window (GTK_WIDGET (window));

	if (wnd != NULL)
	{
		native = gdk_quartz_window_get_nswindow (wnd);
		[native setCollectionBehavior: [native collectionBehavior] | NSWindowCollectionBehaviorFullScreenPrimary];
	}
}

static void
on_window_realized (GtkWidget *widget)
{
	set_window_allow_fullscreen (GEDIT_WINDOW (widget));
}

static BeditWindow *
bedit_app_osx_create_window_impl (BeditApp *app)
{
	BeditWindow *window;

	window = GEDIT_APP_CLASS (bedit_app_osx_parent_class)->create_window (app);

	gtk_window_set_titlebar (GTK_WINDOW (window), NULL);

	if (gtk_widget_get_realized (GTK_WIDGET (window)))
	{
		set_window_allow_fullscreen (window);
	}
	else
	{
		g_signal_connect (window, "realize", G_CALLBACK (on_window_realized), NULL);
	}

	return window;
}

static gboolean
bedit_app_osx_process_window_event_impl (BeditApp    *app,
                                         BeditWindow *window,
                                         GdkEvent    *event)
{
	NSEvent *nsevent;

	/* For OS X we will propagate the event to NSApp, which handles some OS X
	* specific keybindings and the accelerators for the menu
	*/
	nsevent = gdk_quartz_event_get_nsevent (event);
	[NSApp sendEvent:nsevent];

	/* It does not really matter what we return here since it's the last thing
	* in the chain. Also we can't get from sendEvent whether the event was
	* actually handled by NSApp anyway
	*/
	return TRUE;
}

static void
bedit_app_osx_constructed (GObject *object)
{
	/* FIXME: should we do this on all platforms? */
	g_object_set (object, "register-session", TRUE, NULL);
	G_OBJECT_CLASS (bedit_app_osx_parent_class)->constructed (object);
}

static void
bedit_app_osx_window_added (GtkApplication *application,
                            GtkWindow      *window)
{
	GTK_APPLICATION_CLASS (bedit_app_osx_parent_class)->window_added (application, window);

	update_open_sensitivity (GEDIT_APP_OSX (application));
}

static void
bedit_app_osx_window_removed (GtkApplication *application,
                              GtkWindow      *window)
{
	GTK_APPLICATION_CLASS (bedit_app_osx_parent_class)->window_removed (application, window);

	update_open_sensitivity (GEDIT_APP_OSX (application));
}

static void
bedit_app_osx_class_init (BeditAppOSXClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	BeditAppClass *app_class = GEDIT_APP_CLASS (klass);
	GApplicationClass *application_class = G_APPLICATION_CLASS (klass);
	GtkApplicationClass *gtkapplication_class = GTK_APPLICATION_CLASS (klass);

	object_class->finalize = bedit_app_osx_finalize;
	object_class->constructed = bedit_app_osx_constructed;

	application_class->startup = bedit_app_osx_startup;

	gtkapplication_class->window_added = bedit_app_osx_window_added;
	gtkapplication_class->window_removed = bedit_app_osx_window_removed;

	app_class->show_help = bedit_app_osx_show_help_impl;
	app_class->set_window_title = bedit_app_osx_set_window_title_impl;
	app_class->create_window = bedit_app_osx_create_window_impl;
	app_class->process_window_event = bedit_app_osx_process_window_event_impl;
}

static void
bedit_app_osx_init (BeditAppOSX *app)
{
	/* This is required so that Cocoa is not going to parse the
	   command line arguments by itself and generate OpenFile events.
	   We already parse the command line ourselves, so this is needed
	   to prevent opening files twice, etc. */
	[[NSUserDefaults standardUserDefaults] setObject:@"NO"
	                                       forKey:@"NSTreatUnknownArgumentsAsOpen"];
}

/* ex:set ts=8 noet: */
