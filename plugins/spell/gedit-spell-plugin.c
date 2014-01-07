/*
 * gedit-spell-plugin.c
 *
 * Copyright (C) 2002-2005 Paolo Maggi
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gedit-spell-plugin.h"
#include "gedit-spell-app-activatable.h"
#include "gedit-spell-utils.h"

#include <string.h> /* For strlen */

#include <glib/gi18n.h>

#include <gedit/gedit-debug.h>
#include <gedit/gedit-statusbar.h>
#include <gedit/gedit-window.h>
#include <gedit/gedit-window-activatable.h>
#include <gtksourceview/gtksource.h>

#include "gedit-spell-checker.h"
#include "gedit-spell-checker-dialog.h"
#include "gedit-spell-language-dialog.h"
#include "gedit-automatic-spell-checker.h"

#ifdef G_OS_WIN32
#include <gedit/gedit-metadata-manager.h>
#define GEDIT_METADATA_ATTRIBUTE_SPELL_LANGUAGE "spell-language"
#define GEDIT_METADATA_ATTRIBUTE_SPELL_ENABLED  "spell-enabled"
#else
#define GEDIT_METADATA_ATTRIBUTE_SPELL_LANGUAGE "metadata::gedit-spell-language"
#define GEDIT_METADATA_ATTRIBUTE_SPELL_ENABLED  "metadata::gedit-spell-enabled"
#endif

#define GEDIT_AUTOMATIC_SPELL_VIEW "GeditAutomaticSpellView"

#define GEDIT_SPELL_PLUGIN_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), \
					       GEDIT_TYPE_SPELL_PLUGIN, \
					       GeditSpellPluginPrivate))

