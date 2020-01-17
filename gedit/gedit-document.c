/*
 * gedit-document.c
 * This file is part of gedit
 *
 * Copyright (C) 1998, 1999 Alex Roberts, Evan Lawrence
 * Copyright (C) 2000, 2001 Chema Celorio, Paolo Maggi
 * Copyright (C) 2002-2005 Paolo Maggi
 * Copyright (C) 2014-2015 Sébastien Wilmet
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

#include "gedit-document.h"
#include "gedit-document-private.h"

#include <string.h>
#include <glib/gi18n.h>
#include <tepl/tepl.h>

#include "gedit-settings.h"
#include "gedit-debug.h"
#include "gedit-utils.h"

#define NO_LANGUAGE_NAME "_NORMAL_"

static void	gedit_document_loaded_real	(GeditDocument *doc);

static void	gedit_document_saved_real	(GeditDocument *doc);

static void	set_content_type		(GeditDocument *doc,
						 const gchar   *content_type);

typedef struct
{
	GtkSourceFile *file;

	/* Used only internally for the TeplFileMetadata. */
	TeplFile *tepl_file;

	GSettings   *editor_settings;

	gint 	     untitled_number;

	gchar	    *content_type;

	GDateTime   *time_of_last_save_or_load;

	/* The search context for the incremental search, or the search and
	 * replace. They are mutually exclusive.
	 */
	GtkSourceSearchContext *search_context;

	guint user_action;

	guint language_set_by_user : 1;

	/* The search is empty if there is no search context, or if the
	 * search text is empty. It is used for the sensitivity of some menu
	 * actions.
	 */
	guint empty_search : 1;

	/* Create file if location points to a non existing file (for example
	 * when opened from the command line).
	 */
	guint create : 1;
} GeditDocumentPrivate;

enum
{
	PROP_0,
	PROP_SHORTNAME,
	PROP_CONTENT_TYPE,
	PROP_MIME_TYPE,
	PROP_EMPTY_SEARCH,
	LAST_PROP
};

static GParamSpec *properties[LAST_PROP];

enum
{
	CURSOR_MOVED,
	LOAD,
	LOADED,
	SAVE,
	SAVED,
	LAST_SIGNAL
};

static guint document_signals[LAST_SIGNAL];

static GHashTable *allocated_untitled_numbers = NULL;

G_DEFINE_TYPE_WITH_PRIVATE (GeditDocument, gedit_document, GTK_SOURCE_TYPE_BUFFER)

static gint
get_untitled_number (void)
{
	gint i = 1;

	if (allocated_untitled_numbers == NULL)
		allocated_untitled_numbers = g_hash_table_new (NULL, NULL);

	g_return_val_if_fail (allocated_untitled_numbers != NULL, -1);

	while (TRUE)
	{
		if (g_hash_table_lookup (allocated_untitled_numbers, GINT_TO_POINTER (i)) == NULL)
		{
			g_hash_table_insert (allocated_untitled_numbers,
					     GINT_TO_POINTER (i),
					     GINT_TO_POINTER (i));

			return i;
		}

		++i;
	}
}

static void
release_untitled_number (gint n)
{
	g_return_if_fail (allocated_untitled_numbers != NULL);

	g_hash_table_remove (allocated_untitled_numbers, GINT_TO_POINTER (n));
}

static void
update_time_of_last_save_or_load (GeditDocument *doc)
{
	GeditDocumentPrivate *priv = gedit_document_get_instance_private (doc);

	if (priv->time_of_last_save_or_load != NULL)
	{
		g_date_time_unref (priv->time_of_last_save_or_load);
	}

	priv->time_of_last_save_or_load = g_date_time_new_now_utc ();
}

static const gchar *
get_language_string (GeditDocument *doc)
{
	GtkSourceLanguage *lang = gedit_document_get_language (doc);

	return lang != NULL ? gtk_source_language_get_id (lang) : NO_LANGUAGE_NAME;
}

static void
save_metadata (GeditDocument *doc)
{
	GeditDocumentPrivate *priv;
	const gchar *language = NULL;
	GtkTextIter iter;
	gchar *position;

	priv = gedit_document_get_instance_private (doc);
	if (priv->language_set_by_user)
	{
		language = get_language_string (doc);
	}

	gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (doc),
					  &iter,
					  gtk_text_buffer_get_insert (GTK_TEXT_BUFFER (doc)));

	position = g_strdup_printf ("%d", gtk_text_iter_get_offset (&iter));

	if (language == NULL)
	{
		gedit_document_set_metadata (doc,
					     GEDIT_METADATA_ATTRIBUTE_POSITION, position,
					     NULL);
	}
	else
	{
		gedit_document_set_metadata (doc,
					     GEDIT_METADATA_ATTRIBUTE_POSITION, position,
					     GEDIT_METADATA_ATTRIBUTE_LANGUAGE, language,
					     NULL);
	}

	g_free (position);
}

static void
gedit_document_dispose (GObject *object)
{
	GeditDocument *doc = GEDIT_DOCUMENT (object);
	GeditDocumentPrivate *priv = gedit_document_get_instance_private (doc);

	gedit_debug (DEBUG_DOCUMENT);

	/* Metadata must be saved here and not in finalize because the language
	 * is gone by the time finalize runs.
	 */
	if (priv->tepl_file != NULL)
	{
		save_metadata (doc);

		g_object_unref (priv->tepl_file);
		priv->tepl_file = NULL;
	}

	g_clear_object (&priv->file);
	g_clear_object (&priv->editor_settings);
	g_clear_object (&priv->search_context);

	G_OBJECT_CLASS (gedit_document_parent_class)->dispose (object);
}

