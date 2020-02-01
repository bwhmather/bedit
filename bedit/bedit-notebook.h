/*
 * bedit-notebook.h
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * Based on gedit-notebook.h from Gedit.
 *
 * Copyright (C) 2005 - Paolo Maggi
 * Copyright (C) 2005-2014 - Paolo Borelli
 * Copyright (C) 2008 - Joe M Smith
 * Copyright (C) 2010 - Garrett Regier, Steve Frécinaux
 * Copyright (C) 2010-2011 - Jesse van den Kieboom
 * Copyright (C) 2010-2014 - Ignacio Casal Quinteiro
 * Copyright (C) 2013-2015 - Sébastien Wilmet
 *
 * gedit-notebook.h is a modified version of ephy-notebook.h from Epiphany.
 *
 * Copyright (C) 2002 - Christophe Fergeau
 * Copyright (C) 2003 - Marco Pesenti Gritti
 * Copyright (C) 2003-2004 - Christian Persch
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

#ifndef BEDIT_NOTEBOOK_H
#define BEDIT_NOTEBOOK_H

#include <bedit/bedit-tab.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define BEDIT_TYPE_NOTEBOOK (bedit_notebook_get_type())
#define BEDIT_NOTEBOOK(o)                                                      \
    (G_TYPE_CHECK_INSTANCE_CAST((o), BEDIT_TYPE_NOTEBOOK, BeditNotebook))
#define BEDIT_NOTEBOOK_CLASS(k)                                                \
    (G_TYPE_CHECK_CLASS_CAST((k), BEDIT_TYPE_NOTEBOOK, BeditNotebookClass))
#define BEDIT_IS_NOTEBOOK(o)                                                   \
    (G_TYPE_CHECK_INSTANCE_TYPE((o), BEDIT_TYPE_NOTEBOOK))
#define BEDIT_IS_NOTEBOOK_CLASS(k)                                             \
    (G_TYPE_CHECK_CLASS_TYPE((k), BEDIT_TYPE_NOTEBOOK))
#define BEDIT_NOTEBOOK_GET_CLASS(o)                                            \
    (G_TYPE_INSTANCE_GET_CLASS((o), BEDIT_TYPE_NOTEBOOK, BeditNotebookClass))

typedef struct _BeditNotebook BeditNotebook;
typedef struct _BeditNotebookClass BeditNotebookClass;
typedef struct _BeditNotebookPrivate BeditNotebookPrivate;

/* This is now used in multi-notebook but we keep the same enum for
 * backward compatibility since it is used in the gsettings schema */
typedef enum {
    BEDIT_NOTEBOOK_SHOW_TABS_NEVER,
    BEDIT_NOTEBOOK_SHOW_TABS_AUTO,
    BEDIT_NOTEBOOK_SHOW_TABS_ALWAYS
} BeditNotebookShowTabsModeType;

struct _BeditNotebook {
    GtkNotebook notebook;

    /*< private >*/
    BeditNotebookPrivate *priv;
};

struct _BeditNotebookClass {
    GtkNotebookClass parent_class;

    /* Signals */
    void (*tab_close_request)(BeditNotebook *notebook, BeditTab *tab);
    void (*show_popup_menu)(
        BeditNotebook *notebook, GdkEvent *event, BeditTab *tab);
    gboolean (*change_to_page)(BeditNotebook *notebook, gint page_num);
};

GType bedit_notebook_get_type(void) G_GNUC_CONST;

GtkWidget *bedit_notebook_new(void);

void bedit_notebook_add_tab(
    BeditNotebook *nb, BeditTab *tab, gint position, gboolean jump_to);

void bedit_notebook_move_tab(
    BeditNotebook *src, BeditNotebook *dest, BeditTab *tab, gint dest_position);

void bedit_notebook_set_active_tab(BeditNotebook *nb, BeditTab *tab);

BeditTab *bedit_notebook_get_active_tab(BeditNotebook *nb);

gint bedit_notebook_get_n_tabs(BeditNotebook *nb);

void bedit_notebook_foreach_tab(
    BeditNotebook *nb, GtkCallback callback, gpointer callback_data);

GList *bedit_notebook_get_all_tabs(BeditNotebook *nb);

void bedit_notebook_close_tabs(BeditNotebook *nb, const GList *tabs);

void bedit_notebook_close_all_tabs(BeditNotebook *nb);


G_END_DECLS

#endif /* BEDIT_NOTEBOOK_H */

