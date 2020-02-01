/*
 * bedit-document-private.h
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * Based on gedit-document-private.h from Gedit.
 *
 * Copyright (C) 1998-1999 - Alex Roberts
 * Copyright (C) 1998-1999 - Evan Lawrence
 * Copyright (C) 2000-2001 - Chema Celorio
 * Copyright (C) 2000-2005 - Paolo Maggi
 * Copyright (C) 2015 - Paolo Borelli
 * Copyright (C) 2015-2019 - Sébastien Wilmet
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
#ifndef BEDIT_DOCUMENT_PRIVATE_H
#define BEDIT_DOCUMENT_PRIVATE_H

#include "bedit-document.h"

G_BEGIN_DECLS

#define BEDIT_METADATA_ATTRIBUTE_POSITION "bedit-position"
#define BEDIT_METADATA_ATTRIBUTE_ENCODING "bedit-encoding"
#define BEDIT_METADATA_ATTRIBUTE_LANGUAGE "bedit-language"

glong _bedit_document_get_seconds_since_last_save_or_load(BeditDocument *doc);

gboolean _bedit_document_needs_saving(BeditDocument *doc);

gboolean _bedit_document_get_empty_search(BeditDocument *doc);

void _bedit_document_set_create(BeditDocument *doc, gboolean create);

gboolean _bedit_document_get_create(BeditDocument *doc);

G_END_DECLS

#endif /* BEDIT_DOCUMENT_PRIVATE_H */