static void
gedit_document_finalize (GObject *object)
{
	GeditDocumentPrivate *priv;

	gedit_debug (DEBUG_DOCUMENT);

	priv = gedit_document_get_instance_private (GEDIT_DOCUMENT (object));

	if (priv->untitled_number > 0)
	{
		release_untitled_number (priv->untitled_number);
	}

	g_free (priv->content_type);

	if (priv->time_of_last_save_or_load != NULL)
	{
		g_date_time_unref (priv->time_of_last_save_or_load);
	}

	G_OBJECT_CLASS (gedit_document_parent_class)->finalize (object);
}

static void
gedit_document_get_property (GObject    *object,
			     guint       prop_id,
			     GValue     *value,
			     GParamSpec *pspec)
{
	GeditDocument *doc = GEDIT_DOCUMENT (object);
	GeditDocumentPrivate *priv;

	priv = gedit_document_get_instance_private (doc);

	switch (prop_id)
	{
		case PROP_SHORTNAME:
			g_value_take_string (value, gedit_document_get_short_name_for_display (doc));
			break;

		case PROP_CONTENT_TYPE:
			g_value_take_string (value, gedit_document_get_content_type (doc));
			break;

		case PROP_MIME_TYPE:
			g_value_take_string (value, gedit_document_get_mime_type (doc));
			break;

		case PROP_EMPTY_SEARCH:
			g_value_set_boolean (value, priv->empty_search);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gedit_document_set_property (GObject      *object,
			     guint         prop_id,
			     const GValue *value,
			     GParamSpec   *pspec)
{
	GeditDocument *doc = GEDIT_DOCUMENT (object);

	switch (prop_id)
	{
		case PROP_CONTENT_TYPE:
			set_content_type (doc, g_value_get_string (value));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gedit_document_begin_user_action (GtkTextBuffer *buffer)
{
	GeditDocumentPrivate *priv;

	priv = gedit_document_get_instance_private (GEDIT_DOCUMENT (buffer));

	++priv->user_action;

	if (GTK_TEXT_BUFFER_CLASS (gedit_document_parent_class)->begin_user_action != NULL)
	{
		GTK_TEXT_BUFFER_CLASS (gedit_document_parent_class)->begin_user_action (buffer);
	}
}

static void
gedit_document_end_user_action (GtkTextBuffer *buffer)
{
	GeditDocumentPrivate *priv;

	priv = gedit_document_get_instance_private (GEDIT_DOCUMENT (buffer));

	--priv->user_action;

	if (GTK_TEXT_BUFFER_CLASS (gedit_document_parent_class)->end_user_action != NULL)
	{
		GTK_TEXT_BUFFER_CLASS (gedit_document_parent_class)->end_user_action (buffer);
	}
}

static void
gedit_document_mark_set (GtkTextBuffer     *buffer,
                         const GtkTextIter *iter,
                         GtkTextMark       *mark)
{
	GeditDocument *doc = GEDIT_DOCUMENT (buffer);
	GeditDocumentPrivate *priv;

	priv = gedit_document_get_instance_private (doc);

	if (GTK_TEXT_BUFFER_CLASS (gedit_document_parent_class)->mark_set != NULL)
	{
		GTK_TEXT_BUFFER_CLASS (gedit_document_parent_class)->mark_set (buffer, iter, mark);
	}

	if (mark == gtk_text_buffer_get_insert (buffer) && (priv->user_action == 0))
	{
		g_signal_emit (doc, document_signals[CURSOR_MOVED], 0);
	}
}

static void
gedit_document_changed (GtkTextBuffer *buffer)
{
	g_signal_emit (GEDIT_DOCUMENT (buffer), document_signals[CURSOR_MOVED], 0);

	GTK_TEXT_BUFFER_CLASS (gedit_document_parent_class)->changed (buffer);
}

static void
gedit_document_constructed (GObject *object)
{
	GeditDocument *doc = GEDIT_DOCUMENT (object);
	GeditDocumentPrivate *priv;

	priv = gedit_document_get_instance_private (doc);

	/* Bind construct properties. */
	g_settings_bind (priv->editor_settings,
			 GEDIT_SETTINGS_ENSURE_TRAILING_NEWLINE,
			 doc,
			 "implicit-trailing-newline",
			 G_SETTINGS_BIND_GET | G_SETTINGS_BIND_NO_SENSITIVITY);

	G_OBJECT_CLASS (gedit_document_parent_class)->constructed (object);
}

static void
gedit_document_class_init (GeditDocumentClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkTextBufferClass *buf_class = GTK_TEXT_BUFFER_CLASS (klass);

	object_class->dispose = gedit_document_dispose;
	object_class->finalize = gedit_document_finalize;
	object_class->get_property = gedit_document_get_property;
	object_class->set_property = gedit_document_set_property;
	object_class->constructed = gedit_document_constructed;

	buf_class->begin_user_action = gedit_document_begin_user_action;
	buf_class->end_user_action = gedit_document_end_user_action;
	buf_class->mark_set = gedit_document_mark_set;
	buf_class->changed = gedit_document_changed;

	klass->loaded = gedit_document_loaded_real;
	klass->saved = gedit_document_saved_real;

	/**
	 * GeditDocument:shortname:
	 *
	 * The document's short name.
	 */
	properties[PROP_SHORTNAME] =
		g_param_spec_string ("shortname",
		                     "Short Name",
		                     "The document's short name",
		                     NULL,
		                     G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

	/**
	 * GeditDocument:content-type:
	 *
	 * The document's content type.
	 */
	properties[PROP_CONTENT_TYPE] =
		g_param_spec_string ("content-type",
		                     "Content Type",
		                     "The document's Content Type",
		                     NULL,
		                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	/**
	 * GeditDocument:mime-type:
	 *
	 * The document's MIME type.
	 */
	properties[PROP_MIME_TYPE] =
		g_param_spec_string ("mime-type",
		                     "MIME Type",
		                     "The document's MIME Type",
		                     "text/plain",
		                     G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

	/**
	 * GeditDocument:empty-search:
	 *
	 * <warning>
	 * The property is used internally by gedit. It must not be used in a
	 * gedit plugin. The property can be modified or removed at any time.
	 * </warning>
	 */
	properties[PROP_EMPTY_SEARCH] =
		g_param_spec_boolean ("empty-search",
		                      "Empty search",
		                      "Whether the search is empty",
		                      TRUE,
		                      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties (object_class, LAST_PROP, properties);

	/* This signal is used to update the cursor position in the statusbar,
	 * it's emitted either when the insert mark is moved explicitely or
	 * when the buffer changes (insert/delete).
	 * FIXME When the replace_all was implemented in gedit, this signal was
	 * not emitted during the replace_all to improve performance. Now the
	 * replace_all is implemented in GtkSourceView, so the signal is
	 * emitted.
	 */
	document_signals[CURSOR_MOVED] =
		g_signal_new ("cursor-moved",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GeditDocumentClass, cursor_moved),
			      NULL, NULL, NULL,
			      G_TYPE_NONE,
			      0);

	/**
	 * GeditDocument::load:
	 * @document: the #GeditDocument.
	 *
	 * The "load" signal is emitted at the beginning of a file loading.
	 *
	 * Before gedit 3.14 this signal contained parameters to configure the
	 * file loading (the location, encoding, etc). Plugins should not need
	 * those parameters.
	 */
	document_signals[LOAD] =
		g_signal_new ("load",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GeditDocumentClass, load),
			      NULL, NULL, NULL,
			      G_TYPE_NONE, 0);

	/**
	 * GeditDocument::loaded:
	 * @document: the #GeditDocument.
	 *
	 * The "loaded" signal is emitted at the end of a successful file
	 * loading.
	 *
	 * Before gedit 3.14 this signal contained a #GError parameter, and the
	 * signal was also emitted if an error occurred. Plugins should not need
	 * the error parameter.
	 */
	document_signals[LOADED] =
		g_signal_new ("loaded",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GeditDocumentClass, loaded),
			      NULL, NULL, NULL,
			      G_TYPE_NONE, 0);

	/**
	 * GeditDocument::save:
	 * @document: the #GeditDocument.
	 *
	 * The "save" signal is emitted at the beginning of a file saving.
	 *
	 * Before gedit 3.14 this signal contained parameters to configure the
	 * file saving (the location, encoding, etc). Plugins should not need
	 * those parameters.
	 */
	document_signals[SAVE] =
		g_signal_new ("save",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GeditDocumentClass, save),
			      NULL, NULL, NULL,
			      G_TYPE_NONE, 0);

	/**
	 * GeditDocument::saved:
	 * @document: the #GeditDocument.
	 *
	 * The "saved" signal is emitted at the end of a successful file saving.
	 *
	 * Before gedit 3.14 this signal contained a #GError parameter, and the
	 * signal was also emitted if an error occurred. To save a document, a
	 * plugin can use the gedit_commands_save_document_async() function and
	 * get the result of the operation with
	 * gedit_commands_save_document_finish().
	 */
	document_signals[SAVED] =
		g_signal_new ("saved",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GeditDocumentClass, saved),
			      NULL, NULL, NULL,
			      G_TYPE_NONE, 0);
}

static void
set_language (GeditDocument     *doc,
              GtkSourceLanguage *lang,
              gboolean           set_by_user)
{
	GeditDocumentPrivate *priv;
	GtkSourceLanguage *old_lang;

	gedit_debug (DEBUG_DOCUMENT);

	priv = gedit_document_get_instance_private (doc);

	old_lang = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (doc));

	if (old_lang == lang)
	{
		return;
	}

	gtk_source_buffer_set_language (GTK_SOURCE_BUFFER (doc), lang);

	if (set_by_user)
	{
		const gchar *language = get_language_string (doc);

		gedit_document_set_metadata (doc,
					     GEDIT_METADATA_ATTRIBUTE_LANGUAGE, language,
					     NULL);
	}

	priv->language_set_by_user = set_by_user;
}

static void
save_encoding_metadata (GeditDocument *doc)
{
	GeditDocumentPrivate *priv;
	const GtkSourceEncoding *encoding;
	const gchar *charset;

	gedit_debug (DEBUG_DOCUMENT);

	priv = gedit_document_get_instance_private (doc);

	encoding = gtk_source_file_get_encoding (priv->file);

	if (encoding == NULL)
	{
		encoding = gtk_source_encoding_get_utf8 ();
	}

	charset = gtk_source_encoding_get_charset (encoding);

	gedit_document_set_metadata (doc,
				     GEDIT_METADATA_ATTRIBUTE_ENCODING, charset,
				     NULL);
}

static GtkSourceStyleScheme *
get_default_style_scheme (GSettings *editor_settings)
{
	GtkSourceStyleSchemeManager *manager;
	gchar *scheme_id;
	GtkSourceStyleScheme *def_style;

	manager = gtk_source_style_scheme_manager_get_default ();
	scheme_id = g_settings_get_string (editor_settings, GEDIT_SETTINGS_SCHEME);
	def_style = gtk_source_style_scheme_manager_get_scheme (manager, scheme_id);

	if (def_style == NULL)
	{
		g_warning ("Default style scheme '%s' cannot be found, falling back to 'classic' style scheme ", scheme_id);

		def_style = gtk_source_style_scheme_manager_get_scheme (manager, "classic");

		if (def_style == NULL)
		{
			g_warning ("Style scheme 'classic' cannot be found, check your GtkSourceView installation.");
		}
	}

	g_free (scheme_id);

	return def_style;
}

static GtkSourceLanguage *
guess_language (GeditDocument *doc)
{
	GeditDocumentPrivate *priv;
	gchar *data;
	GtkSourceLanguageManager *manager = gtk_source_language_manager_get_default ();
	GtkSourceLanguage *language = NULL;

	priv = gedit_document_get_instance_private (doc);

	data = gedit_document_get_metadata (doc, GEDIT_METADATA_ATTRIBUTE_LANGUAGE);

	if (data != NULL)
	{
		gedit_debug_message (DEBUG_DOCUMENT, "Language from metadata: %s", data);

		if (!g_str_equal (data, NO_LANGUAGE_NAME))
		{
			language = gtk_source_language_manager_get_language (manager, data);
		}

		g_free (data);
	}
	else
	{
		GFile *location;
		gchar *basename = NULL;

		location = gtk_source_file_get_location (priv->file);
		gedit_debug_message (DEBUG_DOCUMENT, "Sniffing Language");

		if (location != NULL)
		{
			basename = g_file_get_basename (location);
		}

		language = gtk_source_language_manager_guess_language (manager,
								       basename,
								       priv->content_type);

		g_free (basename);
	}

	return language;
}

static void
on_content_type_changed (GeditDocument *doc,
			 GParamSpec    *pspec,
			 gpointer       useless)
{
	GeditDocumentPrivate *priv;

	priv = gedit_document_get_instance_private (doc);

	if (!priv->language_set_by_user)
	{
		GtkSourceLanguage *language = guess_language (doc);

		gedit_debug_message (DEBUG_DOCUMENT, "Language: %s",
				     language != NULL ? gtk_source_language_get_name (language) : "None");

		set_language (doc, language, FALSE);
	}
}

static gchar *
get_default_content_type (void)
{
	return g_content_type_from_mime_type ("text/plain");
}

static void
on_location_changed (GtkSourceFile *file,
		     GParamSpec    *pspec,
		     GeditDocument *doc)
{
	GeditDocumentPrivate *priv;
	GFile *location;

	gedit_debug (DEBUG_DOCUMENT);

	priv = gedit_document_get_instance_private (doc);

	location = gtk_source_file_get_location (file);

	if (location != NULL && priv->untitled_number > 0)
	{
		release_untitled_number (priv->untitled_number);
		priv->untitled_number = 0;
	}

	g_object_notify_by_pspec (G_OBJECT (doc), properties[PROP_SHORTNAME]);
}

static void
on_tepl_location_changed (TeplFile      *file,
			  GParamSpec    *pspec,
			  GeditDocument *doc)
{
	TeplFileMetadata *metadata;
	GError *error = NULL;

	/* Load metadata for this location: we load sync since metadata is
	 * always local so it should be fast and we need the information
	 * right after the location was set.
	 * TODO: do async I/O for the metadata.
	 */
	metadata = tepl_file_get_file_metadata (file);
	tepl_file_metadata_load (metadata, NULL, &error);

	if (error != NULL)
	{
		/* Do not complain about metadata if we are opening a
		 * non existing file.
		 * TODO: we should know beforehand whether the file exists or
		 * not, and load the metadata only when needed (after loading
		 * the file content, for example).
		 */
		if (!g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_ISDIR) &&
		    !g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOTDIR) &&
		    !g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT) &&
		    !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
		{
			g_warning ("Loading metadata failed: %s", error->message);
		}

		g_clear_error (&error);
	}
}