static void gedit_window_activatable_iface_init (GeditWindowActivatableInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (GeditSpellPlugin,
				gedit_spell_plugin,
				PEAS_TYPE_EXTENSION_BASE,
				0,
				G_IMPLEMENT_INTERFACE_DYNAMIC (GEDIT_TYPE_WINDOW_ACTIVATABLE,
							       gedit_window_activatable_iface_init))

struct _GeditSpellPluginPrivate
{
	GeditWindow    *window;

	GeditMenuExtension *menu;
	guint           message_cid;
	gulong          tab_added_id;
	gulong          tab_removed_id;
};

typedef struct _CheckRange CheckRange;

struct _CheckRange
{
	GtkTextMark *start_mark;
	GtkTextMark *end_mark;

	gint mw_start; /* misspelled word start */
	gint mw_end;   /* end */

	GtkTextMark *current_mark;
};

enum
{
	PROP_0,
	PROP_WINDOW
};

static void	spell_cb	(GSimpleAction *action, GVariant *parameter, gpointer data);
static void	set_language_cb	(GSimpleAction *action, GVariant *parameter, gpointer data);
static void	auto_spell_cb	(GSimpleAction *action, GVariant *state, gpointer data);

static void
activate_toggle (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       user_data)
{
	GVariant *state;

	state = g_action_get_state (G_ACTION (action));
	g_action_change_state (G_ACTION (action),
	                       g_variant_new_boolean (!g_variant_get_boolean (state)));
	g_variant_unref (state);
}

static GActionEntry action_entries[] =
{
	{ "check_spell", spell_cb },
	{ "config_spell", set_language_cb },
	{ "auto_spell", activate_toggle, NULL, "false", auto_spell_cb }
};

static GQuark spell_checker_id = 0;
static GQuark check_range_id = 0;

static void
gedit_spell_plugin_init (GeditSpellPlugin *plugin)
{
	gedit_debug_message (DEBUG_PLUGINS, "GeditSpellPlugin initializing");

	plugin->priv = G_TYPE_INSTANCE_GET_PRIVATE (plugin,
						    GEDIT_TYPE_SPELL_PLUGIN,
						    GeditSpellPluginPrivate);
}

static void
gedit_spell_plugin_dispose (GObject *object)
{
	GeditSpellPlugin *plugin = GEDIT_SPELL_PLUGIN (object);

	gedit_debug_message (DEBUG_PLUGINS, "GeditSpellPlugin disposing");

	g_clear_object (&plugin->priv->menu);
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
set_spell_language_cb (GeditSpellChecker   *spell,
		       const GeditSpellCheckerLanguage *lang,
		       GeditDocument 	   *doc)
{
	const gchar *key;

	g_return_if_fail (GEDIT_IS_DOCUMENT (doc));
	g_return_if_fail (lang != NULL);

	key = gedit_spell_checker_language_to_key (lang);
	g_return_if_fail (key != NULL);

	gedit_document_set_metadata (doc, GEDIT_METADATA_ATTRIBUTE_SPELL_LANGUAGE,
				     key, NULL);
}

static void
set_language_from_metadata (GeditSpellChecker *spell,
			    GeditDocument     *doc)
{
	const GeditSpellCheckerLanguage *lang = NULL;
	gchar *value = NULL;

	value = gedit_document_get_metadata (doc, GEDIT_METADATA_ATTRIBUTE_SPELL_LANGUAGE);

	if (value != NULL)
	{
		lang = gedit_spell_checker_language_from_key (value);
		g_free (value);
	}

	if (lang != NULL)
	{
		g_signal_handlers_block_by_func (spell, set_spell_language_cb, doc);
		gedit_spell_checker_set_language (spell, lang);
		g_signal_handlers_unblock_by_func (spell, set_spell_language_cb, doc);
	}
}

static GeditSpellChecker *
get_spell_checker_from_document (GeditDocument *doc)
{
	GeditSpellChecker *spell;
	gpointer data;

	gedit_debug (DEBUG_PLUGINS);

	g_return_val_if_fail (doc != NULL, NULL);

	data = g_object_get_qdata (G_OBJECT (doc), spell_checker_id);

	if (data == NULL)
	{
		spell = gedit_spell_checker_new ();

		set_language_from_metadata (spell, doc);

		g_object_set_qdata_full (G_OBJECT (doc),
					 spell_checker_id,
					 spell,
					 (GDestroyNotify) g_object_unref);

		g_signal_connect (spell,
				  "set_language",
				  G_CALLBACK (set_spell_language_cb),
				  doc);
	}
	else
	{
		g_return_val_if_fail (GEDIT_IS_SPELL_CHECKER (data), NULL);
		spell = GEDIT_SPELL_CHECKER (data);
	}

	return spell;
}

static CheckRange *
get_check_range (GeditDocument *doc)
{
	CheckRange *range;

	gedit_debug (DEBUG_PLUGINS);

	g_return_val_if_fail (doc != NULL, NULL);

	range = (CheckRange *) g_object_get_qdata (G_OBJECT (doc), check_range_id);

	return range;
}

static void
update_current (GeditDocument *doc,
		gint           current)
{
	CheckRange *range;
	GtkTextIter iter;
	GtkTextIter end_iter;

	gedit_debug (DEBUG_PLUGINS);

	g_return_if_fail (doc != NULL);
	g_return_if_fail (current >= 0);

	range = get_check_range (doc);
	g_return_if_fail (range != NULL);

	gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (doc),
					    &iter, current);

	if (!gtk_text_iter_inside_word (&iter))
	{
		/* if we're not inside a word,
		 * we must be in some spaces.
		 * skip forward to the beginning of the next word. */
		if (!gtk_text_iter_is_end (&iter))
		{
			gtk_text_iter_forward_word_end (&iter);
			gtk_text_iter_backward_word_start (&iter);
		}
	}
	else
	{
		if (!gtk_text_iter_starts_word (&iter))
			gtk_text_iter_backward_word_start (&iter);
	}

	gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (doc),
					  &end_iter,
					  range->end_mark);

	if (gtk_text_iter_compare (&end_iter, &iter) < 0)
	{
		gtk_text_buffer_move_mark (GTK_TEXT_BUFFER (doc),
					   range->current_mark,
					   &end_iter);
	}
	else
	{
		gtk_text_buffer_move_mark (GTK_TEXT_BUFFER (doc),
					   range->current_mark,
					   &iter);
	}
}

