/*
 * gedit-settings.c
 * This file is part of gedit
 *
 * Copyright (C) 2002-2005 - Paolo Maggi
 *               2009 - Ignacio Casal Quinteiro
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
 * along with gedit; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <gtksourceview/gtksource.h>

#include "gedit-settings.h"
#include "gedit-app.h"
#include "gedit-debug.h"
#include "gedit-view.h"
#include "gedit-window.h"
#include "gedit-plugins-engine.h"
#include "gedit-dirs.h"
#include "gedit-utils.h"
#include "gedit-window-private.h"

#define GEDIT_SETTINGS_LOCKDOWN_COMMAND_LINE "disable-command-line"
#define GEDIT_SETTINGS_LOCKDOWN_PRINTING "disable-printing"
#define GEDIT_SETTINGS_LOCKDOWN_PRINT_SETUP "disable-print-setup"
#define GEDIT_SETTINGS_LOCKDOWN_SAVE_TO_DISK "disable-save-to-disk"

#define GEDIT_SETTINGS_SYSTEM_FONT "monospace-font-name"

struct _GeditSettingsPrivate
{
	GSettings *lockdown;
	GSettings *interface;
	GSettings *editor;
	GSettings *ui;

	gchar *old_scheme;
};

G_DEFINE_TYPE_WITH_PRIVATE (GeditSettings, gedit_settings, G_TYPE_OBJECT)

static void
gedit_settings_finalize (GObject *object)
{
	GeditSettings *gs = GEDIT_SETTINGS (object);

	g_free (gs->priv->old_scheme);

	G_OBJECT_CLASS (gedit_settings_parent_class)->finalize (object);
}

static void
gedit_settings_dispose (GObject *object)
{
	GeditSettingsPrivate *priv = GEDIT_SETTINGS (object)->priv;

	g_clear_object (&priv->lockdown);
	g_clear_object (&priv->interface);
	g_clear_object (&priv->editor);
	g_clear_object (&priv->ui);

	G_OBJECT_CLASS (gedit_settings_parent_class)->dispose (object);
}

static void
on_lockdown_changed (GSettings   *settings,
		     const gchar *key,
		     gpointer     useless)
{
	gboolean locked;
	GeditApp *app;

	locked = g_settings_get_boolean (settings, key);
	app = GEDIT_APP (g_application_get_default ());

	if (strcmp (key, GEDIT_SETTINGS_LOCKDOWN_COMMAND_LINE) == 0)
	{
		_gedit_app_set_lockdown_bit (app,
					     GEDIT_LOCKDOWN_COMMAND_LINE,
					     locked);
	}
	else if (strcmp (key, GEDIT_SETTINGS_LOCKDOWN_PRINTING) == 0)
	{
		_gedit_app_set_lockdown_bit (app,
					     GEDIT_LOCKDOWN_PRINTING,
					     locked);
	}
	else if (strcmp (key, GEDIT_SETTINGS_LOCKDOWN_PRINT_SETUP) == 0)
	{
		_gedit_app_set_lockdown_bit (app,
					     GEDIT_LOCKDOWN_PRINT_SETUP,
					     locked);
	}
	else if (strcmp (key, GEDIT_SETTINGS_LOCKDOWN_SAVE_TO_DISK) == 0)
	{
		_gedit_app_set_lockdown_bit (app,
					     GEDIT_LOCKDOWN_SAVE_TO_DISK,
					     locked);
	}
}

static void
set_font (GeditSettings *gs,
	  const gchar *font)
{
	GList *views, *l;
	guint ts;

	g_settings_get (gs->priv->editor, GEDIT_SETTINGS_TABS_SIZE, "u", &ts);

	views = gedit_app_get_views (GEDIT_APP (g_application_get_default ()));

	for (l = views; l != NULL; l = g_list_next (l))
	{
		/* Note: we use def=FALSE to avoid GeditView to query dconf */
		gedit_view_set_font (GEDIT_VIEW (l->data), FALSE, font);

		gtk_source_view_set_tab_width (GTK_SOURCE_VIEW (l->data), ts);
	}

	g_list_free (views);
}

static void
on_system_font_changed (GSettings     *settings,
			const gchar   *key,
			GeditSettings *gs)
{

	gboolean use_default_font;
	gchar *font;

	use_default_font = g_settings_get_boolean (gs->priv->editor,
						   GEDIT_SETTINGS_USE_DEFAULT_FONT);
	if (!use_default_font)
		return;

	font = g_settings_get_string (settings, key);

	set_font (gs, font);

	g_free (font);
}