static void
gedit_document_init (GeditDocument *doc)
{
	GeditDocumentPrivate *priv;
	GtkSourceStyleScheme *style_scheme;

	gedit_debug (DEBUG_DOCUMENT);

	priv = gedit_document_get_instance_private (doc);

	priv->editor_settings = g_settings_new ("com.bwhmather.bedit.preferences.editor");
	priv->untitled_number = get_untitled_number ();
	priv->content_type = get_default_content_type ();
	priv->language_set_by_user = FALSE;
	priv->empty_search = TRUE;

	update_time_of_last_save_or_load (doc);

	priv->file = gtk_source_file_new ();

	g_signal_connect_object (priv->file,
				 "notify::location",
				 G_CALLBACK (on_location_changed),
				 doc,
				 0);

	priv->tepl_file = tepl_file_new ();

	g_signal_connect_object (priv->tepl_file,
				 "notify::location",
				 G_CALLBACK (on_tepl_location_changed),
				 doc,
				 0);

	/* For using TeplFileMetadata we only need the TeplFile:location. */
	g_object_bind_property (priv->file, "location",
				priv->tepl_file, "location",
				G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

	g_settings_bind (priv->editor_settings,
	                 GEDIT_SETTINGS_MAX_UNDO_ACTIONS,
	                 doc,
	                 "max-undo-levels",
	                 G_SETTINGS_BIND_GET | G_SETTINGS_BIND_NO_SENSITIVITY);

	g_settings_bind (priv->editor_settings,
			 GEDIT_SETTINGS_SYNTAX_HIGHLIGHTING,
			 doc,
			 "highlight-syntax",
			 G_SETTINGS_BIND_GET | G_SETTINGS_BIND_NO_SENSITIVITY);

	g_settings_bind (priv->editor_settings,
	                 GEDIT_SETTINGS_BRACKET_MATCHING,
	                 doc,
	                 "highlight-matching-brackets",
	                 G_SETTINGS_BIND_GET | G_SETTINGS_BIND_NO_SENSITIVITY);

	style_scheme = get_default_style_scheme (priv->editor_settings);
	if (style_scheme != NULL)
	{
		gtk_source_buffer_set_style_scheme (GTK_SOURCE_BUFFER (doc), style_scheme);
	}

	g_signal_connect (doc,
			  "notify::content-type",
			  G_CALLBACK (on_content_type_changed),
			  NULL);
}

GeditDocument *
gedit_document_new (void)
{
	return g_object_new (GEDIT_TYPE_DOCUMENT, NULL);
}

static gchar *
get_content_type_from_content (GeditDocument *doc)
{
	gchar *content_type;
	gchar *data;
	GtkTextBuffer *buffer;
	GtkTextIter start;
	GtkTextIter end;

	buffer = GTK_TEXT_BUFFER (doc);

	gtk_text_buffer_get_start_iter (buffer, &start);
	end = start;
	gtk_text_iter_forward_chars (&end, 255);

	data = gtk_text_buffer_get_text (buffer, &start, &end, TRUE);

	content_type = g_content_type_guess (NULL,
	                                     (const guchar *)data,
	                                     strlen (data),
	                                     NULL);

	g_free (data);

	return content_type;
}

static void
set_content_type_no_guess (GeditDocument *doc,
			   const gchar   *content_type)
{
	GeditDocumentPrivate *priv;
	gchar *dupped_content_type;

	gedit_debug (DEBUG_DOCUMENT);

	priv = gedit_document_get_instance_private (doc);

	if (priv->content_type != NULL &&
	    content_type != NULL &&
	    g_str_equal (priv->content_type, content_type))
	{
		return;
	}

	g_free (priv->content_type);

	/* For compression types, we try to just guess from the content */
	if (gedit_utils_get_compression_type_from_content_type (content_type) !=
	    GTK_SOURCE_COMPRESSION_TYPE_NONE)
	{
		dupped_content_type = get_content_type_from_content (doc);
	}
	else
	{
		dupped_content_type = g_strdup (content_type);
	}

	if (dupped_content_type == NULL ||
	    g_content_type_is_unknown (dupped_content_type))
	{
		priv->content_type = get_default_content_type ();
		g_free (dupped_content_type);
	}
	else
	{
		priv->content_type = dupped_content_type;
	}

	g_object_notify_by_pspec (G_OBJECT (doc), properties[PROP_CONTENT_TYPE]);
}

static void
set_content_type (GeditDocument *doc,
		  const gchar   *content_type)
{
	GeditDocumentPrivate *priv;

	gedit_debug (DEBUG_DOCUMENT);

	priv = gedit_document_get_instance_private (doc);

	if (content_type == NULL)
	{
		GFile *location;
		gchar *guessed_type = NULL;

		/* If content type is null, we guess from the filename */
		location = gtk_source_file_get_location (priv->file);
		if (location != NULL)
		{
			gchar *basename;

			basename = g_file_get_basename (location);
			guessed_type = g_content_type_guess (basename, NULL, 0, NULL);

			g_free (basename);
		}

		set_content_type_no_guess (doc, guessed_type);
		g_free (guessed_type);
	}
	else
	{
		set_content_type_no_guess (doc, content_type);
	}
}

/**
 * gedit_document_get_uri_for_display:
 * @doc: a #GeditDocument.
 *
 * Note: this never returns %NULL.
 **/
gchar *
gedit_document_get_uri_for_display (GeditDocument *doc)
{
	GeditDocumentPrivate *priv;
	GFile *location;

	g_return_val_if_fail (GEDIT_IS_DOCUMENT (doc), g_strdup (""));

	priv = gedit_document_get_instance_private (doc);

	location = gtk_source_file_get_location (priv->file);

	if (location == NULL)
	{
		return g_strdup_printf (_("Untitled Document %d"),
					priv->untitled_number);
	}
	else
	{
		return g_file_get_parse_name (location);
	}
}

/**
 * gedit_document_get_short_name_for_display:
 * @doc: a #GeditDocument.
 *
 * Note: this never returns %NULL.
 **/
gchar *
gedit_document_get_short_name_for_display (GeditDocument *doc)
{
	GeditDocumentPrivate *priv;
	GFile *location;

	g_return_val_if_fail (GEDIT_IS_DOCUMENT (doc), g_strdup (""));

	priv = gedit_document_get_instance_private (doc);

	location = gtk_source_file_get_location (priv->file);

	if (location == NULL)
	{
		return g_strdup_printf (_("Untitled Document %d"),
					priv->untitled_number);
	}
	else
	{
		return gedit_utils_basename_for_display (location);
	}
}

gchar *
gedit_document_get_content_type (GeditDocument *doc)
{
	GeditDocumentPrivate *priv;

	g_return_val_if_fail (GEDIT_IS_DOCUMENT (doc), NULL);

	priv = gedit_document_get_instance_private (doc);

	return g_strdup (priv->content_type);
}

/**
 * gedit_document_get_mime_type:
 * @doc: a #GeditDocument.
 *
 * Note: this never returns %NULL.
 **/
gchar *
gedit_document_get_mime_type (GeditDocument *doc)
{
	GeditDocumentPrivate *priv;

	g_return_val_if_fail (GEDIT_IS_DOCUMENT (doc), g_strdup ("text/plain"));

	priv = gedit_document_get_instance_private (doc);

	if (priv->content_type != NULL &&
	    !g_content_type_is_unknown (priv->content_type))
	{
		return g_content_type_get_mime_type (priv->content_type);
	}

	return g_strdup ("text/plain");
}

static void
loaded_query_info_cb (GFile         *location,
		      GAsyncResult  *result,
		      GeditDocument *doc)
{
	GFileInfo *info;
	GError *error = NULL;

	info = g_file_query_info_finish (location, result, &error);

	if (error != NULL)
	{
		/* Ignore not found error as it can happen when opening a
		 * non-existent file from the command line.
		 */
		if (error->domain != G_IO_ERROR ||
		    error->code != G_IO_ERROR_NOT_FOUND)
		{
			g_warning ("Document loading: query info error: %s", error->message);
		}

		g_error_free (error);
		error = NULL;
	}

	if (info != NULL &&
	    g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE))
	{
		const gchar *content_type;

		content_type = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);

		set_content_type (doc, content_type);
	}

	g_clear_object (&info);

	/* Async operation finished. */
	g_object_unref (doc);
}

