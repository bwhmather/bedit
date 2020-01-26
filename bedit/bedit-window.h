/*
 * gedit-window.h
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
 * MERCHANWINDOWILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GEDIT_WINDOW_H
#define GEDIT_WINDOW_H

#include <gtksourceview/gtksource.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#include <gedit/gedit-tab.h>
#include <gedit/gedit-message-bus.h>

G_BEGIN_DECLS

typedef enum
{
	GEDIT_WINDOW_STATE_NORMAL		= 0,
	GEDIT_WINDOW_STATE_SAVING		= 1 << 1,
	GEDIT_WINDOW_STATE_PRINTING		= 1 << 2,
	GEDIT_WINDOW_STATE_LOADING		= 1 << 3,
	GEDIT_WINDOW_STATE_ERROR		= 1 << 4
} BeditWindowState;

#define GEDIT_TYPE_WINDOW              (gedit_window_get_type())
#define GEDIT_WINDOW(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), GEDIT_TYPE_WINDOW, BeditWindow))
#define GEDIT_WINDOW_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), GEDIT_TYPE_WINDOW, BeditWindowClass))
#define GEDIT_IS_WINDOW(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), GEDIT_TYPE_WINDOW))
#define GEDIT_IS_WINDOW_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GEDIT_TYPE_WINDOW))
#define GEDIT_WINDOW_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), GEDIT_TYPE_WINDOW, BeditWindowClass))

typedef struct _BeditWindow        BeditWindow;
typedef struct _BeditWindowClass   BeditWindowClass;
typedef struct _BeditWindowPrivate BeditWindowPrivate;

struct _BeditWindow
{
	GtkApplicationWindow window;

	/*< private > */
	BeditWindowPrivate *priv;
};

struct _BeditWindowClass
{
	GtkApplicationWindowClass parent_class;

	/* Signals */
	void	 (* tab_added)      	(BeditWindow *window,
					 BeditTab    *tab);
	void	 (* tab_removed)    	(BeditWindow *window,
					 BeditTab    *tab);
	void	 (* tabs_reordered) 	(BeditWindow *window);
	void	 (* active_tab_changed)	(BeditWindow *window,
				     	 BeditTab    *tab);
	void	 (* active_tab_state_changed)
					(BeditWindow *window);
};

/* Public methods */
GType 		 gedit_window_get_type 			(void) G_GNUC_CONST;

BeditTab	*gedit_window_create_tab		(BeditWindow         *window,
							 gboolean             jump_to);

BeditTab	*gedit_window_create_tab_from_location	(BeditWindow             *window,
							 GFile                   *location,
							 const GtkSourceEncoding *encoding,
							 gint                     line_pos,
							 gint                     column_pos,
							 gboolean                 create,
							 gboolean                 jump_to);

BeditTab	*gedit_window_create_tab_from_stream	(BeditWindow             *window,
							 GInputStream            *stream,
							 const GtkSourceEncoding *encoding,
							 gint                     line_pos,
							 gint                     column_pos,
							 gboolean                 jump_to);

void		 gedit_window_close_tab			(BeditWindow         *window,
							 BeditTab            *tab);

void		 gedit_window_close_all_tabs		(BeditWindow         *window);

void		 gedit_window_close_tabs		(BeditWindow         *window,
							 const GList         *tabs);

BeditTab	*gedit_window_get_active_tab		(BeditWindow         *window);

void		 gedit_window_set_active_tab		(BeditWindow         *window,
							 BeditTab            *tab);

/* Helper functions */
BeditView	*gedit_window_get_active_view		(BeditWindow         *window);
BeditDocument	*gedit_window_get_active_document	(BeditWindow         *window);

/* Returns a newly allocated list with all the documents in the window */
GList		*gedit_window_get_documents		(BeditWindow         *window);

/* Returns a newly allocated list with all the documents that need to be
   saved before closing the window */
GList		*gedit_window_get_unsaved_documents 	(BeditWindow         *window);

/* Returns a newly allocated list with all the views in the window */
GList		*gedit_window_get_views			(BeditWindow         *window);

GtkWindowGroup  *gedit_window_get_group			(BeditWindow         *window);

GtkWidget	*gedit_window_get_side_panel		(BeditWindow         *window);

GtkWidget	*gedit_window_get_bottom_panel		(BeditWindow         *window);

GtkWidget	*gedit_window_get_statusbar		(BeditWindow         *window);

BeditWindowState gedit_window_get_state 		(BeditWindow         *window);

BeditTab        *gedit_window_get_tab_from_location	(BeditWindow         *window,
							 GFile               *location);

/* Message bus */
BeditMessageBus	*gedit_window_get_message_bus		(BeditWindow         *window);

/*
 * Non exported functions
 */
GtkWidget	*_gedit_window_get_multi_notebook	(BeditWindow         *window);
GtkWidget	*_gedit_window_get_notebook		(BeditWindow         *window);

GMenuModel	*_gedit_window_get_hamburger_menu	(BeditWindow         *window);

BeditWindow	*_gedit_window_move_tab_to_new_window	(BeditWindow         *window,
							 BeditTab            *tab);
void             _gedit_window_move_tab_to_new_tab_group(BeditWindow         *window,
                                                         BeditTab            *tab);
gboolean	 _gedit_window_is_removing_tabs		(BeditWindow         *window);

GFile		*_gedit_window_get_default_location 	(BeditWindow         *window);

void		 _gedit_window_set_default_location 	(BeditWindow         *window,
							 GFile               *location);

void		 _gedit_window_fullscreen		(BeditWindow         *window);

void		 _gedit_window_unfullscreen		(BeditWindow         *window);

gboolean	 _gedit_window_is_fullscreen		(BeditWindow         *window);

GList		*_gedit_window_get_all_tabs		(BeditWindow         *window);

GFile		*_gedit_window_pop_last_closed_doc	(BeditWindow         *window);

G_END_DECLS

#endif  /* GEDIT_WINDOW_H  */

/* ex:set ts=8 noet: */
