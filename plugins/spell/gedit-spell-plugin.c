/*
 * gedit-spell-plugin.c
 *
 * Copyright (C) 2002-2005 Paolo Maggi
 * Copyright (C) 2015 Sébastien Wilmet
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

#include "gedit-spell-plugin.h"

#include <glib/gi18n.h>
#include <gedit/gedit-debug.h>
#include <gedit/gedit-statusbar.h>
#include <gedit/gedit-app.h>
#include <gedit/gedit-window.h>
#include <gedit/gedit-window-activatable.h>
#include <gtksourceview/gtksource.h>

#include "gedit-automatic-spell-checker.h"
#include "gedit-spell-app-activatable.h"
#include "gedit-spell-checker.h"
#include "gedit-spell-checker-dialog.h"
#include "gedit-spell-language-dialog.h"
#include "gedit-spell-navigator-gtv.h"
#include "gedit-spell-utils.h"

#ifdef G_OS_WIN32
#define GEDIT_METADATA_ATTRIBUTE_SPELL_LANGUAGE "spell-language"
#define GEDIT_METADATA_ATTRIBUTE_SPELL_ENABLED  "spell-enabled"
#else
#define GEDIT_METADATA_ATTRIBUTE_SPELL_LANGUAGE "metadata::gedit-spell-language"
#define GEDIT_METADATA_ATTRIBUTE_SPELL_ENABLED  "metadata::gedit-spell-enabled"
#endif

#define SPELL_ENABLED_STR "1"

#define VIEW_DATA_KEY "GeditSpellPlugin-ViewData"
#define NAVIGATOR_KEY "GeditSpellPlugin-Navigator"

static void gedit_window_activatable_iface_init (GeditWindowActivatableInterface *iface);

struct _GeditSpellPluginPrivate
{
	GeditWindow    *window;

	guint           statusbar_context_id;
	gulong          tab_added_id;
	gulong          tab_removed_id;
};

typedef struct _ViewData ViewData;

struct _ViewData
{
	GeditSpellPlugin *plugin;
	GeditView *view;
	GeditAutomaticSpellChecker *auto_spell;

	/* The doc is also needed, to be sure that the signals are disconnected
	 * on the same doc.
	 */
	GeditDocument *doc;
};

enum
{
	PROP_0,
	PROP_WINDOW
};

G_DEFINE_DYNAMIC_TYPE_EXTENDED (GeditSpellPlugin,
				gedit_spell_plugin,
				PEAS_TYPE_EXTENSION_BASE,
				0,
				G_IMPLEMENT_INTERFACE_DYNAMIC (GEDIT_TYPE_WINDOW_ACTIVATABLE,
							       gedit_window_activatable_iface_init)
				G_ADD_PRIVATE_DYNAMIC (GeditSpellPlugin))

static void	spell_cb			(GSimpleAction *action, GVariant *parameter, gpointer data);
static void	set_language_cb			(GSimpleAction *action, GVariant *parameter, gpointer data);
static void	auto_spell_activate_cb		(GSimpleAction *action, GVariant *parameter, gpointer data);
static void	auto_spell_change_state_cb	(GSimpleAction *action, GVariant *state, gpointer data);

static void	on_document_loaded		(GeditDocument *doc, ViewData *data);
static void	on_document_saved		(GeditDocument *doc, ViewData *data);
static void	set_auto_spell_from_metadata	(ViewData *data);

static GActionEntry action_entries[] =
{
	{ "check-spell", spell_cb },
	{ "config-spell", set_language_cb },
	{ "auto-spell", auto_spell_activate_cb, NULL, "false", auto_spell_change_state_cb }
};

static GQuark spell_checker_id = 0;

static ViewData *
view_data_new (GeditSpellPlugin *plugin,
	       GeditView        *view)
{
	ViewData *data;

	data = g_slice_new (ViewData);
	data->plugin = g_object_ref (plugin);
	data->view = g_object_ref (view);
	data->auto_spell = NULL;

	data->doc = GEDIT_DOCUMENT (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)));
	g_object_ref (data->doc);

	g_signal_connect (data->doc,
			  "loaded",
			  G_CALLBACK (on_document_loaded),
			  data);

	g_signal_connect (data->doc,
			  "saved",
			  G_CALLBACK (on_document_saved),
			  data);

	set_auto_spell_from_metadata (data);

	return data;
}

