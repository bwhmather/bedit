/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * bedit-commands.h
 * This file is part of bedit
 *
 * Copyright (C) 1998, 1999 Alex Roberts, Evan Lawrence
 * Copyright (C) 2000, 2001 Chema Celorio, Paolo Maggi
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

#ifndef GEDIT_COMMANDS_H
#define GEDIT_COMMANDS_H

#include <gtksourceview/gtksource.h>
#include <bedit/bedit-window.h>

G_BEGIN_DECLS

/* Do nothing if URI does not exist */
void		 bedit_commands_load_location		(BeditWindow             *window,
							 GFile                   *location,
							 const GtkSourceEncoding *encoding,
							 gint                     line_pos,
							 gint                     column_pos);

/* Ignore non-existing URIs */
GSList		*bedit_commands_load_locations		(BeditWindow             *window,
							 const GSList            *locations,
							 const GtkSourceEncoding *encoding,
							 gint                     line_pos,
							 gint                     column_pos) G_GNUC_WARN_UNUSED_RESULT;

void		 bedit_commands_save_document		(BeditWindow         *window,
                                                         BeditDocument       *document);

void		 bedit_commands_save_document_async	(BeditDocument       *document,
							 BeditWindow         *window,
							 GCancellable        *cancellable,
							 GAsyncReadyCallback  callback,
							 gpointer             user_data);

gboolean	 bedit_commands_save_document_finish	(BeditDocument       *document,
							 GAsyncResult        *result);

void		 bedit_commands_save_all_documents 	(BeditWindow         *window);

G_END_DECLS

#endif /* GEDIT_COMMANDS_H */
/* ex:set ts=8 noet: */
