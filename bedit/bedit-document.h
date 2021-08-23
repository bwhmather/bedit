/*
 * bedit-document.h
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * Based on gedit-document.h from Gedit.
 *
 * Copyright (C) 1998-1999 - Alex Roberts
 * Copyright (C) 1998-1999 - Evan Lawrence
 * Copyright (C) 2000-2001 - Chema Celorio
 * Copyright (C) 2000 - Jacob Leach
 * Copyright (C) 2001 - Carlos Perelló Marín
 * Copyright (C) 2001-2006 - Paolo Maggi
 * Copyright (C) 2002 - Fernando Herrera
 * Copyright (C) 2003 - Eric Ritezel
 * Copyright (C) 2004-2015 - Paolo Borelli
 * Copyright (C) 2007-2010 - Steve Frécinaux
 * Copyright (C) 2008 - Johan Dahlin
 * Copyright (C) 2008-2010 - Jesse van den Kieboom
 * Copyright (C) 2009-2011 - Ignacio Casal Quinteiro
 * Copyright (C) 2010 - Garrett Regier
 * Copyright (C) 2013 - Volker Sobek
 * Copyright (C) 2013-2019 - Sébastien Wilmet
 * Copyright (C) 2014 - Oliver Gerlich
 * Copyright (C) 2019 - Jordi Mas
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
    BeditDocument, bedit_document, BEDIT, DOCUMENT, GtkSourceBuffer
)

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
    BeditDocument *doc, gint line, gint line_offset
);

void bedit_document_set_language(BeditDocument *doc, GtkSourceLanguage *lang);
GtkSourceLanguage *bedit_document_get_language(BeditDocument *doc);

G_END_DECLS

#endif /* BEDIT_DOCUMENT_H */