static void
view_data_free (ViewData *data)
{
	if (data == NULL)
	{
		return;
	}

	if (data->doc != NULL)
	{
		g_signal_handlers_disconnect_by_func (data->doc, on_document_loaded, data);
		g_signal_handlers_disconnect_by_func (data->doc, on_document_saved, data);

		g_object_unref (data->doc);
	}

	if (data->auto_spell != NULL && data->view != NULL)
	{
		gedit_automatic_spell_checker_detach_view (data->auto_spell,
							   GTK_TEXT_VIEW (data->view));
	}

	g_clear_object (&data->plugin);
	g_clear_object (&data->view);
	g_clear_object (&data->auto_spell);

	g_slice_free (ViewData, data);
}

static void
gedit_spell_plugin_init (GeditSpellPlugin *plugin)
{
	gedit_debug_message (DEBUG_PLUGINS, "GeditSpellPlugin initializing");

	plugin->priv = gedit_spell_plugin_get_instance_private (plugin);
}

static void
gedit_spell_plugin_dispose (GObject *object)
{
	GeditSpellPlugin *plugin = GEDIT_SPELL_PLUGIN (object);

	gedit_debug_message (DEBUG_PLUGINS, "GeditSpellPlugin disposing");

	g_clear_object (&plugin->priv->window);

	G_OBJECT_CLASS (gedit_spell_plugin_parent_class)->dispose (object);
}

