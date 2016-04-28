/*
 * gedit-encoding-items.h
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

#ifndef GEDIT_ENCODING_ITEMS_H
#define GEDIT_ENCODING_ITEMS_H

#include <gtksourceview/gtksource.h>

G_BEGIN_DECLS

typedef struct _GeditEncodingItem GeditEncodingItem;

GSList				*gedit_encoding_items_get		(void);

void				 gedit_encoding_item_free		(GeditEncodingItem *item);
const GtkSourceEncoding		*gedit_encoding_item_get_encoding	(GeditEncodingItem *item);
const gchar			*gedit_encoding_item_get_name		(GeditEncodingItem *item);

G_END_DECLS

#endif /* GEDIT_ENCODING_ITEMS_H */

/* ex:set ts=8 noet: */
