/*
 * gedit-open-document-selector-store.h
 * This file is part of gedit
 *
 * Copyright (C) 2015 - Sébastien Lafargue
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

#ifndef __GEDIT_OPEN_DOCUMENT_SELECTOR_STORE_H__
#define __GEDIT_OPEN_DOCUMENT_SELECTOR_STORE_H__

#include "gedit-open-document-selector-helper.h"
#include "gedit-open-document-selector.h"

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GEDIT_TYPE_OPEN_DOCUMENT_SELECTOR_STORE			(gedit_open_document_selector_store_get_type ())
#define GEDIT_OPEN_DOCUMENT_SELECTOR_STORE(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), GEDIT_TYPE_OPEN_DOCUMENT_SELECTOR_STORE, GeditOpenDocumentSelectorStore))
#define GEDIT_IS_OPEN_DOCUMENT_SELECTOR_STORE(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GEDIT_TYPE_OPEN_DOCUMENT_SELECTOR_STORE))
#define GEDIT_OPEN_DOCUMENT_SELECTOR_STORE_CONST(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GEDIT_TYPE_OPEN_DOCUMENT_SELECTOR_STORE, GeditOpenDocumentSelectorStore const))
#define GEDIT_OPEN_DOCUMENT_SELECTOR_STORE_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), GEDIT_TYPE_OPEN_DOCUMENT_SELECTOR_STORE, GeditOpenDocumentSelectorStoreClass))
#define GEDIT_IS_OPEN_DOCUMENT_SELECTOR_STORE_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GEDIT_TYPE_OPEN_DOCUMENT_SELECTOR_STORE))
#define GEDIT_OPEN_DOCUMENT_SELECTOR_STORE_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), GEDIT_TYPE_OPEN_DOCUMENT_SELECTOR_STORE, GeditOpenDocumentSelectorStoreClass))

typedef struct _GeditOpenDocumentSelectorStore			GeditOpenDocumentSelectorStore;
typedef struct _GeditOpenDocumentSelectorStoreClass		GeditOpenDocumentSelectorStoreClass;
typedef struct _GeditOpenDocumentSelectorStorePrivate		GeditOpenDocumentSelectorStorePrivate;

struct _GeditOpenDocumentSelectorStore
{
	GtkBox parent;

	GeditOpenDocumentSelectorStorePrivate *priv;
};

struct _GeditOpenDocumentSelectorStoreClass
{
	GtkBoxClass  parent_class;
};

#define GEDIT_OPEN_DOCUMENT_SELECTOR_STORE_ERROR gedit_open_document_selector_store_error_quark ()

typedef enum
{
	TYPE_OUT_OF_RANGE
} GeditOpenDocumentSelectorStoreError;

GQuark				 gedit_open_document_selector_store_error_quark				(void);

GType				 gedit_open_document_selector_store_get_type				(void) G_GNUC_CONST;

gint				 gedit_open_document_selector_store_get_recent_limit			(GeditOpenDocumentSelectorStore *store);

void				 gedit_open_document_selector_store_set_recent_filter			(GeditOpenDocumentSelectorStore *store,
                                                                                                         gchar                          *filter);

gchar				*gedit_open_document_selector_store_get_recent_filter			(GeditOpenDocumentSelectorStore *store);

GList				*gedit_open_document_selector_store_update_list_finish			(GeditOpenDocumentSelectorStore  *open_document_selector_store,
                                                                                                         GAsyncResult                    *res,
                                                                                                         GError                         **error);

void				gedit_open_document_selector_store_update_list_async			(GeditOpenDocumentSelectorStore *open_document_selector_store,
                                                                                                         GeditOpenDocumentSelector      *open_document_selector,
                                                                                                         GCancellable                   *cancellable,
                                                                                                         GAsyncReadyCallback             callback,
                                                                                                         ListType                        type,
                                                                                                         gpointer                        user_data);

GeditOpenDocumentSelectorStore	*gedit_open_document_selector_store_get_default				(void);

G_END_DECLS

#endif /* __GEDIT_OPEN_DOCUMENT_SELECTOR_STORE_H__ */

/* ex:set ts=8 noet: */
