/*
 * gedit-encoding-items.c
 * This file is part of gedit
 *
 * Copyright (C) 2014 - Jesse van den Kieboom
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

#include "gedit-encoding-items.h"
#include "gedit-settings.h"
#include "gedit-utils.h"

#include <glib/gi18n.h>

struct _GeditEncodingItem
{
	const GtkSourceEncoding *encoding;
	gchar *name;
};

static GeditEncodingItem *
gedit_encoding_item_new (const GtkSourceEncoding *encoding,
                         gchar                   *name)
{
	GeditEncodingItem *item = g_slice_new (GeditEncodingItem);

	item->encoding = encoding;
	item->name = name;

	return item;
}

void
gedit_encoding_item_free (GeditEncodingItem *item)
{
	if (item == NULL)
	{
		return;
	}

	g_free (item->name);
	g_slice_free (GeditEncodingItem, item);
}

const GtkSourceEncoding *
gedit_encoding_item_get_encoding (GeditEncodingItem *item)
{
	g_return_val_if_fail (item != NULL, NULL);

	return item->encoding;
}

const gchar *
gedit_encoding_item_get_name (GeditEncodingItem *item)
{
	g_return_val_if_fail (item != NULL, NULL);

	return item->name;
}

GSList *
gedit_encoding_items_get (void)
{
	GSList *ret = NULL;
	const GtkSourceEncoding *utf8_encoding;
	const GtkSourceEncoding *current_encoding;
	GSettings *enc_settings;
	gchar *str;
	GSList *encodings;
	gchar **enc_strv;

	utf8_encoding = gtk_source_encoding_get_utf8 ();
	current_encoding = gtk_source_encoding_get_current ();

	enc_settings = g_settings_new ("org.gnome.gedit.preferences.encodings");

	if (utf8_encoding != current_encoding)
	{
		str = gtk_source_encoding_to_string (utf8_encoding);
	}
	else
	{
		str = g_strdup_printf (_("Current Locale (%s)"),
				       gtk_source_encoding_get_charset (utf8_encoding));
	}

	ret = g_slist_prepend (ret, gedit_encoding_item_new (utf8_encoding, str));

	if (current_encoding != utf8_encoding &&
	    current_encoding != NULL)
	{
		str = g_strdup_printf (_("Current Locale (%s)"),
				       gtk_source_encoding_get_charset (current_encoding));

		ret = g_slist_prepend (ret, gedit_encoding_item_new (current_encoding, str));
	}

	enc_strv = g_settings_get_strv (enc_settings, GEDIT_SETTINGS_ENCODING_SHOWN_IN_MENU);

	encodings = _gedit_utils_encoding_strv_to_list ((const gchar * const *)enc_strv);
	g_strfreev (enc_strv);

	g_object_unref (enc_settings);

	while (encodings)
	{
		const GtkSourceEncoding *enc = encodings->data;

		if (enc != current_encoding &&
		    enc != utf8_encoding &&
		    enc != NULL)
		{
			str = gtk_source_encoding_to_string (enc);
			ret = g_slist_prepend (ret, gedit_encoding_item_new (enc, str));
		}

		encodings = g_slist_delete_link (encodings, encodings);
	}

	return g_slist_reverse (ret);
}

/* ex:set ts=8 noet: */
