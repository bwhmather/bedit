/*
 * bedit-multi-notebook.h
 * This file is part of bedit
 *
 * Copyright (C) 2010 - Ignacio Casal Quinteiro
 *
 * bedit is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * bedit is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with bedit; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#ifndef GEDIT_MULTI_NOTEBOOK_H
#define GEDIT_MULTI_NOTEBOOK_H

#include <gtk/gtk.h>

#include "bedit-notebook.h"
#include "bedit-tab.h"

G_BEGIN_DECLS

#define GEDIT_TYPE_MULTI_NOTEBOOK (bedit_multi_notebook_get_type())
#define GEDIT_MULTI_NOTEBOOK(obj)                                              \
    (G_TYPE_CHECK_INSTANCE_CAST(                                               \
        (obj), GEDIT_TYPE_MULTI_NOTEBOOK, BeditMultiNotebook))
#define GEDIT_MULTI_NOTEBOOK_CONST(obj)                                        \
    (G_TYPE_CHECK_INSTANCE_CAST(                                               \
        (obj), GEDIT_TYPE_MULTI_NOTEBOOK, BeditMultiNotebook const))
#define GEDIT_MULTI_NOTEBOOK_CLASS(klass)                                      \
    (G_TYPE_CHECK_CLASS_CAST(                                                  \
        (klass), GEDIT_TYPE_MULTI_NOTEBOOK, BeditMultiNotebookClass))
#define GEDIT_IS_MULTI_NOTEBOOK(obj)                                           \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GEDIT_TYPE_MULTI_NOTEBOOK))
#define GEDIT_IS_MULTI_NOTEBOOK_CLASS(klass)                                   \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GEDIT_TYPE_MULTI_NOTEBOOK))
#define GEDIT_MULTI_NOTEBOOK_GET_CLASS(obj)                                    \
    (G_TYPE_INSTANCE_GET_CLASS(                                                \
        (obj), GEDIT_TYPE_MULTI_NOTEBOOK, BeditMultiNotebookClass))

typedef struct _BeditMultiNotebook BeditMultiNotebook;
typedef struct _BeditMultiNotebookClass BeditMultiNotebookClass;
typedef struct _BeditMultiNotebookPrivate BeditMultiNotebookPrivate;

struct _BeditMultiNotebook {
    GtkGrid parent;

    BeditMultiNotebookPrivate *priv;
};

struct _BeditMultiNotebookClass {
    GtkGridClass parent_class;

    /* Signals */
    void (*notebook_added)(BeditMultiNotebook *mnb, BeditNotebook *notebook);
    void (*notebook_removed)(BeditMultiNotebook *mnb, BeditNotebook *notebook);
    void (*tab_added)(
        BeditMultiNotebook *mnb, BeditNotebook *notebook, BeditTab *tab);
    void (*tab_removed)(
        BeditMultiNotebook *mnb, BeditNotebook *notebook, BeditTab *tab);
    void (*switch_tab)(
        BeditMultiNotebook *mnb, BeditNotebook *old_notebook, BeditTab *old_tab,
        BeditNotebook *new_notebook, BeditTab *new_tab);
    void (*tab_close_request)(
        BeditMultiNotebook *mnb, BeditNotebook *notebook, BeditTab *tab);
    GtkNotebook *(*create_window)(
        BeditMultiNotebook *mnb, BeditNotebook *notebook, GtkWidget *page,
        gint x, gint y);
    void (*page_reordered)(BeditMultiNotebook *mnb);
    void (*show_popup_menu)(
        BeditMultiNotebook *mnb, GdkEvent *event, BeditTab *tab);
};

GType bedit_multi_notebook_get_type(void) G_GNUC_CONST;

BeditMultiNotebook *bedit_multi_notebook_new(void);

BeditNotebook *bedit_multi_notebook_get_active_notebook(
    BeditMultiNotebook *mnb);

gint bedit_multi_notebook_get_n_notebooks(BeditMultiNotebook *mnb);

BeditNotebook *bedit_multi_notebook_get_nth_notebook(
    BeditMultiNotebook *mnb, gint notebook_num);

BeditNotebook *bedit_multi_notebook_get_notebook_for_tab(
    BeditMultiNotebook *mnb, BeditTab *tab);

gint bedit_multi_notebook_get_notebook_num(
    BeditMultiNotebook *mnb, BeditNotebook *notebook);

gint bedit_multi_notebook_get_n_tabs(BeditMultiNotebook *mnb);

gint bedit_multi_notebook_get_page_num(BeditMultiNotebook *mnb, BeditTab *tab);

BeditTab *bedit_multi_notebook_get_active_tab(BeditMultiNotebook *mnb);
void bedit_multi_notebook_set_active_tab(
    BeditMultiNotebook *mnb, BeditTab *tab);

void bedit_multi_notebook_set_current_page(
    BeditMultiNotebook *mnb, gint page_num);

GList *bedit_multi_notebook_get_all_tabs(BeditMultiNotebook *mnb);

void bedit_multi_notebook_close_tabs(
    BeditMultiNotebook *mnb, const GList *tabs);

void bedit_multi_notebook_close_all_tabs(BeditMultiNotebook *mnb);

void bedit_multi_notebook_add_new_notebook(BeditMultiNotebook *mnb);

void bedit_multi_notebook_add_new_notebook_with_tab(
    BeditMultiNotebook *mnb, BeditTab *tab);

void bedit_multi_notebook_remove_active_notebook(BeditMultiNotebook *mnb);

void bedit_multi_notebook_previous_notebook(BeditMultiNotebook *mnb);
void bedit_multi_notebook_next_notebook(BeditMultiNotebook *mnb);

void bedit_multi_notebook_foreach_notebook(
    BeditMultiNotebook *mnb, GtkCallback callback, gpointer callback_data);

void bedit_multi_notebook_foreach_tab(
    BeditMultiNotebook *mnb, GtkCallback callback, gpointer callback_data);

void _bedit_multi_notebook_set_show_tabs(
    BeditMultiNotebook *mnb, gboolean show);

G_END_DECLS

#endif /* GEDIT_MULTI_NOTEBOOK_H */

/* ex:set ts=8 noet: */