static void
gedit_spell_plugin_set_property (GObject      *object,
				 guint         prop_id,
				 const GValue *value,
				 GParamSpec   *pspec)
{
	GeditSpellPlugin *plugin = GEDIT_SPELL_PLUGIN (object);

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
gedit_spell_plugin_get_property (GObject    *object,
				 guint       prop_id,
				 GValue     *value,
				 GParamSpec *pspec)
{
	GeditSpellPlugin *plugin = GEDIT_SPELL_PLUGIN (object);

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
language_notify_cb (GeditSpellChecker *checker,
		    GParamSpec        *pspec,
		    GeditDocument     *doc)
{
	const GeditSpellCheckerLanguage *lang;
	const gchar *key;

	g_return_if_fail (GEDIT_IS_DOCUMENT (doc));

	lang = gedit_spell_checker_get_language (checker);
	g_return_if_fail (lang != NULL);

	key = gedit_spell_checker_language_to_key (lang);
	g_return_if_fail (key != NULL);

	gedit_document_set_metadata (doc,
				     GEDIT_METADATA_ATTRIBUTE_SPELL_LANGUAGE, key,
				     NULL);
}

static const GeditSpellCheckerLanguage *
get_language_from_metadata (GeditDocument *doc)
{
	const GeditSpellCheckerLanguage *lang = NULL;
	gchar *value = NULL;

	value = gedit_document_get_metadata (doc, GEDIT_METADATA_ATTRIBUTE_SPELL_LANGUAGE);

	if (value != NULL)
	{
		lang = gedit_spell_checker_language_from_key (value);
		g_free (value);
	}

	return lang;
}

static GeditSpellChecker *
get_spell_checker_from_document (GeditDocument *doc)
{
	GeditSpellChecker *checker;
	gpointer data;

	gedit_debug (DEBUG_PLUGINS);

	g_return_val_if_fail (doc != NULL, NULL);

	data = g_object_get_qdata (G_OBJECT (doc), spell_checker_id);

	if (data == NULL)
	{
		const GeditSpellCheckerLanguage *lang;

		lang = get_language_from_metadata (doc);
		checker = gedit_spell_checker_new (lang);

		g_object_set_qdata_full (G_OBJECT (doc),
					 spell_checker_id,
					 checker,
					 g_object_unref);

		g_signal_connect (checker,
				  "notify::language",
				  G_CALLBACK (language_notify_cb),
				  doc);
	}
	else
	{
		g_return_val_if_fail (GEDIT_IS_SPELL_CHECKER (data), NULL);
		checker = data;
	}

	return checker;
}

static void
goto_next_cb (GeditSpellCheckerDialog *dialog,
	      GeditSpellNavigator     *navigator)
{
	gchar *word;
	GError *error = NULL;

	word = gedit_spell_navigator_goto_next (navigator, &error);

	if (error != NULL)
	{
		g_warning ("Spell checker plugin: %s", error->message);
		g_error_free (error);
	}

	if (word != NULL)
	{
		gedit_spell_checker_dialog_set_misspelled_word (dialog, word);
		g_free (word);
	}
	else
	{
		gedit_spell_checker_dialog_set_completed (dialog);
	}
}

static void
change_cb (GeditSpellCheckerDialog *dialog,
	   const gchar             *word,
	   const gchar             *change_to,
	   GeditSpellNavigator     *navigator)
{
	gedit_spell_navigator_change (navigator, word, change_to);
}

static void
change_all_cb (GeditSpellCheckerDialog *dialog,
	       const gchar             *word,
	       const gchar             *change_to,
	       GeditSpellNavigator     *navigator)
{
	gedit_spell_navigator_change_all (navigator, word, change_to);
}

static void
language_dialog_response (GtkDialog         *dialog,
			  gint               response_id,
			  GeditSpellChecker *checker)
{
	if (response_id == GTK_RESPONSE_HELP)
	{
		gedit_app_show_help (GEDIT_APP (g_application_get_default ()),
				     GTK_WINDOW (dialog),
				     NULL,
				     "gedit-spellcheck");
		return;
	}

	if (response_id == GTK_RESPONSE_OK)
	{
		const GeditSpellCheckerLanguage *lang;

		lang = gedit_spell_language_get_selected_language (GEDIT_SPELL_LANGUAGE_DIALOG (dialog));
		if (lang != NULL)
		{
			gedit_spell_checker_set_language (checker, lang);
		}
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
set_language_cb (GSimpleAction *action,
		 GVariant      *parameter,
		 gpointer       data)
{
	GeditSpellPlugin *plugin = GEDIT_SPELL_PLUGIN (data);
	GeditSpellPluginPrivate *priv;
	GeditDocument *doc;
	GeditSpellChecker *checker;
	const GeditSpellCheckerLanguage *lang;
	GtkWidget *dialog;
	GtkWindowGroup *wg;

	gedit_debug (DEBUG_PLUGINS);

	priv = plugin->priv;

	doc = gedit_window_get_active_document (priv->window);
	g_return_if_fail (doc != NULL);

	checker = get_spell_checker_from_document (doc);
	g_return_if_fail (checker != NULL);

	lang = gedit_spell_checker_get_language (checker);

	dialog = gedit_spell_language_dialog_new (GTK_WINDOW (priv->window), lang);

	wg = gedit_window_get_group (priv->window);

	gtk_window_group_add_window (wg, GTK_WINDOW (dialog));

	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

	gtk_dialog_add_button (GTK_DIALOG (dialog),
			       _("_Help"),
			       GTK_RESPONSE_HELP);

	g_signal_connect (dialog,
			  "response",
			  G_CALLBACK (language_dialog_response),
			  checker);

	gtk_widget_show (dialog);
}

static void
spell_cb (GSimpleAction *action,
	  GVariant      *parameter,
	  gpointer       data)
{
	GeditSpellPlugin *plugin = GEDIT_SPELL_PLUGIN (data);
	GeditSpellPluginPrivate *priv;
	GeditView *view;
	GeditDocument *doc;
	GeditSpellChecker *checker;
	GeditSpellNavigator *navigator;
	GtkWidget *dialog;
	gchar *word;
	GtkTextIter start;
	GtkTextIter end;
	GError *error = NULL;

	gedit_debug (DEBUG_PLUGINS);

	priv = plugin->priv;

	view = gedit_window_get_active_view (priv->window);
	g_return_if_fail (view != NULL);

	doc = GEDIT_DOCUMENT (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)));

	checker = get_spell_checker_from_document (doc);
	g_return_if_fail (checker != NULL);

	if (gtk_text_buffer_get_char_count (GTK_TEXT_BUFFER (doc)) <= 0)
	{
		GtkWidget *statusbar;

		statusbar = gedit_window_get_statusbar (priv->window);
		gedit_statusbar_flash_message (GEDIT_STATUSBAR (statusbar),
					       priv->statusbar_context_id,
					       _("The document is empty."));

		return;
	}

	navigator = gedit_spell_navigator_gtv_new (GTK_TEXT_VIEW (view), checker);

	word = gedit_spell_navigator_goto_next (navigator, &error);

	if (error != NULL)
	{
		g_warning ("Spell checker plugin: %s", error->message);
		g_error_free (error);
		error = NULL;
	}

	if (word == NULL)
	{
		GtkWidget *statusbar;

		statusbar = gedit_window_get_statusbar (priv->window);
		gedit_statusbar_flash_message (GEDIT_STATUSBAR (statusbar),
					       priv->statusbar_context_id,
					       _("No misspelled words"));

		g_object_unref (navigator);
		return;
	}

	dialog = gedit_spell_checker_dialog_new (GTK_WINDOW (priv->window), checker);

	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

	g_object_set_data_full (G_OBJECT (dialog),
				NAVIGATOR_KEY,
				navigator,
				g_object_unref);

	g_signal_connect (dialog,
			  "change",
			  G_CALLBACK (change_cb),
			  navigator);

	g_signal_connect (dialog,
			  "change-all",
			  G_CALLBACK (change_all_cb),
			  navigator);

	g_signal_connect (dialog,
			  "goto-next",
			  G_CALLBACK (goto_next_cb),
			  navigator);

	gedit_spell_checker_dialog_set_misspelled_word (GEDIT_SPELL_CHECKER_DIALOG (dialog), word);
	g_free (word);

	gtk_text_buffer_get_selection_bounds (GTK_TEXT_BUFFER (doc), &start, &end);

	gtk_widget_show (dialog);

	/* Restore selection. Showing the dialog makes a focus change, which
	 * unselects the GtkTextBuffer selection.
	 */
	gtk_text_buffer_select_range (GTK_TEXT_BUFFER (doc), &start, &end);
}

static void
set_auto_spell (ViewData *data,
		gboolean  active)
{
	if (!active)
	{
		if (data->auto_spell != NULL && data->view != NULL)
		{
			gedit_automatic_spell_checker_detach_view (data->auto_spell,
								   GTK_TEXT_VIEW (data->view));
		}

		g_clear_object (&data->auto_spell);
	}
	else if (data->auto_spell == NULL)
	{
		GeditSpellChecker *checker;
		GtkTextBuffer *buffer;

		checker = get_spell_checker_from_document (data->doc);
		g_return_if_fail (checker != NULL);

		buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (data->view));
		g_return_if_fail (buffer == GTK_TEXT_BUFFER (data->doc));

		data->auto_spell = gedit_automatic_spell_checker_new (GTK_SOURCE_BUFFER (data->doc),
								      checker);

		gedit_automatic_spell_checker_attach_view (data->auto_spell,
							   GTK_TEXT_VIEW (data->view));

		gedit_automatic_spell_checker_recheck_all (data->auto_spell);
	}
}

static void
auto_spell_activate_cb (GSimpleAction *action,
			GVariant      *parameter,
			gpointer       data)
{
	GeditSpellPlugin *plugin = GEDIT_SPELL_PLUGIN (data);
	GeditSpellPluginPrivate *priv = plugin->priv;
	GVariant *state;
	gboolean active;
	GeditView *view;

	gedit_debug (DEBUG_PLUGINS);

	state = g_action_get_state (G_ACTION (action));
	g_return_if_fail (state != NULL);

	active = g_variant_get_boolean (state);
	g_variant_unref (state);

	/* We must toggle ourself the value. */
	active = !active;
	g_action_change_state (G_ACTION (action), g_variant_new_boolean (active));

	view = gedit_window_get_active_view (priv->window);
	if (view != NULL)
	{
		GeditDocument *doc;

		doc = GEDIT_DOCUMENT (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)));

		/* Set metadata in the "activate" handler, not in "change-state"
		 * because "change-state" is called every time the state
		 * changes, not specifically when the user has changed the state
		 * herself. For example "change-state" is called to initialize
		 * the sate to the default value specified in the GActionEntry.
		 */
		gedit_document_set_metadata (doc,
					     GEDIT_METADATA_ATTRIBUTE_SPELL_ENABLED,
					     active ? SPELL_ENABLED_STR : NULL,
					     NULL);
	}
}

