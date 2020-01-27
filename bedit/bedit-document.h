/*
 * bedit-document.h
 * This file is part of bedit
 *
 * Copyright (C) 1998, 1999 Alex Roberts, Evan Lawrence
 * Copyright (C) 2000, 2001 Chema Celorio, Paolo Maggi
 * Copyright (C) 2002-2005 Paolo Maggi
 * Copyright (C) 2014 Sébastien Wilmet
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

#ifndef BEDIT_DOCUMENT_H
#define BEDIT_DOCUMENT_H

#include <gtksourceview/gtksource.h>

G_BEGIN_DECLS

#define BEDIT_TYPE_DOCUMENT (bedit_document_get_type())

G_DECLARE_DERIVABLE_TYPE(
    BeditDocument, bedit_document, BEDIT, DOCUMENT, GtkSourceBuffer)

struct _BeditDocumentClass {
    GtkSourceBufferClass parent_class;

    /* Signals */
    void (*cursor_moved)(BeditDocument *document);

    void (*load)(BeditDocument *document);

    void (*loaded)(BeditDocument *document);

    void (*save)(BeditDocument *document);

    void (*saved)(BeditDocument *document);
};

BeditDocument *bedit_document_new(void);

GtkSourceFile *bedit_document_get_file(BeditDocument *doc);

gchar *bedit_document_get_uri_for_display(BeditDocument *doc);

gchar *bedit_document_get_short_name_for_display(BeditDocument *doc);

gchar *bedit_document_get_content_type(BeditDocument *doc);

gchar *bedit_document_get_mime_type(BeditDocument *doc);

gboolean bedit_document_is_untouched(BeditDocument *doc);

gboolean bedit_document_is_untitled(BeditDocument *doc);

gboolean bedit_document_goto_line(BeditDocument *doc, gint line);

gboolean bedit_document_goto_line_offset(
    BeditDocument *doc, gint line, gint line_offset);

void bedit_document_set_language(BeditDocument *doc, GtkSourceLanguage *lang);
GtkSourceLanguage *bedit_document_get_language(BeditDocument *doc);

gchar *bedit_document_get_metadata(BeditDocument *doc, const gchar *key);

void bedit_document_set_metadata(
    BeditDocument *doc, const gchar *first_key, ...);

void bedit_document_set_search_context(
    BeditDocument *doc, GtkSourceSearchContext *search_context);

GtkSourceSearchContext *bedit_document_get_search_context(BeditDocument *doc);

G_END_DECLS

#endif /* BEDIT_DOCUMENT_H */

