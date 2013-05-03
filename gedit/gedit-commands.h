/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gedit-commands.h
 * This file is part of gedit
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the gedit Team, 1998-2005. See the AUTHORS file for a
 * list of people on the gedit Team.
 * See the ChangeLog files for a list of changes.
 *
 * $Id$
 */

#ifndef __GEDIT_COMMANDS_H__
#define __GEDIT_COMMANDS_H__

#include <gtk/gtk.h>
#include <gedit/gedit-window.h>

G_BEGIN_DECLS

/* Do nothing if URI does not exist */
void		 gedit_commands_load_location		(GeditWindow         *window,
							 GFile               *location,
							 const GeditEncoding *encoding,
							 gint                 line_pos,
							 gint                 column_pos);

/* Ignore non-existing URIs */
GSList		*gedit_commands_load_locations		(GeditWindow         *window,
							 const GSList        *locations,
							 const GeditEncoding *encoding,
							 gint                 line_pos,
							 gint                 column_pos) G_GNUC_WARN_UNUSED_RESULT;

void		 gedit_commands_save_document		(GeditWindow         *window,
                                                         GeditDocument       *document);

void		 gedit_commands_save_all_documents 	(GeditWindow         *window);

/*
 * Non-exported functions
 */

/* Create titled documens for non-existing URIs */
GSList	        *_gedit_cmd_load_files_from_prompt	(GeditWindow         *window,
							 GSList              *files,
							 const GeditEncoding *encoding,
							 gint                 line_pos,
							 gint                 column_pos) G_GNUC_WARN_UNUSED_RESULT;

void		_gedit_cmd_file_new			(GSimpleAction *action,
                                                         GVariant      *parameter,
                                                         gpointer       user_data);
void		_gedit_cmd_file_open			(GSimpleAction *action,
                                                         GVariant      *parameter,
                                                         gpointer       user_data);
void		_gedit_cmd_file_save_tab		(GeditTab    *tab,
							 GeditWindow *window);
void		_gedit_cmd_file_save_as_tab		(GeditTab    *tab,
							 GeditWindow *window);
void		_gedit_cmd_file_save			(GSimpleAction *action,
                                                         GVariant      *parameter,
                                                         gpointer       user_data);
void		_gedit_cmd_file_save_as			(GSimpleAction *action,
                                                         GVariant      *parameter,
                                                         gpointer       user_data);
void		_gedit_cmd_file_save_all		(GSimpleAction *action,
                                                         GVariant      *parameter,
                                                         gpointer       user_data);
void		_gedit_cmd_file_revert			(GSimpleAction *action,
                                                         GVariant      *parameter,
                                                         gpointer       user_data);
void		_gedit_cmd_file_open_uri		(GtkAction   *action,
							 GeditWindow *window);
void		_gedit_cmd_file_print			(GSimpleAction *action,
                                                         GVariant      *parameter,
                                                         gpointer       user_data);
void		_gedit_cmd_file_close			(GSimpleAction *action,
                                                         GVariant      *parameter,
                                                         gpointer       user_data);
void		_gedit_cmd_file_close_all		(GSimpleAction *action,
                                                         GVariant      *parameter,
                                                         gpointer       user_data);
void		_gedit_cmd_file_quit			(GSimpleAction *action,
                                                         GVariant      *parameter,
                                                         gpointer       user_data);

void		_gedit_cmd_edit_undo			(GSimpleAction *action,
                                                         GVariant      *parameter,
                                                         gpointer       user_data);
void		_gedit_cmd_edit_redo			(GSimpleAction *action,
                                                         GVariant      *parameter,
                                                         gpointer       user_data);
void		_gedit_cmd_edit_cut			(GSimpleAction *action,
                                                         GVariant      *parameter,
                                                         gpointer       user_data);
void		_gedit_cmd_edit_copy			(GSimpleAction *action,
                                                         GVariant      *parameter,
                                                         gpointer       user_data);
void		_gedit_cmd_edit_paste			(GSimpleAction *action,
                                                         GVariant      *parameter,
                                                         gpointer       user_data);
void		_gedit_cmd_edit_delete			(GSimpleAction *action,
                                                         GVariant      *parameter,
                                                         gpointer       user_data);
void		_gedit_cmd_edit_select_all		(GSimpleAction *action,
                                                         GVariant      *parameter,
                                                         gpointer       user_data);
void		_gedit_cmd_edit_preferences		(GSimpleAction *action,
                                                         GVariant      *parameter,
                                                         gpointer       user_data);

void		_gedit_cmd_view_toggle_fullscreen_mode	(GSimpleAction *action,
                                                         GVariant      *parameter,
                                                         gpointer       user_data);
void		_gedit_cmd_view_leave_fullscreen_mode	(GSimpleAction *action,
                                                         GVariant      *parameter,
                                                         gpointer       user_data);
void            _gedit_cmd_view_highlight_mode          (GSimpleAction *action,
                                                         GVariant      *parameter,
                                                         gpointer       user_data);

void		_gedit_cmd_search_find			(GSimpleAction *action,
                                                         GVariant      *parameter,
                                                         gpointer       user_data);
void		_gedit_cmd_search_find_next		(GSimpleAction *action,
                                                         GVariant      *parameter,
                                                         gpointer       user_data);
void		_gedit_cmd_search_find_prev		(GSimpleAction *action,
                                                         GVariant      *parameter,
                                                         gpointer       user_data);
void		_gedit_cmd_search_replace		(GSimpleAction *action,
                                                         GVariant      *parameter,
                                                         gpointer       user_data);
void		_gedit_cmd_search_clear_highlight	(GSimpleAction *action,
                                                         GVariant      *parameter,
                                                         gpointer       user_data);
void		_gedit_cmd_search_goto_line		(GSimpleAction *action,
                                                         GVariant      *parameter,
                                                         gpointer       user_data);

void		_gedit_cmd_documents_previous_document	(GSimpleAction *action,
                                                         GVariant      *parameter,
                                                         gpointer       user_data);
void		_gedit_cmd_documents_next_document	(GSimpleAction *action,
                                                         GVariant      *parameter,
                                                         gpointer       user_data);
void		_gedit_cmd_documents_move_to_new_window	(GSimpleAction *action,
                                                         GVariant      *parameter,
                                                         gpointer       user_data);
void		_gedit_cmd_documents_new_tab_group	(GSimpleAction *action,
                                                         GVariant      *parameter,
                                                         gpointer       user_data);
void		_gedit_cmd_documents_previous_tab_group	(GSimpleAction *action,
                                                         GVariant      *parameter,
                                                         gpointer       user_data);
void		_gedit_cmd_documents_next_tab_group	(GSimpleAction *action,
                                                         GVariant      *parameter,
                                                         gpointer       user_data);

void		_gedit_cmd_help_contents		(GtkAction   *action,
							 GeditWindow *window);
void		_gedit_cmd_help_about			(GtkAction   *action,
							 GeditWindow *window);

void		_gedit_cmd_file_close_tab 		(GeditTab    *tab,
							 GeditWindow *window);

void		_gedit_cmd_file_save_documents_list	(GeditWindow *window,
							 GList       *docs);


G_END_DECLS

#endif /* __GEDIT_COMMANDS_H__ */
/* ex:set ts=8 noet: */