static void
set_check_range (GeditDocument *doc,
		 GtkTextIter   *start,
		 GtkTextIter   *end)
{
	CheckRange *range;
	GtkTextIter iter;

	gedit_debug (DEBUG_PLUGINS);

	range = get_check_range (doc);

	if (range == NULL)
	{
		gedit_debug_message (DEBUG_PLUGINS, "There was not a previous check range");

		gtk_text_buffer_get_end_iter (GTK_TEXT_BUFFER (doc), &iter);

		range = g_new0 (CheckRange, 1);

		range->start_mark = gtk_text_buffer_create_mark (GTK_TEXT_BUFFER (doc),
				"check_range_start_mark", &iter, TRUE);

		range->end_mark = gtk_text_buffer_create_mark (GTK_TEXT_BUFFER (doc),
				"check_range_end_mark", &iter, FALSE);

		range->current_mark = gtk_text_buffer_create_mark (GTK_TEXT_BUFFER (doc),
				"check_range_current_mark", &iter, TRUE);

		g_object_set_qdata_full (G_OBJECT (doc),
				 check_range_id,
				 range,
				 (GDestroyNotify)g_free);
	}

	if (gedit_spell_utils_skip_no_spell_check (start, end))
	{
		if (!gtk_text_iter_inside_word (end))
		{
			/* if we're neither inside a word,
			 * we must be in some spaces.
			 * skip backward to the end of the previous word. */
			if (!gtk_text_iter_is_end (end))
			{
				gtk_text_iter_backward_word_start (end);
				gtk_text_iter_forward_word_end (end);
			}
		}
		else
		{
			if (!gtk_text_iter_ends_word (end))
				gtk_text_iter_forward_word_end (end);
		}
	}
	else
	{
		/* no spell checking in the specified range */
		start = end;
	}

	gtk_text_buffer_move_mark (GTK_TEXT_BUFFER (doc),
				   range->start_mark,
				   start);
	gtk_text_buffer_move_mark (GTK_TEXT_BUFFER (doc),
				   range->end_mark,
				   end);

	range->mw_start = -1;
	range->mw_end = -1;

	update_current (doc, gtk_text_iter_get_offset (start));
}

static gchar *
get_current_word (GeditDocument *doc, gint *start, gint *end)
{
	const CheckRange *range;
	GtkTextIter end_iter;
	GtkTextIter current_iter;
	gint range_end;

	gedit_debug (DEBUG_PLUGINS);

	g_return_val_if_fail (doc != NULL, NULL);
	g_return_val_if_fail (start != NULL, NULL);
	g_return_val_if_fail (end != NULL, NULL);

	range = get_check_range (doc);
	g_return_val_if_fail (range != NULL, NULL);

	gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (doc),
			&end_iter, range->end_mark);

	range_end = gtk_text_iter_get_offset (&end_iter);

	gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (doc),
			&current_iter, range->current_mark);

	end_iter = current_iter;

	if (!gtk_text_iter_is_end (&end_iter))
	{
		gedit_debug_message (DEBUG_PLUGINS, "Current is not end");

		gtk_text_iter_forward_word_end (&end_iter);
	}

	*start = gtk_text_iter_get_offset (&current_iter);
	*end = MIN (gtk_text_iter_get_offset (&end_iter), range_end);

	gedit_debug_message (DEBUG_PLUGINS, "Current word extends [%d, %d]", *start, *end);

	if (!(*start < *end))
		return NULL;

	return gtk_text_buffer_get_slice (GTK_TEXT_BUFFER (doc),
					  &current_iter,
					  &end_iter,
					  TRUE);
}

static gboolean
goto_next_word (GeditDocument *doc)
{
	CheckRange *range;
	GtkTextIter current_iter;
	GtkTextIter old_current_iter;
	GtkTextIter end_iter;

	gedit_debug (DEBUG_PLUGINS);

	g_return_val_if_fail (doc != NULL, FALSE);

	range = get_check_range (doc);
	g_return_val_if_fail (range != NULL, FALSE);

	gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (doc),
					  &current_iter,
					  range->current_mark);
	gtk_text_buffer_get_end_iter (GTK_TEXT_BUFFER (doc), &end_iter);

	old_current_iter = current_iter;

	gtk_text_iter_forward_word_ends (&current_iter, 2);
	gtk_text_iter_backward_word_start (&current_iter);

	if (gedit_spell_utils_skip_no_spell_check (&current_iter, &end_iter) &&
	    (gtk_text_iter_compare (&old_current_iter, &current_iter) < 0) &&
	    (gtk_text_iter_compare (&current_iter, &end_iter) < 0))
	{
		update_current (doc, gtk_text_iter_get_offset (&current_iter));
		return TRUE;
	}

	return FALSE;
}

