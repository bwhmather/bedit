/*
 * gedit-file-browser-widget.h - Bedit plugin providing easy file access
 * from the sidepanel
 *
 * Copyright (C) 2006 - Jesse van den Kieboom <jesse@icecrew.nl>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GEDIT_FILE_BROWSER_WIDGET_H
#define GEDIT_FILE_BROWSER_WIDGET_H

#include <gtk/gtk.h>
#include <gedit/gedit-menu-extension.h>
#include "gedit-file-browser-store.h"
#include "gedit-file-bookmarks-store.h"
#include "gedit-file-browser-view.h"

G_BEGIN_DECLS
#define GEDIT_TYPE_FILE_BROWSER_WIDGET			(gedit_file_browser_widget_get_type ())
#define GEDIT_FILE_BROWSER_WIDGET(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), GEDIT_TYPE_FILE_BROWSER_WIDGET, BeditFileBrowserWidget))
#define GEDIT_FILE_BROWSER_WIDGET_CONST(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GEDIT_TYPE_FILE_BROWSER_WIDGET, BeditFileBrowserWidget const))
#define GEDIT_FILE_BROWSER_WIDGET_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), GEDIT_TYPE_FILE_BROWSER_WIDGET, BeditFileBrowserWidgetClass))
#define GEDIT_IS_FILE_BROWSER_WIDGET(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GEDIT_TYPE_FILE_BROWSER_WIDGET))
#define GEDIT_IS_FILE_BROWSER_WIDGET_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GEDIT_TYPE_FILE_BROWSER_WIDGET))
#define GEDIT_FILE_BROWSER_WIDGET_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), GEDIT_TYPE_FILE_BROWSER_WIDGET, BeditFileBrowserWidgetClass))

typedef struct _BeditFileBrowserWidget        BeditFileBrowserWidget;
typedef struct _BeditFileBrowserWidgetClass   BeditFileBrowserWidgetClass;
typedef struct _BeditFileBrowserWidgetPrivate BeditFileBrowserWidgetPrivate;

typedef
gboolean (*BeditFileBrowserWidgetFilterFunc) (BeditFileBrowserWidget *obj,
					      BeditFileBrowserStore  *model,
					      GtkTreeIter            *iter,
					      gpointer                user_data);

struct _BeditFileBrowserWidget
{
	GtkBox parent;

	BeditFileBrowserWidgetPrivate *priv;
};

struct _BeditFileBrowserWidgetClass
{
	GtkBoxClass parent_class;

	/* Signals */
	void (* location_activated)	(BeditFileBrowserWidget *widget,
					 GFile                  *location);
	void (* error)			(BeditFileBrowserWidget *widget,
					 guint                   code,
					 gchar const            *message);
	gboolean (* confirm_delete)	(BeditFileBrowserWidget *widget,
					 BeditFileBrowserStore  *model,
					 GList                  *list);
	gboolean (* confirm_no_trash)	(BeditFileBrowserWidget *widget,
					 GList                  *list);
	void (* open_in_terminal)       (BeditFileBrowserWidget *widget,
	                                 GFile                  *location);
	void (* set_active_root)        (BeditFileBrowserWidget *widget);
};

GType		 gedit_file_browser_widget_get_type            (void) G_GNUC_CONST;

GtkWidget	*gedit_file_browser_widget_new            	(void);

void		 gedit_file_browser_widget_show_bookmarks       (BeditFileBrowserWidget *obj);
void		 gedit_file_browser_widget_show_files           (BeditFileBrowserWidget *obj);

void		 gedit_file_browser_widget_set_root		(BeditFileBrowserWidget *obj,
								 GFile                  *root,
								 gboolean                virtual_root);
void		gedit_file_browser_widget_set_root_and_virtual_root
								(BeditFileBrowserWidget *obj,
								 GFile                  *root,
								 GFile                  *virtual_root);

gboolean	 gedit_file_browser_widget_get_selected_directory
								(BeditFileBrowserWidget *obj,
								 GtkTreeIter            *iter);

void             gedit_file_browser_widget_set_active_root_enabled (BeditFileBrowserWidget *widget,
                                                                    gboolean                enabled);

BeditFileBrowserStore *
gedit_file_browser_widget_get_browser_store         		(BeditFileBrowserWidget *obj);
BeditFileBookmarksStore *
gedit_file_browser_widget_get_bookmarks_store			(BeditFileBrowserWidget *obj);
BeditFileBrowserView *
gedit_file_browser_widget_get_browser_view			(BeditFileBrowserWidget *obj);
GtkWidget *
gedit_file_browser_widget_get_filter_entry			(BeditFileBrowserWidget *obj);

gulong gedit_file_browser_widget_add_filter			(BeditFileBrowserWidget *obj,
								 BeditFileBrowserWidgetFilterFunc func,
								 gpointer                user_data,
								 GDestroyNotify          notify);
void		 gedit_file_browser_widget_remove_filter	(BeditFileBrowserWidget *obj,
								 gulong                  id);
void		 gedit_file_browser_widget_set_filter_pattern	(BeditFileBrowserWidget *obj,
								 gchar const            *pattern);
BeditMenuExtension *
		 gedit_file_browser_widget_extend_context_menu	(BeditFileBrowserWidget *obj);
void		 gedit_file_browser_widget_refresh		(BeditFileBrowserWidget *obj);
void		 gedit_file_browser_widget_history_back		(BeditFileBrowserWidget *obj);
void		 gedit_file_browser_widget_history_forward	(BeditFileBrowserWidget *obj);

void		 _gedit_file_browser_widget_register_type      (GTypeModule            *type_module);

G_END_DECLS

#endif /* GEDIT_FILE_BROWSER_WIDGET_H */
/* ex:set ts=8 noet: */
