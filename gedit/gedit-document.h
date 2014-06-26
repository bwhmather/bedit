/*
 * gedit-document.h
 * This file is part of gedit
 *
 * Copyright (C) 1998, 1999 Alex Roberts, Evan Lawrence
 * Copyright (C) 2000, 2001 Chema Celorio, Paolo Maggi
 * Copyright (C) 2002-2005 Paolo Maggi
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

#ifndef __GEDIT_DOCUMENT_H__
#define __GEDIT_DOCUMENT_H__

#include <gtksourceview/gtksource.h>

#include <gedit/gedit-encodings.h>

G_BEGIN_DECLS

#define GEDIT_TYPE_DOCUMENT              (gedit_document_get_type())
#define GEDIT_DOCUMENT(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), GEDIT_TYPE_DOCUMENT, GeditDocument))
#define GEDIT_DOCUMENT_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), GEDIT_TYPE_DOCUMENT, GeditDocumentClass))
#define GEDIT_IS_DOCUMENT(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), GEDIT_TYPE_DOCUMENT))
#define GEDIT_IS_DOCUMENT_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GEDIT_TYPE_DOCUMENT))
#define GEDIT_DOCUMENT_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), GEDIT_TYPE_DOCUMENT, GeditDocumentClass))

typedef struct _GeditDocument		GeditDocument;
typedef struct _GeditDocumentClass	GeditDocumentClass;
typedef struct _GeditDocumentPrivate	GeditDocumentPrivate;

#ifdef G_OS_WIN32
#define GEDIT_METADATA_ATTRIBUTE_POSITION "position"
#define GEDIT_METADATA_ATTRIBUTE_ENCODING "encoding"
#define GEDIT_METADATA_ATTRIBUTE_LANGUAGE "language"
#else
#define GEDIT_METADATA_ATTRIBUTE_POSITION "metadata::gedit-position"
#define GEDIT_METADATA_ATTRIBUTE_ENCODING "metadata::gedit-encoding"
#define GEDIT_METADATA_ATTRIBUTE_LANGUAGE "metadata::gedit-language"
#endif

typedef enum
{
	GEDIT_DOCUMENT_NEWLINE_TYPE_LF,
	GEDIT_DOCUMENT_NEWLINE_TYPE_CR,
	GEDIT_DOCUMENT_NEWLINE_TYPE_CR_LF
} GeditDocumentNewlineType;

#ifdef G_OS_WIN32
#define GEDIT_DOCUMENT_NEWLINE_TYPE_DEFAULT GEDIT_DOCUMENT_NEWLINE_TYPE_CR_LF
#else
#define GEDIT_DOCUMENT_NEWLINE_TYPE_DEFAULT GEDIT_DOCUMENT_NEWLINE_TYPE_LF
#endif

/**
 * GeditDocumentSaveFlags:
 * @GEDIT_DOCUMENT_SAVE_IGNORE_MTIME: save file despite external modifications.
 * @GEDIT_DOCUMENT_SAVE_IGNORE_BACKUP: write the file directly without attempting to backup.
 * @GEDIT_DOCUMENT_SAVE_PRESERVE_BACKUP: preserve previous backup file, needed to support autosaving.
 * @GEDIT_DOCUMENT_SAVE_IGNORE_INVALID_CHARS: do not save invalid characters.
 */
typedef enum
{
	GEDIT_DOCUMENT_SAVE_IGNORE_MTIME 	= 1 << 0,
	GEDIT_DOCUMENT_SAVE_IGNORE_BACKUP	= 1 << 1,
	GEDIT_DOCUMENT_SAVE_PRESERVE_BACKUP	= 1 << 2,
	GEDIT_DOCUMENT_SAVE_IGNORE_INVALID_CHARS= 1 << 3
} GeditDocumentSaveFlags;

struct _GeditDocument
{
	GtkSourceBuffer buffer;

	/*< private > */
	GeditDocumentPrivate *priv;
};

struct _GeditDocumentClass
{
	GtkSourceBufferClass parent_class;

	/* Signals */

	void (* cursor_moved)		(GeditDocument *document);

	void (* load)			(GeditDocument *document);

	void (* loaded)			(GeditDocument *document,
					 const GError  *error);

	void (* save)			(GeditDocument *document);