static gchar *
get_next_misspelled_word (GeditView *view)
{
	GeditDocument *doc;
	CheckRange *range;
	gint start, end;
	gchar *word;
	GeditSpellChecker *spell;

	g_return_val_if_fail (view != NULL, NULL);

	doc = GEDIT_DOCUMENT (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)));
	g_return_val_if_fail (doc != NULL, NULL);

	range = get_check_range (doc);
	g_return_val_if_fail (range != NULL, NULL);

	spell = get_spell_checker_from_document (doc);
	g_return_val_if_fail (spell != NULL, NULL);

	word = get_current_word (doc, &start, &end);
	if (word == NULL)
		return NULL;

	gedit_debug_message (DEBUG_PLUGINS, "Word to check: %s", word);

	while (gedit_spell_checker_check_word (spell, word, -1))
	{
		g_free (word);

		if (!goto_next_word (doc))
			return NULL;

		/* may return null if we reached the end of the selection */
		word = get_current_word (doc, &start, &end);
		if (word == NULL)
			return NULL;

		gedit_debug_message (DEBUG_PLUGINS, "Word to check: %s", word);
	}

	if (!goto_next_word (doc))
		update_current (doc, gtk_text_buffer_get_char_count (GTK_TEXT_BUFFER (doc)));

	if (word != NULL)
	{
		GtkTextIter s, e;

		range->mw_start = start;
		range->mw_end = end;

		gedit_debug_message (DEBUG_PLUGINS, "Select [%d, %d]", start, end);

		gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (doc), &s, start);
		gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (doc), &e, end);

		gtk_text_buffer_select_range (GTK_TEXT_BUFFER (doc), &s, &e);

		gedit_view_scroll_to_cursor (view);
	}
	else
	{
		range->mw_start = -1;
		range->mw_end = -1;
	}

	return word;
}

static void
ignore_cb (GeditSpellCheckerDialog *dlg,
	   const gchar             *w,
	   GeditView               *view)
{
	gchar *word = NULL;

	gedit_debug (DEBUG_PLUGINS);

	g_return_if_fail (w != NULL);
	g_return_if_fail (view != NULL);

	word = get_next_misspelled_word (view);
	if (word == NULL)
	{
		gedit_spell_checker_dialog_set_completed (dlg);

		return;
	}

	gedit_spell_checker_dialog_set_misspelled_word (GEDIT_SPELL_CHECKER_DIALOG (dlg),
							word,
							-1);

	g_free (word);
}

static void
change_cb (GeditSpellCheckerDialog *dlg,
	   const gchar             *word,
	   const gchar             *change,
	   GeditView               *view)
{
	GeditDocument *doc;
	CheckRange *range;
	gchar *w = NULL;
	GtkTextIter start, end;

	gedit_debug (DEBUG_PLUGINS);

	g_return_if_fail (view != NULL);
	g_return_if_fail (word != NULL);
	g_return_if_fail (change != NULL);

	doc = GEDIT_DOCUMENT (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)));
	g_return_if_fail (doc != NULL);

	range = get_check_range (doc);
	g_return_if_fail (range != NULL);

	gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (doc), &start, range->mw_start);
	if (range->mw_end < 0)
		gtk_text_buffer_get_end_iter (GTK_TEXT_BUFFER (doc), &end);
	else
		gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (doc), &end, range->mw_end);

	w = gtk_text_buffer_get_slice (GTK_TEXT_BUFFER (doc), &start, &end, TRUE);
	g_return_if_fail (w != NULL);

	if (strcmp (w, word) != 0)
	{
		g_free (w);
		return;
	}

	g_free (w);

	gtk_text_buffer_begin_user_action (GTK_TEXT_BUFFER(doc));

	gtk_text_buffer_delete (GTK_TEXT_BUFFER (doc), &start, &end);
	gtk_text_buffer_insert (GTK_TEXT_BUFFER (doc), &start, change, -1);

	gtk_text_buffer_end_user_action (GTK_TEXT_BUFFER(doc));

	update_current (doc, range->mw_start + g_utf8_strlen (change, -1));

	/* go to next misspelled word */
	ignore_cb (dlg, word, view);
}

