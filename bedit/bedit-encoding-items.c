/*
 * bedit-encoding-items.c
 * This file is part of bedit
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

#include "bedit-encoding-items.h"

#include <glib/gi18n.h>

#include "bedit-settings.h"

struct _BeditEncodingItem
{
	const GtkSourceEncoding *encoding;
	gchar *name;
};

static BeditEncodingItem *
bedit_encoding_item_new (const GtkSourceEncoding *encoding,
                         gchar                   *name)
{
	BeditEncodingItem *item = g_slice_new (BeditEncodingItem);

	item->encoding = encoding;
	item->name = name;

	return item;
}

void
bedit_encoding_item_free (BeditEncodingItem *item)
{
	if (item == NULL)
	{
		return;
	}

	g_free (item->name);
	g_slice_free (BeditEncodingItem, item);
}

const GtkSourceEncoding *
bedit_encoding_item_get_encoding (BeditEncodingItem *item)
{
	g_return_val_if_fail (item != NULL, NULL);

	return item->encoding;
}

const gchar *
bedit_encoding_item_get_name (BeditEncodingItem *item)
{
	g_return_val_if_fail (item != NULL, NULL);

	return item->name;
}

GSList *
bedit_encoding_items_get (void)
{
	const GtkSourceEncoding *current_encoding;
	GSList *encodings;
	GSList *items = NULL;
	GSList *l;

	encodings = bedit_settings_get_candidate_encodings (NULL);

	current_encoding = gtk_source_encoding_get_current ();

	for (l = encodings; l != NULL; l = l->next)
	{
		const GtkSourceEncoding *enc = l->data;
		gchar *name;

		if (enc == current_encoding)
		{
			name = g_strdup_printf (_("Current Locale (%s)"),
						gtk_source_encoding_get_charset (enc));
		}
		else
		{
			name = gtk_source_encoding_to_string (enc);
		}

		items = g_slist_prepend (items, bedit_encoding_item_new (enc, name));
	}

	g_slist_free (encodings);

	return g_slist_reverse (items);
}

/* ex:set ts=8 noet: */
