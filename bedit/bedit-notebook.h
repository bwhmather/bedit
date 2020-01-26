/*
 * gedit-notebook.h
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

/* This file is a modified version of the epiphany file ephy-notebook.h
 * Here the relevant copyright:
 *
 *  Copyright (C) 2002 Christophe Fergeau
 *  Copyright (C) 2003 Marco Pesenti Gritti
 *  Copyright (C) 2003, 2004 Christian Persch
 *
 */

#ifndef GEDIT_NOTEBOOK_H
#define GEDIT_NOTEBOOK_H

#include <gedit/gedit-tab.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GEDIT_TYPE_NOTEBOOK		(gedit_notebook_get_type ())
#define GEDIT_NOTEBOOK(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GEDIT_TYPE_NOTEBOOK, BeditNotebook))
#define GEDIT_NOTEBOOK_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), GEDIT_TYPE_NOTEBOOK, BeditNotebookClass))
#define GEDIT_IS_NOTEBOOK(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GEDIT_TYPE_NOTEBOOK))
#define GEDIT_IS_NOTEBOOK_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GEDIT_TYPE_NOTEBOOK))
#define GEDIT_NOTEBOOK_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GEDIT_TYPE_NOTEBOOK, BeditNotebookClass))

typedef struct _BeditNotebook		BeditNotebook;
typedef struct _BeditNotebookClass	BeditNotebookClass;
typedef struct _BeditNotebookPrivate	BeditNotebookPrivate;

/* This is now used in multi-notebook but we keep the same enum for
 * backward compatibility since it is used in the gsettings schema */
typedef enum
{
	GEDIT_NOTEBOOK_SHOW_TABS_NEVER,
	GEDIT_NOTEBOOK_SHOW_TABS_AUTO,
	GEDIT_NOTEBOOK_SHOW_TABS_ALWAYS
} BeditNotebookShowTabsModeType;

struct _BeditNotebook
{
	GtkNotebook notebook;

	/*< private >*/
	BeditNotebookPrivate *priv;
};

struct _BeditNotebookClass
{
	GtkNotebookClass parent_class;

	/* Signals */
	void	(* tab_close_request)	(BeditNotebook *notebook,
					 BeditTab      *tab);
	void	(* show_popup_menu)	(BeditNotebook *notebook,
					 GdkEvent      *event,
					 BeditTab      *tab);
	gboolean(* change_to_page)      (BeditNotebook *notebook,
	                                 gint           page_num);
};

GType		gedit_notebook_get_type		(void) G_GNUC_CONST;

GtkWidget      *gedit_notebook_new		(void);

void		gedit_notebook_add_tab		(BeditNotebook *nb,
						 BeditTab      *tab,
						 gint           position,
						 gboolean       jump_to);

void		gedit_notebook_move_tab		(BeditNotebook *src,
						 BeditNotebook *dest,
						 BeditTab      *tab,
						 gint           dest_position);

void		gedit_notebook_remove_all_tabs 	(BeditNotebook *nb);

G_END_DECLS

#endif /* GEDIT_NOTEBOOK_H */

/* ex:set ts=8 noet: */