static void
change_all_cb (GeditSpellCheckerDialog *dlg,
	       const gchar             *word,
	       const gchar             *change,
	       GeditView               *view)
{
	GeditDocument *doc;
	CheckRange *range;
	gchar *w = NULL;
	GtkTextIter start, end;
	GtkSourceSearchSettings *search_settings;
	GtkSourceSearchContext *search_context;

	gedit_debug (DEBUG_PLUGINS);

	g_return_if_fail (view != NULL);
	g_return_if_fail (word != NULL);
	g_return_if_fail (change != NULL);

	doc = GEDIT_DOCUMENT (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)));
	g_return_if_fail (doc != NULL);

	range = get_check_range (doc);
	g_return_if_fail (range != NULL);

	gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (doc), &start, range->mw_start);
	if (range->mw_end < 0)
		gtk_text_buffer_get_end_iter (GTK_TEXT_BUFFER (doc), &end);
	else
		gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (doc), &end, range->mw_end);

	w = gtk_text_buffer_get_slice (GTK_TEXT_BUFFER (doc), &start, &end, TRUE);
	g_return_if_fail (w != NULL);

	if (strcmp (w, word) != 0)
	{
		g_free (w);
		return;
	}

	g_free (w);

	search_settings = gtk_source_search_settings_new ();
	gtk_source_search_settings_set_case_sensitive (search_settings, TRUE);
	gtk_source_search_settings_set_at_word_boundaries (search_settings, TRUE);
	gtk_source_search_settings_set_search_text (search_settings, word);

	search_context = gtk_source_search_context_new (GTK_SOURCE_BUFFER (doc),
							search_settings);

	gtk_source_search_context_set_highlight (search_context, FALSE);

	gtk_source_search_context_replace_all (search_context, change, -1, NULL);

	update_current (doc, range->mw_start + g_utf8_strlen (change, -1));

	/* go to next misspelled word */
	ignore_cb (dlg, word, view);

	g_object_unref (search_settings);
	g_object_unref (search_context);
}

static void
add_word_cb (GeditSpellCheckerDialog *dlg,
	     const gchar             *word,
	     GeditView               *view)
{
	g_return_if_fail (view != NULL);
	g_return_if_fail (word != NULL);

	/* go to next misspelled word */
	ignore_cb (dlg, word, view);
}

static void
language_dialog_response (GtkDialog         *dlg,
			  gint               res_id,
			  GeditSpellChecker *spell)
{
	if (res_id == GTK_RESPONSE_OK)
	{
		const GeditSpellCheckerLanguage *lang;

		lang = gedit_spell_language_get_selected_language (GEDIT_SPELL_LANGUAGE_DIALOG (dlg));
		if (lang != NULL)
			gedit_spell_checker_set_language (spell, lang);
	}

	gtk_widget_destroy (GTK_WIDGET (dlg));
}

