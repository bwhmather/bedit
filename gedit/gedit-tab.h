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

#ifndef __GEDIT_TAB_H__
#define __GEDIT_TAB_H__

#include <gtk/gtk.h>
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
	GEDIT_TAB_STATE_PRINT_PREVIEWING,
	GEDIT_TAB_STATE_SHOWING_PRINT_PREVIEW,
	GEDIT_TAB_STATE_GENERIC_NOT_EDITABLE,
	GEDIT_TAB_STATE_LOADING_ERROR,
	GEDIT_TAB_STATE_REVERTING_ERROR,
	GEDIT_TAB_STATE_SAVING_ERROR,
	GEDIT_TAB_STATE_GENERIC_ERROR,
	GEDIT_TAB_STATE_CLOSING,
	GEDIT_TAB_STATE_EXTERNALLY_MODIFIED_NOTIFICATION,
	GEDIT_TAB_NUM_OF_STATES /* This is not a valid state */
} GeditTabState;

#define GEDIT_TYPE_TAB              (gedit_tab_get_type())
#define GEDIT_TAB(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), GEDIT_TYPE_TAB, GeditTab))
#define GEDIT_TAB_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), GEDIT_TYPE_TAB, GeditTabClass))
#define GEDIT_IS_TAB(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), GEDIT_TYPE_TAB))
#define GEDIT_IS_TAB_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GEDIT_TYPE_TAB))
#define GEDIT_TAB_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), GEDIT_TYPE_TAB, GeditTabClass))

typedef struct _GeditTab	GeditTab;
typedef struct _GeditTabClass	GeditTabClass;
typedef struct _GeditTabPrivate	GeditTabPrivate;

struct _GeditTab
{
	GtkBox vbox;

	/*< private >*/
	GeditTabPrivate *priv;
};

struct _GeditTabClass
{
	GtkBoxClass parent_class;

	void (* drop_uris)	(GeditView *view,
				 gchar    **uri_list);
};

GType 		 gedit_tab_get_type 		(void) G_GNUC_CONST;

GeditView	*gedit_tab_get_view		(GeditTab            *tab);

/* This is only an helper function */
GeditDocument	*gedit_tab_get_document		(GeditTab            *tab);

GeditTab	*gedit_tab_get_from_document	(GeditDocument       *doc);

GeditTabState	 gedit_tab_get_state		(GeditTab	     *tab);

gboolean	 gedit_tab_get_auto_save_enabled
						(GeditTab            *tab);

void		 gedit_tab_set_auto_save_enabled
						(GeditTab            *tab,
						 gboolean            enable);

gint		 gedit_tab_get_auto_save_interval
						(GeditTab            *tab);

void		 gedit_tab_set_auto_save_interval
						(GeditTab            *tab,
						 gint                interval);

void		 gedit_tab_set_info_bar		(GeditTab            *tab,
						 GtkWidget           *info_bar);
/*
 * Non exported methods
 */
GtkWidget 	*_gedit_tab_new 		(void);

/* Whether create is TRUE, creates a new empty document if location does
   not refer to an existing location */
GtkWidget	*_gedit_tab_new_from_location	(GFile               *location,
						 const GeditEncoding *encoding,
						 gint                 line_pos,
						 gint                 column_pos,
						 gboolean             create);

GtkWidget	*_gedit_tab_new_from_stream	(GInputStream        *stream,
						 const GeditEncoding *encoding,
						 gint                 line_pos,
						 gint                 column_pos);

gchar 		*_gedit_tab_get_name		(GeditTab            *tab);
gchar 		*_gedit_tab_get_tooltip		(GeditTab            *tab);
GdkPixbuf 	*_gedit_tab_get_icon		(GeditTab            *tab);
void		 _gedit_tab_load		(GeditTab            *tab,
						 GFile               *location,
						 const GeditEncoding *encoding,
						 gint                 line_pos,
						 gint                 column_pos,
						 gboolean             create);

void		 _gedit_tab_load_stream		(GeditTab            *tab,
						 GInputStream        *location,
						 const GeditEncoding *encoding,
						 gint                 line_pos,
						 gint                 column_pos);

void		 _gedit_tab_revert		(GeditTab            *tab);
void		 _gedit_tab_save		(GeditTab            *tab);
void		 _gedit_tab_save_as		(GeditTab            *tab,
						 GFile               *location,
						 const GeditEncoding *encoding,
						 GeditDocumentNewlineType newline_type,
						 GeditDocumentCompressionType compression_type);

void		 _gedit_tab_print		(GeditTab            *tab);
void		 _gedit_tab_print_preview	(GeditTab            *tab);

void		 _gedit_tab_mark_for_closing	(GeditTab	     *tab);

gboolean	 _gedit_tab_get_can_close	(GeditTab	     *tab);

GtkWidget	*_gedit_tab_get_view_frame	(GeditTab            *tab);

void		 _gedit_tab_set_network_available
						(GeditTab	     *tab,
						 gboolean	     enable);

G_END_DECLS

#endif  /* __GEDIT_TAB_H__  */

/* ex:set ts=8 noet: */