static void
gedit_document_loaded_real (GeditDocument *doc)
{
	GeditDocumentPrivate *priv;
	GFile *location;

	priv = gedit_document_get_instance_private (doc);

	if (!priv->language_set_by_user)
	{
		GtkSourceLanguage *language = guess_language (doc);

		gedit_debug_message (DEBUG_DOCUMENT, "Language: %s",
				     language != NULL ? gtk_source_language_get_name (language) : "None");

		set_language (doc, language, FALSE);
	}

	update_time_of_last_save_or_load (doc);
	set_content_type (doc, NULL);

	location = gtk_source_file_get_location (priv->file);

	if (location != NULL)
	{
		/* Keep the doc alive during the async operation. */
		g_object_ref (doc);

		g_file_query_info_async (location,
					 G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
					 G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE,
					 G_FILE_QUERY_INFO_NONE,
					 G_PRIORITY_DEFAULT,
					 NULL,
					 (GAsyncReadyCallback) loaded_query_info_cb,
					 doc);
	}
}

static void
saved_query_info_cb (GFile         *location,
		     GAsyncResult  *result,
		     GeditDocument *doc)
{
	GeditDocumentPrivate *priv;
	GFileInfo *info;
	const gchar *content_type = NULL;
	GError *error = NULL;

	priv = gedit_document_get_instance_private (doc);

	info = g_file_query_info_finish (location, result, &error);

	if (error != NULL)
	{
		g_warning ("Document saving: query info error: %s", error->message);
		g_error_free (error);
		error = NULL;
	}

	if (info != NULL &&
	    g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE))
	{
		content_type = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);
	}

	set_content_type (doc, content_type);

	if (info != NULL)
	{
		/* content_type (owned by info) is no longer needed. */
		g_object_unref (info);
	}

	update_time_of_last_save_or_load (doc);

	priv->create = FALSE;

	save_encoding_metadata (doc);

	/* Async operation finished. */
	g_object_unref (doc);
}