static void
set_language_cb (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       data)
{
	GeditSpellPlugin *plugin = GEDIT_SPELL_PLUGIN (data);
	GeditSpellPluginPrivate *priv;
	GeditDocument *doc;
	GeditSpellChecker *spell;
	const GeditSpellCheckerLanguage *lang;
	GtkWidget *dlg;
	GtkWindowGroup *wg;
	gchar *data_dir;

	gedit_debug (DEBUG_PLUGINS);

	priv = plugin->priv;

	doc = gedit_window_get_active_document (priv->window);
	g_return_if_fail (doc != NULL);

	spell = get_spell_checker_from_document (doc);
	g_return_if_fail (spell != NULL);

	lang = gedit_spell_checker_get_language (spell);

	data_dir = peas_extension_base_get_data_dir (PEAS_EXTENSION_BASE (plugin));
	dlg = gedit_spell_language_dialog_new (GTK_WINDOW (priv->window),
					       lang,
					       data_dir);
	g_free (data_dir);

	wg = gedit_window_get_group (priv->window);

	gtk_window_group_add_window (wg, GTK_WINDOW (dlg));

	gtk_window_set_modal (GTK_WINDOW (dlg), TRUE);

	g_signal_connect (dlg,
			  "response",
			  G_CALLBACK (language_dialog_response),
			  spell);

	gtk_widget_show (dlg);
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
	GeditSpellChecker *spell;
	GtkWidget *dlg;
	GtkTextIter start, end;
	gchar *word;
	gchar *data_dir;

	gedit_debug (DEBUG_PLUGINS);

	priv = plugin->priv;

	view = gedit_window_get_active_view (priv->window);
	g_return_if_fail (view != NULL);

	doc = GEDIT_DOCUMENT (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)));
	g_return_if_fail (doc != NULL);

	spell = get_spell_checker_from_document (doc);
	g_return_if_fail (spell != NULL);

	if (gtk_text_buffer_get_char_count (GTK_TEXT_BUFFER (doc)) <= 0)
	{
		GtkWidget *statusbar;

		statusbar = gedit_window_get_statusbar (priv->window);
		gedit_statusbar_flash_message (GEDIT_STATUSBAR (statusbar),
					       priv->message_cid,
					       _("The document is empty."));

		return;
	}

	if (!gtk_text_buffer_get_selection_bounds (GTK_TEXT_BUFFER (doc),
						   &start,
						   &end))
	{
		/* no selection, get the whole doc */
		gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (doc),
					    &start,
					    &end);
	}

	set_check_range (doc, &start, &end);

	word = get_next_misspelled_word (view);
	if (word == NULL)
	{
		GtkWidget *statusbar;

		statusbar = gedit_window_get_statusbar (priv->window);
		gedit_statusbar_flash_message (GEDIT_STATUSBAR (statusbar),
					       priv->message_cid,
					       _("No misspelled words"));

		return;
	}

	data_dir = peas_extension_base_get_data_dir (PEAS_EXTENSION_BASE (plugin));
	dlg = gedit_spell_checker_dialog_new_from_spell_checker (spell, data_dir);
	g_free (data_dir);
	gtk_window_set_modal (GTK_WINDOW (dlg), TRUE);
	gtk_window_set_transient_for (GTK_WINDOW (dlg),
				      GTK_WINDOW (priv->window));

	g_signal_connect (dlg, "ignore", G_CALLBACK (ignore_cb), view);
	g_signal_connect (dlg, "ignore_all", G_CALLBACK (ignore_cb), view);

	g_signal_connect (dlg, "change", G_CALLBACK (change_cb), view);
	g_signal_connect (dlg, "change_all", G_CALLBACK (change_all_cb), view);

	g_signal_connect (dlg, "add_word_to_personal", G_CALLBACK (add_word_cb), view);

	gedit_spell_checker_dialog_set_misspelled_word (GEDIT_SPELL_CHECKER_DIALOG (dlg),
							word,
							-1);

	g_free (word);

	gtk_widget_show (dlg);
}

static void
set_auto_spell (GeditWindow   *window,
                GeditView     *view,
                gboolean       active)
{
	GeditAutomaticSpellChecker *autospell;
	GeditSpellChecker *spell;
	GeditDocument *doc;

	doc = GEDIT_DOCUMENT (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)));

	spell = get_spell_checker_from_document (doc);
	g_return_if_fail (spell != NULL);

	autospell = gedit_automatic_spell_checker_get_from_document (doc);

	if (active)
	{
		if (autospell == NULL)
		{
			autospell = gedit_automatic_spell_checker_new (doc, spell);
			gedit_automatic_spell_checker_attach_view (autospell, view);
			gedit_automatic_spell_checker_recheck_all (autospell);
		}
	}
	else
	{
		if (autospell != NULL)
			gedit_automatic_spell_checker_free (autospell);
	}
}