static void
auto_spell_change_state_cb (GSimpleAction *action,
			    GVariant      *state,
			    gpointer       data)
{
	GeditSpellPlugin *plugin = GEDIT_SPELL_PLUGIN (data);
	GeditSpellPluginPrivate *priv = plugin->priv;
	GeditView *view;
	gboolean active;

	gedit_debug (DEBUG_PLUGINS);

	active = g_variant_get_boolean (state);

	gedit_debug_message (DEBUG_PLUGINS, active ? "Auto Spell activated" : "Auto Spell deactivated");

	view = gedit_window_get_active_view (priv->window);
	if (view != NULL)
	{
		ViewData *data = g_object_get_data (G_OBJECT (view), VIEW_DATA_KEY);

		if (data != NULL)
		{
			set_auto_spell (data, active);
		}

		g_simple_action_set_state (action, g_variant_new_boolean (active));
	}
}

static void
update_ui (GeditSpellPlugin *plugin)
{
	GeditSpellPluginPrivate *priv;
	GeditView *view;
	GAction *check_spell_action;
	GAction *config_spell_action;
	GAction *auto_spell_action;

	gedit_debug (DEBUG_PLUGINS);

	priv = plugin->priv;

	view = gedit_window_get_active_view (priv->window);

	check_spell_action = g_action_map_lookup_action (G_ACTION_MAP (priv->window),
	                                                 "check-spell");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (check_spell_action),
	                             (view != NULL) &&
	                             gtk_text_view_get_editable (GTK_TEXT_VIEW (view)));

	config_spell_action = g_action_map_lookup_action (G_ACTION_MAP (priv->window),
	                                                  "config-spell");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (config_spell_action),
	                             (view != NULL) &&
	                             gtk_text_view_get_editable (GTK_TEXT_VIEW (view)));

	auto_spell_action = g_action_map_lookup_action (G_ACTION_MAP (priv->window),
	                                                "auto-spell");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (auto_spell_action),
	                             (view != NULL) &&
	                             gtk_text_view_get_editable (GTK_TEXT_VIEW (view)));

	if (view != NULL)
	{
		GeditTab *tab;
		GtkTextBuffer *buffer;

		tab = gedit_window_get_active_tab (priv->window);
		g_return_if_fail (gedit_tab_get_view (tab) == view);

		/* If the document is loading we can't get the metadata so we
		 * endup with an useless speller.
		 */
		if (gedit_tab_get_state (tab) == GEDIT_TAB_STATE_NORMAL)
		{
			ViewData *data;
			gboolean auto_spell_enabled;

			data = g_object_get_data (G_OBJECT (view), VIEW_DATA_KEY);
			auto_spell_enabled = data != NULL && data->auto_spell != NULL;

			g_action_change_state (auto_spell_action,
					       g_variant_new_boolean (auto_spell_enabled));
		}

		buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

		g_simple_action_set_enabled (G_SIMPLE_ACTION (check_spell_action),
		                             gtk_text_buffer_get_char_count (buffer) > 0);
	}
}

