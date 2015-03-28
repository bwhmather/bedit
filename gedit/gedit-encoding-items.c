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

#include <glib/gi18n.h>

#include "gedit-settings.h"
#include "gedit-utils.h"

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
	const GtkSourceEncoding *utf8_encoding;
	const GtkSourceEncoding *current_encoding;
	GSettings *settings;
	gchar **settings_strv;
	GSList *encodings;
	GSList *ret = NULL;
	GSList *l;

	utf8_encoding = gtk_source_encoding_get_utf8 ();
	current_encoding = gtk_source_encoding_get_current ();

	settings = g_settings_new ("org.gnome.gedit.preferences.encodings");

	settings_strv = g_settings_get_strv (settings, GEDIT_SETTINGS_CANDIDATE_ENCODINGS);

	/* First take the candidate encodings from GSettings. If the gsetting is
	 * empty, take the default candidates of GtkSourceEncoding.
	 */
	if (settings_strv != NULL && settings_strv[0] != NULL)
	{
		encodings = _gedit_utils_encoding_strv_to_list ((const gchar * const *)settings_strv);

		/* Ensure that UTF-8 is present. */
		if (utf8_encoding != current_encoding &&
		    g_slist_find (encodings, utf8_encoding) == NULL)
		{
			encodings = g_slist_prepend (encodings, (gpointer)utf8_encoding);
		}

		/* Ensure that the current locale encoding is present (if not
		 * present, it must be the first encoding).
		 */
		if (g_slist_find (encodings, current_encoding) == NULL)
		{
			encodings = g_slist_prepend (encodings, (gpointer)current_encoding);
		}
	}
	else
	{
		encodings = gtk_source_encoding_get_default_candidates ();
	}

	for (l = encodings; l != NULL; l = l->next)
	{
		const GtkSourceEncoding *enc = l->data;
		gchar *name;

		if (enc == NULL)
		{
			continue;
		}

		if (enc == current_encoding)
		{
			name = g_strdup_printf (_("Current Locale (%s)"),
						gtk_source_encoding_get_charset (enc));
		}
		else
		{
			name = gtk_source_encoding_to_string (enc);
		}

		ret = g_slist_prepend (ret, gedit_encoding_item_new (enc, name));
	}

	ret = g_slist_reverse (ret);

	g_object_unref (settings);
	g_strfreev (settings_strv);
	g_slist_free (encodings);
	return ret;
}

/* ex:set ts=8 noet: */
