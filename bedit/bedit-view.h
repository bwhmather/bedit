/*
 * bedit-view.h
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * Based on gedit-view.h from Gedit.
 *
 * Copyright (C) 1998-1999 - Alex Roberts, Evan Lawrence
 * Copyright (C) 2000-2001 - Chema Celorio
 * Copyright (C) 2001 - Carlos Perelló Marín
 * Copyright (C) 2001-2008 - Paolo Maggi
 * Copyright (C) 2005-2015 - Paolo Borelli
 * Copyright (C) 2007-2010 - Jesse van den Kieboom
 * Copyright (C) 2010 - Garrett Regier, Steve Frécinaux
 * Copyright (C) 2011 - Ignacio Casal Quinteiro
 * Copyright (C) 2013-2016 - Sébastien Wilmet
 * Copyright (C) 2018 - Sebastien Lafargue
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

#ifndef BEDIT_VIEW_H
#define BEDIT_VIEW_H

#include <gtk/gtk.h>

#include <bedit/bedit-document.h>
#include <gtksourceview/gtksource.h>

G_BEGIN_DECLS

#define BEDIT_TYPE_VIEW (bedit_view_get_type())
#define BEDIT_VIEW(obj)                                                     \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), BEDIT_TYPE_VIEW, BeditView))
#define BEDIT_VIEW_CLASS(klass)                                             \
    (G_TYPE_CHECK_CLASS_CAST((klass), BEDIT_TYPE_VIEW, BeditViewClass))
#define BEDIT_IS_VIEW(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), BEDIT_TYPE_VIEW))
#define BEDIT_IS_VIEW_CLASS(klass)                                          \
    (G_TYPE_CHECK_CLASS_TYPE((klass), BEDIT_TYPE_VIEW))
#define BEDIT_VIEW_GET_CLASS(obj)                                           \
    (G_TYPE_INSTANCE_GET_CLASS((obj), BEDIT_TYPE_VIEW, BeditViewClass))

typedef struct _BeditView BeditView;
typedef struct _BeditViewClass BeditViewClass;
typedef struct _BeditViewPrivate BeditViewPrivate;

struct _BeditView {
    GtkSourceView view;

    /*< private >*/
    BeditViewPrivate *priv;
};

struct _BeditViewClass {
    GtkSourceViewClass parent_class;

    void (*drop_uris)(BeditView *view, gchar **uri_list);

    gpointer padding;
};

GType bedit_view_get_type(void) G_GNUC_CONST;

GtkWidget *bedit_view_new(BeditDocument *doc);

void bedit_view_cut_clipboard(BeditView *view);
void bedit_view_copy_clipboard(BeditView *view);
void bedit_view_paste_clipboard(BeditView *view);
void bedit_view_delete_selection(BeditView *view);
void bedit_view_select_all(BeditView *view);

void bedit_view_scroll_to_cursor(BeditView *view);

void bedit_view_set_font(
    BeditView *view, gboolean default_font, const gchar *font_name
);

G_END_DECLS

#endif /* BEDIT_VIEW_H */