static void
set_auto_spell_from_metadata (ViewData *data)
{
	GeditSpellPlugin *plugin = data->plugin;
	gboolean active = FALSE;
	gchar *active_str;
	GeditView *active_view;

	active_str = gedit_document_get_metadata (data->doc, GEDIT_METADATA_ATTRIBUTE_SPELL_ENABLED);
	if (active_str != NULL)
	{
		active = g_str_equal (active_str, SPELL_ENABLED_STR);
		g_free (active_str);
	}

	set_auto_spell (data, active);

	/* In case that the view is the active one we mark the spell action */
	active_view = gedit_window_get_active_view (plugin->priv->window);

	if (active_view == data->view)
	{
		GAction *action;

		action = g_action_map_lookup_action (G_ACTION_MAP (plugin->priv->window),
		                                     "auto-spell");
		g_action_change_state (action, g_variant_new_boolean (active));
	}
}

static void
on_document_loaded (GeditDocument *doc,
		    ViewData      *data)
{
	GeditSpellChecker *checker;

	checker = GEDIT_SPELL_CHECKER (g_object_get_qdata (G_OBJECT (doc),
							   spell_checker_id));
	if (checker != NULL)
	{
		const GeditSpellCheckerLanguage *lang;

		lang = get_language_from_metadata (doc);

		if (lang != NULL)
		{
			g_signal_handlers_block_by_func (checker, language_notify_cb, doc);
			gedit_spell_checker_set_language (checker, lang);
			g_signal_handlers_unblock_by_func (checker, language_notify_cb, doc);
		}
	}

	set_auto_spell_from_metadata (data);
}

static void
on_document_saved (GeditDocument *doc,
		   ViewData      *data)
{
	GeditSpellChecker *checker;
	const gchar *key;

	/* Make sure to save the metadata here too */

	checker = GEDIT_SPELL_CHECKER (g_object_get_qdata (G_OBJECT (doc), spell_checker_id));

	if (checker != NULL)
	{
		key = gedit_spell_checker_language_to_key (gedit_spell_checker_get_language (checker));
	}
	else
	{
		key = NULL;
	}

	gedit_document_set_metadata (doc,
	                             GEDIT_METADATA_ATTRIBUTE_SPELL_ENABLED,
				     data->auto_spell != NULL ? SPELL_ENABLED_STR : NULL,
	                             GEDIT_METADATA_ATTRIBUTE_SPELL_LANGUAGE,
	                             key,
	                             NULL);
}