static void
gedit_document_saved_real (GeditDocument *doc)
{
	GeditDocumentPrivate *priv;
	GFile *location;

	priv = gedit_document_get_instance_private (doc);

	location = gtk_source_file_get_location (priv->file);

	/* Keep the doc alive during the async operation. */
	g_object_ref (doc);

	g_file_query_info_async (location,
				 G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
				 G_FILE_QUERY_INFO_NONE,
				 G_PRIORITY_DEFAULT,
				 NULL,
				 (GAsyncReadyCallback) saved_query_info_cb,
				 doc);
}

gboolean
gedit_document_is_untouched (GeditDocument *doc)
{
	GeditDocumentPrivate *priv;
	GFile *location;

	g_return_val_if_fail (GEDIT_IS_DOCUMENT (doc), TRUE);

	priv = gedit_document_get_instance_private (doc);

	location = gtk_source_file_get_location (priv->file);

	return location == NULL && !gtk_text_buffer_get_modified (GTK_TEXT_BUFFER (doc));
}

gboolean
gedit_document_is_untitled (GeditDocument *doc)
{
	GeditDocumentPrivate *priv;

	g_return_val_if_fail (GEDIT_IS_DOCUMENT (doc), TRUE);

	priv = gedit_document_get_instance_private (doc);

	return gtk_source_file_get_location (priv->file) == NULL;
}