static void
auto_spell_cb (GSimpleAction  *action,
               GVariant       *state,
               gpointer        data)
{
	GeditSpellPlugin *plugin = GEDIT_SPELL_PLUGIN (data);
	GeditSpellPluginPrivate *priv = plugin->priv;
	GeditDocument *doc;
	GeditView *view;
	gboolean active;

	gedit_debug (DEBUG_PLUGINS);

	active = g_variant_get_boolean (state);

	gedit_debug_message (DEBUG_PLUGINS, active ? "Auto Spell activated" : "Auto Spell deactivated");

	view = gedit_window_get_active_view (priv->window);
	if (view == NULL)
		return;

	doc = GEDIT_DOCUMENT (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)));

	gedit_document_set_metadata (doc,
				     GEDIT_METADATA_ATTRIBUTE_SPELL_ENABLED,
				     active ? "1" : NULL, NULL);

	set_auto_spell (priv->window, view, active);
	g_simple_action_set_state (action, g_variant_new_boolean (active));
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
	                                                 "check_spell");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (check_spell_action),
	                             (view != NULL) &&
	                             gtk_text_view_get_editable (GTK_TEXT_VIEW (view)));

	config_spell_action = g_action_map_lookup_action (G_ACTION_MAP (priv->window),
	                                                  "config_spell");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (config_spell_action),
	                             (view != NULL) &&
	                             gtk_text_view_get_editable (GTK_TEXT_VIEW (view)));

	auto_spell_action = g_action_map_lookup_action (G_ACTION_MAP (priv->window),
	                                                "auto_spell");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (auto_spell_action),
	                             (view != NULL) &&
	                             gtk_text_view_get_editable (GTK_TEXT_VIEW (view)));

	if (view != NULL)
	{
		GeditDocument *doc;
		GeditTab *tab;
		GeditTabState state;
		gboolean autospell;

		doc = GEDIT_DOCUMENT (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)));
		tab = gedit_window_get_active_tab (priv->window);
		state = gedit_tab_get_state (tab);
		autospell = (doc != NULL &&
		             gedit_automatic_spell_checker_get_from_document (doc) != NULL);

		/* If the document is loading we can't get the metadata so we
		   endup with an useless speller */
		if (state == GEDIT_TAB_STATE_NORMAL)
		{
			g_action_change_state (auto_spell_action, g_variant_new_boolean (autospell));
		}

		g_simple_action_set_enabled (G_SIMPLE_ACTION (check_spell_action),
		                             gtk_text_buffer_get_char_count (GTK_TEXT_BUFFER (doc)) > 0);
	}
}

static void
set_auto_spell_from_metadata (GeditSpellPlugin *plugin,
			      GeditView        *view)
{
	gboolean active = FALSE;
	gchar *active_str;
	GeditDocument *doc;
	GeditDocument *active_doc;

	doc = GEDIT_DOCUMENT (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)));
	active_str = gedit_document_get_metadata (doc,
						  GEDIT_METADATA_ATTRIBUTE_SPELL_ENABLED);

	if (active_str)
	{
		active = *active_str == '1';

		g_free (active_str);
	}

	set_auto_spell (plugin->priv->window, view, active);

	/* In case that the doc is the active one we mark the spell action */
	active_doc = gedit_window_get_active_document (plugin->priv->window);

	if (active_doc == doc)
	{
		GAction *action;

		action = g_action_map_lookup_action (G_ACTION_MAP (plugin->priv->window),
		                                     "auto_spell");
		g_action_change_state (action, g_variant_new_boolean (active));
	}
}

static void
on_document_loaded (GeditDocument    *doc,
		    const GError     *error,
		    GeditSpellPlugin *plugin)
{
	if (error == NULL)
	{
		GeditSpellChecker *spell;
		GeditView *view;

		spell = GEDIT_SPELL_CHECKER (g_object_get_qdata (G_OBJECT (doc),
								 spell_checker_id));
		if (spell != NULL)
		{
			set_language_from_metadata (spell, doc);
		}

		view = GEDIT_VIEW (g_object_get_data (G_OBJECT (doc), GEDIT_AUTOMATIC_SPELL_VIEW));

		set_auto_spell_from_metadata (plugin, view);
	}
}

