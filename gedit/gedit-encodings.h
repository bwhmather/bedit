/*
 * gedit-encodings.h
 * This file is part of gedit
 *
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

#ifndef __GEDIT_ENCODINGS_H__
#define __GEDIT_ENCODINGS_H__

#include <gtksourceview/gtksource.h>

G_BEGIN_DECLS

typedef GtkSourceEncoding GeditEncoding;

#define GEDIT_TYPE_ENCODING     (gedit_encoding_get_type ())

GType              	 gedit_encoding_get_type	 (void) G_GNUC_CONST;

const GeditEncoding	*gedit_encoding_get_from_charset (const gchar         *charset);

const GeditEncoding	*gedit_encoding_get_from_index	 (gint                 index);

gchar 			*gedit_encoding_to_string	 (const GeditEncoding *enc);

const gchar		*gedit_encoding_get_name	 (const GeditEncoding *enc);

const gchar		*gedit_encoding_get_charset	 (const GeditEncoding *enc);

const GeditEncoding 	*gedit_encoding_get_utf8	 (void);

const GeditEncoding 	*gedit_encoding_get_current	 (void);

/* These should not be used, they are just to make python bindings happy */
GeditEncoding		*gedit_encoding_copy		 (const GeditEncoding *enc);

void               	 gedit_encoding_free		 (GeditEncoding       *enc);

GSList			*_gedit_encoding_strv_to_list    (const gchar * const *enc_str);
gchar		       **_gedit_encoding_list_to_strv	 (const GSList        *enc);

G_END_DECLS

#endif  /* __GEDIT_ENCODINGS_H__ */

/* ex:set ts=8 noet: */
