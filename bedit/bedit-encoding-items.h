/*
 * bedit-encoding-items.h
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

#ifndef BEDIT_ENCODING_ITEMS_H
#define BEDIT_ENCODING_ITEMS_H

#include <gtksourceview/gtksource.h>

G_BEGIN_DECLS

typedef struct _BeditEncodingItem BeditEncodingItem;

GSList *bedit_encoding_items_get(void);

void bedit_encoding_item_free(BeditEncodingItem *item);
const GtkSourceEncoding *bedit_encoding_item_get_encoding(
    BeditEncodingItem *item);
const gchar *bedit_encoding_item_get_name(BeditEncodingItem *item);

G_END_DECLS

#endif /* BEDIT_ENCODING_ITEMS_H */