static void
on_document_saved (GeditDocument    *doc,
		   const GError     *error,
		   GeditSpellPlugin *plugin)
{
	GeditAutomaticSpellChecker *autospell;
	GeditSpellChecker *spell;
	const gchar *key;

	if (error != NULL)
	{
		return;
	}

	/* Make sure to save the metadata here too */
	autospell = gedit_automatic_spell_checker_get_from_document (doc);
	spell = GEDIT_SPELL_CHECKER (g_object_get_qdata (G_OBJECT (doc), spell_checker_id));

	if (spell != NULL)
	{
		key = gedit_spell_checker_language_to_key (gedit_spell_checker_get_language (spell));
	}
	else
	{
		key = NULL;
	}

	gedit_document_set_metadata (doc,
	                             GEDIT_METADATA_ATTRIBUTE_SPELL_ENABLED,
	                             autospell != NULL ? "1" : NULL,
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
	GeditDocument *doc;

	view = gedit_tab_get_view (tab);
	doc = GEDIT_DOCUMENT (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)));

	/* we need to pass the view with the document as there is no way to
	   attach the view to the automatic spell checker. */
	g_object_set_data (G_OBJECT (doc), GEDIT_AUTOMATIC_SPELL_VIEW, view);

	g_signal_connect (doc, "loaded",
			  G_CALLBACK (on_document_loaded),
			  plugin);

	g_signal_connect (doc, "saved",
			  G_CALLBACK (on_document_saved),
			  plugin);
}

static void
tab_removed_cb (GeditWindow      *window,
		GeditTab         *tab,
		GeditSpellPlugin *plugin)
{
	GeditDocument *doc;

	doc = gedit_tab_get_document (tab);
	g_object_set_data (G_OBJECT (doc), GEDIT_AUTOMATIC_SPELL_VIEW, NULL);

	g_signal_handlers_disconnect_by_func (doc, on_document_loaded, plugin);
	g_signal_handlers_disconnect_by_func (doc, on_document_saved, plugin);
}

static void
gedit_spell_plugin_activate (GeditWindowActivatable *activatable)
{
	GeditSpellPlugin *plugin;
	GeditSpellPluginPrivate *priv;
	GMenuItem *item;
	GList *views, *l;

	gedit_debug (DEBUG_PLUGINS);

	plugin = GEDIT_SPELL_PLUGIN (activatable);
	priv = plugin->priv;

	g_action_map_add_action_entries (G_ACTION_MAP (priv->window),
	                                 action_entries,
	                                 G_N_ELEMENTS (action_entries),
	                                 activatable);

	priv->menu = gedit_window_activatable_extend_gear_menu (activatable,
	                                                        "ext5");

	item = g_menu_item_new (_("_Check Spelling..."), "win.check_spell");
	gedit_menu_extension_append_menu_item (priv->menu, item);
	g_object_unref (item);

	item = g_menu_item_new (_("Set _Language..."), "win.config_spell");
	gedit_menu_extension_append_menu_item (priv->menu, item);
	g_object_unref (item);

	item = g_menu_item_new (_("_Highlight Misspelled Words"), "win.auto_spell");
	gedit_menu_extension_append_menu_item (priv->menu, item);
	g_object_unref (item);

	priv->message_cid = gtk_statusbar_get_context_id (GTK_STATUSBAR (gedit_window_get_statusbar (priv->window)),
	                                                  "spell_plugin_message");

	update_ui (plugin);

	views = gedit_window_get_views (priv->window);
	for (l = views; l != NULL; l = g_list_next (l))
	{
		GeditView *view = GEDIT_VIEW (l->data);

		set_auto_spell_from_metadata (plugin, view);
	}

	priv->tab_added_id =
		g_signal_connect (priv->window, "tab-added",
				  G_CALLBACK (tab_added_cb), activatable);
	priv->tab_removed_id =
		g_signal_connect (priv->window, "tab-removed",
				  G_CALLBACK (tab_removed_cb), activatable);
}

static void
gedit_spell_plugin_deactivate (GeditWindowActivatable *activatable)
{
	GeditSpellPluginPrivate *priv;

	gedit_debug (DEBUG_PLUGINS);

	priv = GEDIT_SPELL_PLUGIN (activatable)->priv;

	g_action_map_remove_action (G_ACTION_MAP (priv->window),
	                            "check_spell");
	g_action_map_remove_action (G_ACTION_MAP (priv->window),
	                            "config_spell");
	g_action_map_remove_action (G_ACTION_MAP (priv->window),
	                            "auto_spell");

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
		spell_checker_id = g_quark_from_string ("GeditSpellCheckerID");

	if (check_range_id == 0)
		check_range_id = g_quark_from_string ("CheckRangeID");

	g_object_class_override_property (object_class, PROP_WINDOW, "window");

	g_type_class_add_private (klass, sizeof (GeditSpellPluginPrivate));
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

/* ex:ts=8:noet: */