static void
on_use_default_font_changed (GSettings     *settings,
			     const gchar   *key,
			     GeditSettings *gs)
{
	gboolean def;
	gchar *font;

	def = g_settings_get_boolean (settings, key);

	if (def)
	{
		font = g_settings_get_string (gs->priv->interface,
					      GEDIT_SETTINGS_SYSTEM_FONT);
	}
	else
	{
		font = g_settings_get_string (gs->priv->editor,
					      GEDIT_SETTINGS_EDITOR_FONT);
	}

	set_font (gs, font);

	g_free (font);
}

static void
on_editor_font_changed (GSettings     *settings,
			const gchar   *key,
			GeditSettings *gs)
{
	gboolean use_default_font;
	gchar *font;

	use_default_font = g_settings_get_boolean (gs->priv->editor,
						   GEDIT_SETTINGS_USE_DEFAULT_FONT);
	if (use_default_font)
		return;

	font = g_settings_get_string (settings, key);

	set_font (gs, font);

	g_free (font);
}

static void
on_scheme_changed (GSettings     *settings,
		   const gchar   *key,
		   GeditSettings *gs)
{
	GtkSourceStyleSchemeManager *manager;
	GtkSourceStyleScheme *style;
	gchar *scheme;
	GList *docs;
	GList *l;

	scheme = g_settings_get_string (settings, key);

	if (gs->priv->old_scheme != NULL &&
	    (strcmp (scheme, gs->priv->old_scheme) == 0))
	{
		g_free (scheme);
		return;
	}

	g_free (gs->priv->old_scheme);
	gs->priv->old_scheme = scheme;

	manager = gtk_source_style_scheme_manager_get_default ();
	style = gtk_source_style_scheme_manager_get_scheme (manager, scheme);
	if (style == NULL)
	{
		g_warning ("Default style scheme '%s' not found, falling back to 'classic'", scheme);

		style = gtk_source_style_scheme_manager_get_scheme (manager, "classic");
		if (style == NULL)
		{
			g_warning ("Style scheme 'classic' cannot be found, check your GtkSourceView installation.");
			return;
		}
	}

	docs = gedit_app_get_documents (GEDIT_APP (g_application_get_default ()));

	for (l = docs; l != NULL; l = g_list_next (l))
	{
		g_return_if_fail (GTK_SOURCE_IS_BUFFER (l->data));

		gtk_source_buffer_set_style_scheme (GTK_SOURCE_BUFFER (l->data),
						    style);
	}

	g_list_free (docs);
}

static void
on_auto_save_changed (GSettings     *settings,
		      const gchar   *key,
		      GeditSettings *gs)
{
	GList *docs, *l;
	gboolean auto_save;

	auto_save = g_settings_get_boolean (settings, key);

	docs = gedit_app_get_documents (GEDIT_APP (g_application_get_default ()));

	for (l = docs; l != NULL; l = g_list_next (l))
	{
		GeditTab *tab = gedit_tab_get_from_document (GEDIT_DOCUMENT (l->data));

		gedit_tab_set_auto_save_enabled (tab, auto_save);
	}

	g_list_free (docs);
}

static void
on_auto_save_interval_changed (GSettings     *settings,
			       const gchar   *key,
			       GeditSettings *gs)
{
	GList *docs, *l;
	gint auto_save_interval;

	g_settings_get (settings, key, "u", &auto_save_interval);

	docs = gedit_app_get_documents (GEDIT_APP (g_application_get_default ()));

	for (l = docs; l != NULL; l = g_list_next (l))
	{
		GeditTab *tab = gedit_tab_get_from_document (GEDIT_DOCUMENT (l->data));

		gedit_tab_set_auto_save_interval (tab, auto_save_interval);
	}

	g_list_free (docs);
}

static void
on_syntax_highlighting_changed (GSettings     *settings,
				const gchar   *key,
				GeditSettings *gs)
{
	const GList *windows;
	GList *docs, *l;
	gboolean enable;

	enable = g_settings_get_boolean (settings, key);

	docs = gedit_app_get_documents (GEDIT_APP (g_application_get_default ()));

	for (l = docs; l != NULL; l = g_list_next (l))
	{
		gtk_source_buffer_set_highlight_syntax (GTK_SOURCE_BUFFER (l->data),
							enable);
	}

	g_list_free (docs);

	/* update the sensitivity of the Higlight Mode menu item */
	windows = gtk_application_get_windows (GTK_APPLICATION (g_application_get_default ()));
	while (windows != NULL)
	{
		GtkUIManager *ui;
		GtkAction *a;

		ui = gedit_window_get_ui_manager (GEDIT_WINDOW (windows->data));

		a = gtk_ui_manager_get_action (ui,
					       "/MenuBar/ViewMenu/ViewHighlightModeMenu");

		gtk_action_set_sensitive (a, enable);

		windows = g_list_next (windows);
	}
}

