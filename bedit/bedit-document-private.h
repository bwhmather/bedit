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

#ifndef GEDIT_DOCUMENT_PRIVATE_H
#define GEDIT_DOCUMENT_PRIVATE_H

#include "bedit-document.h"

G_BEGIN_DECLS

#define GEDIT_METADATA_ATTRIBUTE_POSITION "bedit-position"
#define GEDIT_METADATA_ATTRIBUTE_ENCODING "bedit-encoding"
#define GEDIT_METADATA_ATTRIBUTE_LANGUAGE "bedit-language"

glong		 _bedit_document_get_seconds_since_last_save_or_load	(BeditDocument       *doc);

gboolean	 _bedit_document_needs_saving				(BeditDocument       *doc);

gboolean	 _bedit_document_get_empty_search			(BeditDocument       *doc);

void		 _bedit_document_set_create				(BeditDocument       *doc,
									 gboolean             create);

gboolean	 _bedit_document_get_create				(BeditDocument       *doc);

G_END_DECLS

#endif /* GEDIT_DOCUMENT_PRIVATE_H */
/* ex:set ts=8 noet: */
