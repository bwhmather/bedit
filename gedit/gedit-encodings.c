/*
 * gedit-encodings.c
 * This file is part of gedit
 *
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

/**
 * SECTION:gedit-encodings
 * @short_description: character encodings (deprecated)
 *
 * Character encodings. The functions are deprecated and should not be used in
 * newly written code. Use #GtkSourceEncoding instead.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <glib/gi18n.h>

#include "gedit-encodings.h"

G_DEFINE_BOXED_TYPE (GeditEncoding, gedit_encoding,
                     gedit_encoding_copy,
                     gedit_encoding_free)

const GeditEncoding *
gedit_encoding_get_from_charset (const gchar *charset)
{
	return (const GeditEncoding *) gtk_source_encoding_get_from_charset (charset);
}

const GeditEncoding *
gedit_encoding_get_from_index (gint idx)
{
	return (const GeditEncoding *) gtk_source_encoding_get_from_index (idx);
}

const GeditEncoding *
gedit_encoding_get_utf8 (void)
{
	return (const GeditEncoding *) gtk_source_encoding_get_utf8 ();
}

const GeditEncoding *
gedit_encoding_get_current (void)
{
	return (const GeditEncoding *) gtk_source_encoding_get_current ();
}

gchar *
gedit_encoding_to_string (const GeditEncoding* enc)
{
	return gtk_source_encoding_to_string ((const GtkSourceEncoding *) enc);
}

const gchar *
gedit_encoding_get_charset (const GeditEncoding* enc)
{
	return gtk_source_encoding_get_charset ((const GtkSourceEncoding *) enc);
}

const gchar *
gedit_encoding_get_name (const GeditEncoding* enc)
{
	return gtk_source_encoding_get_name ((const GtkSourceEncoding *) enc);
}

/**
 * gedit_encoding_copy:
 * @enc: a #GeditEncoding.
 *
 * Returns: (transfer none): the copy of @enc.
 */
GeditEncoding *
gedit_encoding_copy (const GeditEncoding *enc)
{
	return (GeditEncoding *) gtk_source_encoding_copy ((const GtkSourceEncoding *) enc);
}

void
gedit_encoding_free (GeditEncoding *enc)
{
	gtk_source_encoding_free ((GtkSourceEncoding *) enc);
}

/* ex:set ts=8 noet: */
