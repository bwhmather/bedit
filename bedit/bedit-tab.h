/*
 * gedit-tab.h
 * This file is part of gedit
 *
 * Copyright (C) 2005 - Paolo Maggi
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

#ifndef GEDIT_TAB_H
#define GEDIT_TAB_H

#include <gtksourceview/gtksource.h>
#include <gedit/gedit-view.h>
#include <gedit/gedit-document.h>

G_BEGIN_DECLS

typedef enum
{
	GEDIT_TAB_STATE_NORMAL = 0,
	GEDIT_TAB_STATE_LOADING,
	GEDIT_TAB_STATE_REVERTING,
	GEDIT_TAB_STATE_SAVING,
	GEDIT_TAB_STATE_PRINTING,
	GEDIT_TAB_STATE_SHOWING_PRINT_PREVIEW,
	GEDIT_TAB_STATE_LOADING_ERROR,
	GEDIT_TAB_STATE_REVERTING_ERROR,
	GEDIT_TAB_STATE_SAVING_ERROR,
	GEDIT_TAB_STATE_GENERIC_ERROR,
	GEDIT_TAB_STATE_CLOSING,
	GEDIT_TAB_STATE_EXTERNALLY_MODIFIED_NOTIFICATION,
	GEDIT_TAB_NUM_OF_STATES /* This is not a valid state */
} BeditTabState;

#define GEDIT_TYPE_TAB (gedit_tab_get_type())

G_DECLARE_FINAL_TYPE (BeditTab, gedit_tab, GEDIT, TAB, GtkBox)

BeditView	*gedit_tab_get_view			(BeditTab            *tab);

/* This is only an helper function */
BeditDocument	*gedit_tab_get_document			(BeditTab            *tab);

BeditTab	*gedit_tab_get_from_document		(BeditDocument       *doc);

BeditTabState	 gedit_tab_get_state			(BeditTab            *tab);

gboolean	 gedit_tab_get_auto_save_enabled	(BeditTab            *tab);

void		 gedit_tab_set_auto_save_enabled	(BeditTab            *tab,
							 gboolean            enable);

gint		 gedit_tab_get_auto_save_interval	(BeditTab            *tab);

void		 gedit_tab_set_auto_save_interval	(BeditTab            *tab,
							 gint                interval);

void		 gedit_tab_set_info_bar			(BeditTab            *tab,
							 GtkWidget           *info_bar);

G_END_DECLS

#endif  /* GEDIT_TAB_H  */

/* ex:set ts=8 noet: */