/*
 * Deletion and external modification is only checked for local files.
 */
gboolean
_gedit_document_needs_saving (GeditDocument *doc)
{
	GeditDocumentPrivate *priv;
	gboolean externally_modified = FALSE;
	gboolean deleted = FALSE;

	g_return_val_if_fail (GEDIT_IS_DOCUMENT (doc), FALSE);

	priv = gedit_document_get_instance_private (doc);

	if (gtk_text_buffer_get_modified (GTK_TEXT_BUFFER (doc)))
	{
		return TRUE;
	}

	if (gtk_source_file_is_local (priv->file))
	{
		gtk_source_file_check_file_on_disk (priv->file);
		externally_modified = gtk_source_file_is_externally_modified (priv->file);
		deleted = gtk_source_file_is_deleted (priv->file);
	}

	return (externally_modified || deleted) && !priv->create;
}

/* If @line is bigger than the lines of the document, the cursor is moved
 * to the last line and FALSE is returned.
 */
gboolean
gedit_document_goto_line (GeditDocument *doc,
			  gint           line)
{
	GtkTextIter iter;

	gedit_debug (DEBUG_DOCUMENT);

	g_return_val_if_fail (GEDIT_IS_DOCUMENT (doc), FALSE);
	g_return_val_if_fail (line >= -1, FALSE);

	gtk_text_buffer_get_iter_at_line (GTK_TEXT_BUFFER (doc),
					  &iter,
					  line);

	gtk_text_buffer_place_cursor (GTK_TEXT_BUFFER (doc), &iter);

	return gtk_text_iter_get_line (&iter) == line;
}