	void (* saved)  		(GeditDocument *document,
					 const GError  *error);
};

#define GEDIT_DOCUMENT_ERROR gedit_document_error_quark ()

enum
{
	GEDIT_DOCUMENT_ERROR_EXTERNALLY_MODIFIED,
	GEDIT_DOCUMENT_ERROR_CANT_CREATE_BACKUP, /* unused */
	GEDIT_DOCUMENT_ERROR_TOO_BIG,
	GEDIT_DOCUMENT_ERROR_ENCODING_AUTO_DETECTION_FAILED,
	GEDIT_DOCUMENT_ERROR_CONVERSION_FALLBACK,
	GEDIT_DOCUMENT_NUM_ERRORS
};

GQuark		 gedit_document_error_quark	(void);

GType		 gedit_document_get_type      	(void) G_GNUC_CONST;

GeditDocument   *gedit_document_new 		(void);

GFile		*gedit_document_get_location	(GeditDocument       *doc);
void		 gedit_document_set_location	(GeditDocument       *doc,
						 GFile               *location);

gchar		*gedit_document_get_uri_for_display
						(GeditDocument       *doc);
gchar		*gedit_document_get_short_name_for_display
					 	(GeditDocument       *doc);

void		 gedit_document_set_short_name_for_display
						(GeditDocument       *doc,
						 const gchar         *short_name);

gchar		*gedit_document_get_content_type
					 	(GeditDocument       *doc);

void		 gedit_document_set_content_type
					 	(GeditDocument       *doc,
					 	 const gchar         *content_type);

gchar		*gedit_document_get_mime_type 	(GeditDocument       *doc);

gboolean	 gedit_document_get_readonly 	(GeditDocument       *doc);

gboolean	 gedit_document_is_untouched 	(GeditDocument       *doc);
gboolean	 gedit_document_is_untitled 	(GeditDocument       *doc);

gboolean	 gedit_document_is_local	(GeditDocument       *doc);

gboolean	 gedit_document_get_deleted	(GeditDocument       *doc);

gboolean	 gedit_document_goto_line 	(GeditDocument       *doc,
						 gint                 line);

gboolean	 gedit_document_goto_line_offset(GeditDocument       *doc,
						 gint                 line,
						 gint                 line_offset);

void 		 gedit_document_set_language 	(GeditDocument       *doc,
						 GtkSourceLanguage   *lang);
GtkSourceLanguage
		*gedit_document_get_language 	(GeditDocument       *doc);

const GeditEncoding
		*gedit_document_get_encoding	(GeditDocument       *doc);

GeditDocumentNewlineType
		 gedit_document_get_newline_type (GeditDocument      *doc);

GtkSourceCompressionType
		 gedit_document_get_compression_type (GeditDocument  *doc);

gchar		*gedit_document_get_metadata	(GeditDocument       *doc,
						 const gchar         *key);

void		 gedit_document_set_metadata	(GeditDocument       *doc,
						 const gchar         *first_key,
						 ...);

void			 gedit_document_set_search_context	(GeditDocument          *doc,
								 GtkSourceSearchContext *search_context);

GtkSourceSearchContext	*gedit_document_get_search_context	(GeditDocument          *doc);

/* Non exported functions */

glong		 _gedit_document_get_seconds_since_last_save_or_load
						(GeditDocument       *doc);

/* Note: this is a sync stat: use only on local files */
gboolean	_gedit_document_check_externally_modified
						(GeditDocument       *doc);

gboolean	 _gedit_document_needs_saving	(GeditDocument       *doc);

/**
 * GeditMountOperationFactory: (skip)
 * @doc:
 * @userdata:
 */
typedef GMountOperation *(*GeditMountOperationFactory)(GeditDocument *doc,
						       gpointer       userdata);

void		 _gedit_document_set_mount_operation_factory
						(GeditDocument	            *doc,
						 GeditMountOperationFactory  callback,
						 gpointer	             userdata);

gboolean		 _gedit_document_get_empty_search	(GeditDocument		*doc);

GtkSourceFile		*gedit_document_get_file		(GeditDocument *doc);

G_END_DECLS

#endif /* __GEDIT_DOCUMENT_H__ */

/* ex:set ts=8 noet: */