static void
tab_added_cb (GeditWindow      *window,
	      GeditTab         *tab,
	      GeditSpellPlugin *plugin)
{
	GeditView *view;
	ViewData *data;

	view = gedit_tab_get_view (tab);

	data = view_data_new (plugin, view);
	g_object_set_data_full (G_OBJECT (view),
				VIEW_DATA_KEY,
				data,
				(GDestroyNotify) view_data_free);
}

static void
tab_removed_cb (GeditWindow      *window,
		GeditTab         *tab,
		GeditSpellPlugin *plugin)
{
	GeditView *view = gedit_tab_get_view (tab);
	g_object_set_data (G_OBJECT (view), VIEW_DATA_KEY, NULL);
}

static void
gedit_spell_plugin_activate (GeditWindowActivatable *activatable)
{
	GeditSpellPlugin *plugin;
	GeditSpellPluginPrivate *priv;
	GtkStatusbar *statusbar;
	GList *views, *l;

	gedit_debug (DEBUG_PLUGINS);

	plugin = GEDIT_SPELL_PLUGIN (activatable);
	priv = plugin->priv;

	g_action_map_add_action_entries (G_ACTION_MAP (priv->window),
	                                 action_entries,
	                                 G_N_ELEMENTS (action_entries),
	                                 activatable);

	statusbar = GTK_STATUSBAR (gedit_window_get_statusbar (priv->window));
	priv->statusbar_context_id = gtk_statusbar_get_context_id (statusbar, "spell_plugin_message");

	update_ui (plugin);

	views = gedit_window_get_views (priv->window);
	for (l = views; l != NULL; l = g_list_next (l))
	{
		GeditView *view = GEDIT_VIEW (l->data);
		ViewData *data = view_data_new (plugin, view);

		g_object_set_data_full (G_OBJECT (view),
					VIEW_DATA_KEY,
					data,
					(GDestroyNotify) view_data_free);
	}

	priv->tab_added_id = g_signal_connect (priv->window,
					       "tab-added",
					       G_CALLBACK (tab_added_cb),
					       activatable);

	priv->tab_removed_id = g_signal_connect (priv->window,
						 "tab-removed",
						 G_CALLBACK (tab_removed_cb),
						 activatable);
}

static void
gedit_spell_plugin_deactivate (GeditWindowActivatable *activatable)
{
	GeditSpellPluginPrivate *priv;

	gedit_debug (DEBUG_PLUGINS);

	priv = GEDIT_SPELL_PLUGIN (activatable)->priv;

	g_action_map_remove_action (G_ACTION_MAP (priv->window),
	                            "check-spell");
	g_action_map_remove_action (G_ACTION_MAP (priv->window),
	                            "config-spell");
	g_action_map_remove_action (G_ACTION_MAP (priv->window),
	                            "auto-spell");

	g_signal_handler_disconnect (priv->window, priv->tab_added_id);
	g_signal_handler_disconnect (priv->window, priv->tab_removed_id);
}

static void
gedit_spell_plugin_update_state (GeditWindowActivatable *activatable)
{
	gedit_debug (DEBUG_PLUGINS);

	update_ui (GEDIT_SPELL_PLUGIN (activatable));
}

static void
gedit_spell_plugin_class_init (GeditSpellPluginClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = gedit_spell_plugin_dispose;
	object_class->set_property = gedit_spell_plugin_set_property;
	object_class->get_property = gedit_spell_plugin_get_property;

	if (spell_checker_id == 0)
	{
		spell_checker_id = g_quark_from_string ("GeditSpellCheckerID");
	}

	g_object_class_override_property (object_class, PROP_WINDOW, "window");
}

static void
gedit_spell_plugin_class_finalize (GeditSpellPluginClass *klass)
{
}

static void
gedit_window_activatable_iface_init (GeditWindowActivatableInterface *iface)
{
	iface->activate = gedit_spell_plugin_activate;
	iface->deactivate = gedit_spell_plugin_deactivate;
	iface->update_state = gedit_spell_plugin_update_state;
}

G_MODULE_EXPORT void
peas_register_types (PeasObjectModule *module)
{
	gedit_spell_plugin_register_type (G_TYPE_MODULE (module));
	gedit_spell_app_activatable_register (G_TYPE_MODULE (module));

	peas_object_module_register_extension_type (module,
						    GEDIT_TYPE_WINDOW_ACTIVATABLE,
						    GEDIT_TYPE_SPELL_PLUGIN);
}

/* ex:set ts=8 noet: */