gboolean
gedit_document_goto_line_offset (GeditDocument *doc,
				 gint           line,
				 gint           line_offset)
{
	GtkTextIter iter;

	g_return_val_if_fail (GEDIT_IS_DOCUMENT (doc), FALSE);
	g_return_val_if_fail (line >= -1, FALSE);
	g_return_val_if_fail (line_offset >= -1, FALSE);

	gtk_text_buffer_get_iter_at_line_offset (GTK_TEXT_BUFFER (doc),
						 &iter,
						 line,
						 line_offset);

	gtk_text_buffer_place_cursor (GTK_TEXT_BUFFER (doc), &iter);

	return (gtk_text_iter_get_line (&iter) == line &&
		gtk_text_iter_get_line_offset (&iter) == line_offset);
}

/**
 * gedit_document_set_language:
 * @doc:
 * @lang: (allow-none):
 **/
void
gedit_document_set_language (GeditDocument     *doc,
			     GtkSourceLanguage *lang)
{
	g_return_if_fail (GEDIT_IS_DOCUMENT (doc));

	set_language (doc, lang, TRUE);
}

/**
 * gedit_document_get_language:
 * @doc:
 *
 * Return value: (transfer none):
 */
GtkSourceLanguage *
gedit_document_get_language (GeditDocument *doc)
{
	g_return_val_if_fail (GEDIT_IS_DOCUMENT (doc), NULL);

	return gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (doc));
}

glong
_gedit_document_get_seconds_since_last_save_or_load (GeditDocument *doc)
{
	GeditDocumentPrivate *priv;
	GDateTime *now;
	GTimeSpan n_microseconds;

	gedit_debug (DEBUG_DOCUMENT);

	g_return_val_if_fail (GEDIT_IS_DOCUMENT (doc), -1);

	priv = gedit_document_get_instance_private (doc);

	if (priv->time_of_last_save_or_load == NULL)
	{
		return -1;
	}

	now = g_date_time_new_now_utc ();
	if (now == NULL)
	{
		return -1;
	}

	n_microseconds = g_date_time_difference (now, priv->time_of_last_save_or_load);
	g_date_time_unref (now);

	return n_microseconds / (1000 * 1000);
}

/**
 * gedit_document_get_metadata:
 * @doc: a #GeditDocument
 * @key: name of the key
 *
 * Gets the metadata assigned to @key.
 *
 * Returns: the value assigned to @key. Free with g_free().
 */
gchar *
gedit_document_get_metadata (GeditDocument *doc,
			     const gchar   *key)
{
	GeditDocumentPrivate *priv;
	TeplFileMetadata *metadata;

	g_return_val_if_fail (GEDIT_IS_DOCUMENT (doc), NULL);
	g_return_val_if_fail (key != NULL, NULL);

	priv = gedit_document_get_instance_private (doc);

	if (priv->tepl_file == NULL)
	{
		return NULL;
	}

	metadata = tepl_file_get_file_metadata (priv->tepl_file);
	return tepl_file_metadata_get (metadata, key);
}

/**
 * gedit_document_set_metadata:
 * @doc: a #GeditDocument
 * @first_key: name of the first key to set
 * @...: (allow-none): value for the first key, followed optionally by more key/value pairs,
 * followed by %NULL.
 *
 * Sets metadata on a document.
 */
void
gedit_document_set_metadata (GeditDocument *doc,
			     const gchar   *first_key,
			     ...)
{
	GeditDocumentPrivate *priv;
	TeplFileMetadata *metadata;
	va_list var_args;
	const gchar *key;
	GError *error = NULL;

	g_return_if_fail (GEDIT_IS_DOCUMENT (doc));
	g_return_if_fail (first_key != NULL);

	priv = gedit_document_get_instance_private (doc);

	if (priv->tepl_file == NULL)
	{
		return;
	}

	metadata = tepl_file_get_file_metadata (priv->tepl_file);
	va_start (var_args, first_key);

	for (key = first_key; key != NULL; key = va_arg (var_args, const gchar *))
	{
		const gchar *value = va_arg (var_args, const gchar *);
		tepl_file_metadata_set (metadata, key, value);
	}

	va_end (var_args);

	/* We save synchronously since metadata is always local so it should be
	 * fast. Moreover this function can be called on application shutdown,
	 * when the main loop has already exited, so an async operation would
	 * not terminate.
	 * https://bugzilla.gnome.org/show_bug.cgi?id=736591
	 * TODO: do async I/O for the metadata.
	 */
	tepl_file_metadata_save (metadata, NULL, &error);

	if (error != NULL)
	{
		/* Do not complain about metadata if we are closing a document
		 * for a non existing file.
		 * TODO: we should know beforehand whether the file exists or
		 * not, and save the metadata only when needed (after saving the
		 * file content, for example).
		 */
		if (!g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT) &&
		    !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
		{
			g_warning ("Saving metadata failed: %s", error->message);
		}

		g_clear_error (&error);
	}
}