static void
gedit_settings_init (GeditSettings *gs)
{
	gs->priv = gedit_settings_get_instance_private (gs);

	gs->priv->old_scheme = NULL;
	gs->priv->editor = g_settings_new ("org.gnome.gedit.preferences.editor");
	gs->priv->ui = g_settings_new ("org.gnome.gedit.preferences.ui");

	/* Load settings */
	gs->priv->lockdown = g_settings_new ("org.gnome.desktop.lockdown");

	g_signal_connect (gs->priv->lockdown,
			  "changed",
			  G_CALLBACK (on_lockdown_changed),
			  NULL);

	gs->priv->interface = g_settings_new ("org.gnome.desktop.interface");

	g_signal_connect (gs->priv->interface,
			  "changed::monospace-font-name",
			  G_CALLBACK (on_system_font_changed),
			  gs);

	/* editor changes */
	g_signal_connect (gs->priv->editor,
			  "changed::use-default-font",
			  G_CALLBACK (on_use_default_font_changed),
			  gs);
	g_signal_connect (gs->priv->editor,
			  "changed::editor-font",
			  G_CALLBACK (on_editor_font_changed),
			  gs);
	g_signal_connect (gs->priv->editor,
			  "changed::scheme",
			  G_CALLBACK (on_scheme_changed),
			  gs);
	g_signal_connect (gs->priv->editor,
			  "changed::auto-save",
			  G_CALLBACK (on_auto_save_changed),
			  gs);
	g_signal_connect (gs->priv->editor,
			  "changed::auto-save-interval",
			  G_CALLBACK (on_auto_save_interval_changed),
			  gs);
	g_signal_connect (gs->priv->editor,
			  "changed::syntax-highlighting",
			  G_CALLBACK (on_syntax_highlighting_changed),
			  gs);
}

static void
gedit_settings_class_init (GeditSettingsClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = gedit_settings_finalize;
	object_class->dispose = gedit_settings_dispose;
}

GObject *
gedit_settings_new ()
{
	return g_object_new (GEDIT_TYPE_SETTINGS, NULL);
}

GeditLockdownMask
gedit_settings_get_lockdown (GeditSettings *gs)
{
	guint lockdown = 0;
	gboolean command_line, printing, print_setup, save_to_disk;

	command_line = g_settings_get_boolean (gs->priv->lockdown,
					       GEDIT_SETTINGS_LOCKDOWN_COMMAND_LINE);
	printing = g_settings_get_boolean (gs->priv->lockdown,
					   GEDIT_SETTINGS_LOCKDOWN_PRINTING);
	print_setup = g_settings_get_boolean (gs->priv->lockdown,
					      GEDIT_SETTINGS_LOCKDOWN_PRINT_SETUP);
	save_to_disk = g_settings_get_boolean (gs->priv->lockdown,
					       GEDIT_SETTINGS_LOCKDOWN_SAVE_TO_DISK);

	if (command_line)
		lockdown |= GEDIT_LOCKDOWN_COMMAND_LINE;

	if (printing)
		lockdown |= GEDIT_LOCKDOWN_PRINTING;

	if (print_setup)
		lockdown |= GEDIT_LOCKDOWN_PRINT_SETUP;

	if (save_to_disk)
		lockdown |= GEDIT_LOCKDOWN_SAVE_TO_DISK;

	return lockdown;
}

gchar *
gedit_settings_get_system_font (GeditSettings *gs)
{
	gchar *system_font;

	g_return_val_if_fail (GEDIT_IS_SETTINGS (gs), NULL);

	system_font = g_settings_get_string (gs->priv->interface,
					     "monospace-font-name");

	return system_font;
}

GSList *
gedit_settings_get_list (GSettings   *settings,
			 const gchar *key)
{
	GSList *list = NULL;
	gchar **values;
	gsize i;

	g_return_val_if_fail (G_IS_SETTINGS (settings), NULL);
	g_return_val_if_fail (key != NULL, NULL);

	values = g_settings_get_strv (settings, key);
	i = 0;

	while (values[i] != NULL)
	{
		list = g_slist_prepend (list, values[i]);
		i++;
	}

	g_free (values);

	return g_slist_reverse (list);
}

void
gedit_settings_set_list (GSettings    *settings,
			 const gchar  *key,
			 const GSList *list)
{
	gchar **values = NULL;
	const GSList *l;

	g_return_if_fail (G_IS_SETTINGS (settings));
	g_return_if_fail (key != NULL);

	if (list != NULL)
	{
		gint i, len;

		len = g_slist_length ((GSList *)list);
		values = g_new (gchar *, len + 1);

		for (l = list, i = 0; l != NULL; l = g_slist_next (l), i++)
		{
			values[i] = l->data;
		}
		values[i] = NULL;
	}

	g_settings_set_strv (settings, key, (const gchar * const *)values);
	g_free (values);
}

/* ex:set ts=8 noet: */
