/*
 * gedit-open-document-selector.h
 * This file is part of gedit
 *
 * Copyright (C) 2014 - Sébastien Lafargue
 *
 * gedit is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * gedit is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with gedit. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GEDIT_OPEN_DOCUMENT_SELECTOR_H__
#define __GEDIT_OPEN_DOCUMENT_SELECTOR_H__

#include <glib-object.h>
#include "gedit-window.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GEDIT_TYPE_OPEN_DOCUMENT_SELECTOR		(gedit_open_document_selector_get_type ())
#define GEDIT_OPEN_DOCUMENT_SELECTOR(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GEDIT_TYPE_OPEN_DOCUMENT_SELECTOR, GeditOpenDocumentSelector))
#define GEDIT_IS_OPEN_DOCUMENT_SELECTOR(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GEDIT_TYPE_OPEN_DOCUMENT_SELECTOR))
#define GEDIT_OPEN_DOCUMENT_SELECTOR_CONST(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GEDIT_TYPE_OPEN_DOCUMENT_SELECTOR, GeditOpenDocumentSelector const))
#define GEDIT_OPEN_DOCUMENT_SELECTOR_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), GEDIT_TYPE_OPEN_DOCUMENT_SELECTOR, GeditOpenDocumentSelectorClass))
#define GEDIT_IS_OPEN_DOCUMENT_SELECTOR_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GEDIT_TYPE_OPEN_DOCUMENT_SELECTOR))
#define GEDIT_OPEN_DOCUMENT_SELECTOR_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), GEDIT_TYPE_OPEN_DOCUMENT_SELECTOR, GeditOpenDocumentSelectorClass))

typedef struct _GeditOpenDocumentSelector		GeditOpenDocumentSelector;
typedef struct _GeditOpenDocumentSelectorClass		GeditOpenDocumentSelectorClass;
typedef struct _GeditOpenDocumentSelectorPrivate	GeditOpenDocumentSelectorPrivate;

struct _GeditOpenDocumentSelector
{
	GtkBox parent;

	GtkWidget *recent_search_entry;

	GeditOpenDocumentSelectorPrivate *priv;
};

struct _GeditOpenDocumentSelectorClass
{
	GtkBoxClass  parent_class;

	/* Signals */
	void (* recent_file_activated)	(GeditOpenDocumentSelector *open_document_selector, gchar *uri);
};

GType gedit_open_document_selector_get_type		(void) G_GNUC_CONST;

GeditOpenDocumentSelector * gedit_open_document_selector_new			(void);

G_END_DECLS

#endif /* __GEDIT_OPEN_DOCUMENT_SELECTOR_H__ */

/* ex:set ts=8 noet: */