static void
update_empty_search (GeditDocument *doc)
{
	GeditDocumentPrivate *priv;
	gboolean new_value;

	priv = gedit_document_get_instance_private (doc);

	if (priv->search_context == NULL)
	{
		new_value = TRUE;
	}
	else
	{
		GtkSourceSearchSettings *search_settings;

		search_settings = gtk_source_search_context_get_settings (priv->search_context);

		new_value = gtk_source_search_settings_get_search_text (search_settings) == NULL;
	}

	if (priv->empty_search != new_value)
	{
		priv->empty_search = new_value;
		g_object_notify_by_pspec (G_OBJECT (doc), properties[PROP_EMPTY_SEARCH]);
	}
}

static void
connect_search_settings (GeditDocument *doc)
{
	GeditDocumentPrivate *priv;
	GtkSourceSearchSettings *search_settings;

	priv = gedit_document_get_instance_private (doc);

	search_settings = gtk_source_search_context_get_settings (priv->search_context);

	/* Note: the signal handler is never disconnected. If the search context
	 * changes its search settings, the old search settings will most
	 * probably be destroyed, anyway. So it shouldn't cause performance
	 * problems.
	 */
	g_signal_connect_object (search_settings,
				 "notify::search-text",
				 G_CALLBACK (update_empty_search),
				 doc,
				 G_CONNECT_SWAPPED);
}

/**
 * gedit_document_set_search_context:
 * @doc: a #GeditDocument
 * @search_context: (allow-none): the new #GtkSourceSearchContext
 *
 * Sets the new search context for the document. Use this function only when the
 * search occurrences are highlighted. So this function should not be used for
 * background searches. The purpose is to have only one highlighted search
 * context at a time in the document.
 *
 * After using this function, you should unref the @search_context. The @doc
 * should be the only owner of the @search_context, so that the Clear Highlight
 * action works. If you need the @search_context after calling this function,
 * use gedit_document_get_search_context().
 */
void
gedit_document_set_search_context (GeditDocument          *doc,
				   GtkSourceSearchContext *search_context)
{
	GeditDocumentPrivate *priv;

	g_return_if_fail (GEDIT_IS_DOCUMENT (doc));

	priv = gedit_document_get_instance_private (doc);

	if (priv->search_context != NULL)
	{
		g_signal_handlers_disconnect_by_func (priv->search_context,
						      connect_search_settings,
						      doc);

		g_object_unref (priv->search_context);
	}

	priv->search_context = search_context;

	if (search_context != NULL)
	{
		g_object_ref (search_context);

		g_settings_bind (priv->editor_settings,
				 GEDIT_SETTINGS_SEARCH_HIGHLIGHTING,
				 search_context, "highlight",
				 G_SETTINGS_BIND_GET | G_SETTINGS_BIND_NO_SENSITIVITY);

		g_signal_connect_object (search_context,
					 "notify::settings",
					 G_CALLBACK (connect_search_settings),
					 doc,
					 G_CONNECT_SWAPPED);

		connect_search_settings (doc);
	}

	update_empty_search (doc);
}

/**
 * gedit_document_get_search_context:
 * @doc: a #GeditDocument
 *
 * Gets the search context. Use this function only if you have used
 * gedit_document_set_search_context() before. You should not alter other search
 * contexts, so you have to verify that the returned search context is yours.
 * One way to verify that is to compare the search settings object, or to mark
 * the search context with g_object_set_data().
 *
 * Returns: (transfer none): the current search context of the document, or NULL
 * if there is no current search context.
 */
GtkSourceSearchContext *
gedit_document_get_search_context (GeditDocument *doc)
{
	GeditDocumentPrivate *priv;

	g_return_val_if_fail (GEDIT_IS_DOCUMENT (doc), NULL);

	priv = gedit_document_get_instance_private (doc);

	return priv->search_context;
}

gboolean
_gedit_document_get_empty_search (GeditDocument *doc)
{
	GeditDocumentPrivate *priv;

	g_return_val_if_fail (GEDIT_IS_DOCUMENT (doc), TRUE);

	priv = gedit_document_get_instance_private (doc);

	return priv->empty_search;
}

/**
 * gedit_document_get_file:
 * @doc: a #GeditDocument.
 *
 * Gets the associated #GtkSourceFile. You should use it only for reading
 * purposes, not for creating a #GtkSourceFileLoader or #GtkSourceFileSaver,
 * because gedit does some extra work when loading or saving a file and
 * maintains an internal state. If you use in a plugin a file loader or saver on
 * the returned #GtkSourceFile, the internal state of gedit won't be updated.
 *
 * If you want to save the #GeditDocument to a secondary file, you can create a
 * new #GtkSourceFile and use a #GtkSourceFileSaver.
 *
 * Returns: (transfer none): the associated #GtkSourceFile.
 * Since: 3.14
 */
GtkSourceFile *
gedit_document_get_file (GeditDocument *doc)
{
	GeditDocumentPrivate *priv;

	g_return_val_if_fail (GEDIT_IS_DOCUMENT (doc), NULL);

	priv = gedit_document_get_instance_private (doc);

	return priv->file;
}

void
_gedit_document_set_create (GeditDocument *doc,
			    gboolean       create)
{
	GeditDocumentPrivate *priv;

	g_return_if_fail (GEDIT_IS_DOCUMENT (doc));

	priv = gedit_document_get_instance_private (doc);

	priv->create = create != FALSE;
}

gboolean
_gedit_document_get_create (GeditDocument *doc)
{
	GeditDocumentPrivate *priv;

	g_return_val_if_fail (GEDIT_IS_DOCUMENT (doc), FALSE);

	priv = gedit_document_get_instance_private (doc);

	return priv->create;
}

/* ex:set ts=8 noet: */
