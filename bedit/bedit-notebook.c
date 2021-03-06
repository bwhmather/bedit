/*
 * bedit-notebook.c
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * Based on gedit-notebook.c from Gedit.
 *
 * Copyright (C) 2005-2015 - Paolo Borelli
 * Copyright (C) 2006 - Paolo Maggi
 * Copyright (C) 2006-2010 - Steve Frécinaux
 * Copyright (C) 2008 - Joe M Smith
 * Copyright (C) 2008-2014 - Ignacio Casal Quinteiro
 * Copyright (C) 2009-2010 - Javier Jardón
 * Copyright (C) 2010 - Garrett Regier
 * Copyright (C) 2010-2011 - Jesse van den Kieboom
 * Copyright (C) 2012 - bharaththiruveedula
 * Copyright (C) 2013 - William Jon McCann
 * Copyright (C) 2013-2016 - Sébastien Wilmet
 * Copyright (C) 2014 - Robert Roth, Sebastien Lafargue
 * Copyright (C) 2015 - Matthias Clasen
 * Copyright (C) 2018 - Corey Daley
 *
 * gedit-notebook.c is a modified version of ephy-notebook.c from Epiphany.
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

#include "bedit-notebook.h"
#include "bedit-tab-label.h"

#define BEDIT_NOTEBOOK_GROUP_NAME "BeditNotebookGroup"

/* The DND targets defined in BeditView start at 100.
 * Those defined in GtkSourceView start at 200.
 */
#define TARGET_TAB 150

struct _BeditNotebookPrivate {
    /* History of focused pages. The first element contains the most recent
     * one.
     */
    GList *focused_pages;

    guint ignore_focused_page_update : 1;
};

G_DEFINE_TYPE_WITH_PRIVATE(BeditNotebook, bedit_notebook, GTK_TYPE_NOTEBOOK)

enum { TAB_CLOSE_REQUEST, SHOW_POPUP_MENU, CHANGE_TO_PAGE, LAST_SIGNAL };

static guint signals[LAST_SIGNAL] = {0};

static void bedit_notebook_finalize(GObject *object) {
    BeditNotebook *notebook = BEDIT_NOTEBOOK(object);

    g_list_free(notebook->priv->focused_pages);

    G_OBJECT_CLASS(bedit_notebook_parent_class)->finalize(object);
}

static void bedit_notebook_grab_focus(GtkWidget *widget) {
    GtkNotebook *notebook = GTK_NOTEBOOK(widget);
    gint current_page;
    GtkWidget *tab;

    current_page = gtk_notebook_get_current_page(notebook);
    tab = gtk_notebook_get_nth_page(notebook, current_page);

    if (tab != NULL) {
        gtk_widget_grab_focus(tab);
    }
}

static gint find_tab_num_at_pos(
    GtkNotebook *notebook, gint screen_x, gint screen_y
) {
    GtkPositionType tab_pos;
    GtkAllocation tab_allocation;
    gint page_num;

    tab_pos = gtk_notebook_get_tab_pos(notebook);

    for (page_num = 0;; page_num++) {
        GtkWidget *page;
        GtkWidget *tab_label;
        gint max_x, max_y, x_root, y_root;

        page = gtk_notebook_get_nth_page(notebook, page_num);

        if (page == NULL) {
            break;
        }

        tab_label = gtk_notebook_get_tab_label(notebook, page);
        g_return_val_if_fail(tab_label != NULL, -1);

        if (!gtk_widget_get_mapped(tab_label)) {
            continue;
        }

        gdk_window_get_origin(
            gtk_widget_get_window(tab_label), &x_root, &y_root
        );

        gtk_widget_get_allocation(tab_label, &tab_allocation);
        max_x = x_root + tab_allocation.x + tab_allocation.width;
        max_y = y_root + tab_allocation.y + tab_allocation.height;

        if (
            (tab_pos == GTK_POS_TOP || tab_pos == GTK_POS_BOTTOM) &&
            screen_x <= max_x
        ) {
            return page_num;
        }

        if (
            (tab_pos == GTK_POS_LEFT || tab_pos == GTK_POS_RIGHT) &&
            screen_y <= max_y
        ) {
            return page_num;
        }
    }

    return -1;
}

static gboolean bedit_notebook_button_press_event(
    GtkWidget *widget, GdkEventButton *event
) {
    GtkNotebook *notebook = GTK_NOTEBOOK(widget);

    if (
        event->type == GDK_BUTTON_PRESS &&
        (event->state & gtk_accelerator_get_default_mod_mask()) == 0
    ) {
        gint tab_clicked;

        tab_clicked = find_tab_num_at_pos(
            notebook, event->x_root, event->y_root
        );
        if (tab_clicked >= 0) {
            GtkWidget *tab;

            tab = gtk_notebook_get_nth_page(notebook, tab_clicked);
            switch (event->button) {
            case GDK_BUTTON_SECONDARY:
                g_signal_emit(
                    G_OBJECT(widget), signals[SHOW_POPUP_MENU], 0, event, tab
                );
                return GDK_EVENT_STOP;

            case GDK_BUTTON_MIDDLE:
                g_signal_emit(
                    G_OBJECT(notebook), signals[TAB_CLOSE_REQUEST], 0, tab
                );
                return GDK_EVENT_STOP;

            default:
                break;
            }
        }
    }

    return GTK_WIDGET_CLASS(bedit_notebook_parent_class)
        ->button_press_event(widget, event);
}

/*
 * We need to override this because when we don't show the tabs, like in
 * fullscreen we need to have wrap around too
 */
static gboolean bedit_notebook_change_current_page(
    GtkNotebook *notebook, gint offset
) {
    gint current;

    current = gtk_notebook_get_current_page(notebook);

    if (current != -1) {
        gint target;
        gboolean wrap_around;

        target = current + offset;

        g_object_get(
            gtk_widget_get_settings(GTK_WIDGET(notebook)),
            "gtk-keynav-wrap-around", &wrap_around, NULL
        );

        if (wrap_around) {
            if (target < 0) {
                target = gtk_notebook_get_n_pages(notebook) - 1;
            } else if (target >= gtk_notebook_get_n_pages(notebook)) {
                target = 0;
            }
        }

        gtk_notebook_set_current_page(notebook, target);
    } else {
        gtk_widget_error_bell(GTK_WIDGET(notebook));
    }

    return TRUE;
}

static void bedit_notebook_switch_page(
    GtkNotebook *notebook, GtkWidget *page, guint page_num
) {
    BeditNotebookPrivate *priv = BEDIT_NOTEBOOK(notebook)->priv;

    GTK_NOTEBOOK_CLASS(bedit_notebook_parent_class)
        ->switch_page(notebook, page, page_num);

    if (!priv->ignore_focused_page_update) {
        /* Get again page_num and page, the signal handler may have
         * changed them.
         */
        page_num = gtk_notebook_get_current_page(notebook);
        if (page_num != -1) {
            page = gtk_notebook_get_nth_page(notebook, page_num);
            g_assert(page != NULL);

            /* Remove the old page, we dont want to grow unnecessarily
             * the list.
             */
            priv->focused_pages = g_list_remove(priv->focused_pages, page);

            priv->focused_pages = g_list_prepend(priv->focused_pages, page);
        }
    }

    /* give focus to the tab */
    gtk_widget_grab_focus(page);
}

static void close_button_clicked_cb(
    BeditTabLabel *tab_label, BeditNotebook *notebook
) {
    BeditTab *tab;

    tab = bedit_tab_label_get_tab(tab_label);
    g_signal_emit(notebook, signals[TAB_CLOSE_REQUEST], 0, tab);
}

static void switch_to_last_focused_page(
    BeditNotebook *notebook, BeditTab *tab
) {
    if (notebook->priv->focused_pages != NULL) {
        GList *node;
        GtkWidget *page;
        gint page_num;

        node = notebook->priv->focused_pages;
        page = GTK_WIDGET(node->data);

        page_num = gtk_notebook_page_num(GTK_NOTEBOOK(notebook), page);
        g_return_if_fail(page_num != -1);

        gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), page_num);
    }
}

static void drag_data_received_cb(
    GtkWidget *widget, GdkDragContext *context, gint x, gint y,
    GtkSelectionData *selection_data, guint info, guint timestamp
) {
    GtkWidget *notebook;
    GtkWidget *new_notebook;
    GtkWidget *page;

    if (info != TARGET_TAB) {
        return;
    }

    notebook = gtk_drag_get_source_widget(context);

    if (!GTK_IS_WIDGET(notebook)) {
        return;
    }

    page = *(GtkWidget **)gtk_selection_data_get_data(selection_data);
    g_return_if_fail(page != NULL);

    /* We need to iterate and get the notebook of the target view
     * because we can have several notebooks per window.
     */
    new_notebook = gtk_widget_get_ancestor(widget, BEDIT_TYPE_NOTEBOOK);
    g_return_if_fail(new_notebook != NULL);

    if (notebook != new_notebook) {
        bedit_notebook_move_tab(
            BEDIT_NOTEBOOK(notebook), BEDIT_NOTEBOOK(new_notebook),
            BEDIT_TAB(page), 0
        );
    }

    gtk_drag_finish(context, TRUE, TRUE, timestamp);
}

static void bedit_notebook_page_removed(
    GtkNotebook *notebook, GtkWidget *page, guint page_num
) {
    BeditNotebookPrivate *priv = BEDIT_NOTEBOOK(notebook)->priv;
    gboolean current_page;

    /* The page removed was the current page. */
    current_page = (
        priv->focused_pages != NULL && priv->focused_pages->data == page
    );

    priv->focused_pages = g_list_remove(priv->focused_pages, page);

    if (current_page) {
        switch_to_last_focused_page(BEDIT_NOTEBOOK(notebook), BEDIT_TAB(page));
    }
}

static void bedit_notebook_page_added(
    GtkNotebook *notebook, GtkWidget *page, guint page_num
) {
    GtkWidget *tab_label;
    BeditView *view;

    g_return_if_fail(BEDIT_IS_TAB(page));

    tab_label = gtk_notebook_get_tab_label(notebook, page);
    g_return_if_fail(BEDIT_IS_TAB_LABEL(tab_label));

    /* For a DND from one notebook to another, the same tab_label can be
     * used, so we need to connect the signal here.
     * More precisely, the same tab_label is used when the drop zone is in
     * the tab labels (not the BeditView), that is, when the DND is handled
     * by the GtkNotebook implementation.
     */
    g_signal_connect(
        tab_label, "close-clicked",
        G_CALLBACK(close_button_clicked_cb), notebook
    );

    view = bedit_tab_get_view(BEDIT_TAB(page));
    g_signal_connect(
        view, "drag-data-received",
        G_CALLBACK(drag_data_received_cb), NULL
    );
}

static void bedit_notebook_remove(GtkContainer *container, GtkWidget *widget) {
    GtkNotebook *notebook = GTK_NOTEBOOK(container);
    BeditNotebookPrivate *priv = BEDIT_NOTEBOOK(container)->priv;
    GtkWidget *tab_label;
    BeditView *view;

    g_return_if_fail(BEDIT_IS_TAB(widget));

    tab_label = gtk_notebook_get_tab_label(notebook, widget);
    g_return_if_fail(BEDIT_IS_TAB_LABEL(tab_label));

    /* For a DND from one notebook to another, the same tab_label can be
     * used, so we need to disconnect the signal.
     */
    g_signal_handlers_disconnect_by_func(
        tab_label, G_CALLBACK(close_button_clicked_cb), notebook
    );

    view = bedit_tab_get_view(BEDIT_TAB(widget));
    g_signal_handlers_disconnect_by_func(view, drag_data_received_cb, NULL);

    /* This is where GtkNotebook will remove the page. By doing so, it
     * will also switch to a new page, messing up our focus list. So we
     * set a flag here to ignore the switch temporarily.
     */
    priv->ignore_focused_page_update = TRUE;

    if (GTK_CONTAINER_CLASS(bedit_notebook_parent_class)->remove != NULL) {
        GTK_CONTAINER_CLASS(bedit_notebook_parent_class)
            ->remove(container, widget);
    }

    priv->ignore_focused_page_update = FALSE;
}

static gboolean bedit_notebook_change_to_page(
    BeditNotebook *notebook, gint page_num
) {
    gint n_pages;

    n_pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook));

    if (page_num >= n_pages) {
        return FALSE;
    }

    gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), page_num);

    return TRUE;
}

static void bedit_notebook_class_init(BeditNotebookClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
    GtkContainerClass *container_class = GTK_CONTAINER_CLASS(klass);
    GtkNotebookClass *notebook_class = GTK_NOTEBOOK_CLASS(klass);
    GtkBindingSet *binding_set;
    gint i;

    object_class->finalize = bedit_notebook_finalize;

    widget_class->grab_focus = bedit_notebook_grab_focus;
    widget_class->button_press_event = bedit_notebook_button_press_event;

    container_class->remove = bedit_notebook_remove;

    notebook_class->change_current_page = bedit_notebook_change_current_page;
    notebook_class->switch_page = bedit_notebook_switch_page;
    notebook_class->page_removed = bedit_notebook_page_removed;
    notebook_class->page_added = bedit_notebook_page_added;

    klass->change_to_page = bedit_notebook_change_to_page;
    /*
    signals[TAB_ADDED] = g_signal_new(
        "tab-added", G_OBJECT_CLASS_TYPE(object_class), G_SIGNAL_RUN_FIRST,
        G_STRUCT_OFFSET(BeditNotebookClass, tab_added), NULL, NULL, NULL,
        G_TYPE_NONE, 2, BEDIT_TYPE_NOTEBOOK, BEDIT_TYPE_TAB
    );

    signals[TAB_REMOVED] = g_signal_new(
        "tab-removed", G_OBJECT_CLASS_TYPE(object_class), G_SIGNAL_RUN_FIRST,
        G_STRUCT_OFFSET(BeditNotebookClass, tab_removed), NULL, NULL, NULL,
        G_TYPE_NONE, 2, BEDIT_TYPE_NOTEBOOK, BEDIT_TYPE_TAB
    );

    signals[SWITCH_TAB] = g_signal_new(
        "switch-tab", G_OBJECT_CLASS_TYPE(object_class), G_SIGNAL_RUN_FIRST,
        G_STRUCT_OFFSET(BeditNotebookClass, switch_tab), NULL, NULL, NULL,
        G_TYPE_NONE, 4, BEDIT_TYPE_NOTEBOOK, BEDIT_TYPE_TAB,
        BEDIT_TYPE_NOTEBOOK, BEDIT_TYPE_TAB
    );
    */
    signals[TAB_CLOSE_REQUEST] = g_signal_new(
        "tab-close-request", G_OBJECT_CLASS_TYPE(object_class),
        G_SIGNAL_RUN_LAST,
        G_STRUCT_OFFSET(BeditNotebookClass, tab_close_request), NULL, NULL,
        NULL, G_TYPE_NONE, 1, BEDIT_TYPE_TAB
    );

    signals[SHOW_POPUP_MENU] = g_signal_new(
        "show-popup-menu", G_OBJECT_CLASS_TYPE(object_class),
        G_SIGNAL_RUN_FIRST,
        G_STRUCT_OFFSET(BeditNotebookClass, show_popup_menu), NULL, NULL, NULL,
        G_TYPE_NONE, 2, GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE,
        BEDIT_TYPE_TAB
    );

    signals[CHANGE_TO_PAGE] = g_signal_new(
        "change-to-page", G_TYPE_FROM_CLASS(object_class),
        G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
        G_STRUCT_OFFSET(BeditNotebookClass, change_to_page), NULL, NULL, NULL,
        G_TYPE_BOOLEAN, 1, G_TYPE_INT
    );

    binding_set = gtk_binding_set_by_class(klass);
    for (i = 1; i < 10; i++) {
        gtk_binding_entry_add_signal(
            binding_set, GDK_KEY_0 + i, GDK_MOD1_MASK, "change-to-page", 1,
            G_TYPE_INT, i - 1
        );
    }
}

/**
 * bedit_notebook_new:
 *
 * Creates a new #BeditNotebook object.
 *
 * Returns: a new #BeditNotebook
 */
GtkWidget *bedit_notebook_new(void) {
    return GTK_WIDGET(g_object_new(BEDIT_TYPE_NOTEBOOK, NULL));
}

static void bedit_notebook_init(BeditNotebook *notebook) {
    notebook->priv = bedit_notebook_get_instance_private(notebook);

    gtk_notebook_set_scrollable(GTK_NOTEBOOK(notebook), TRUE);
    gtk_notebook_set_show_border(GTK_NOTEBOOK(notebook), FALSE);
    gtk_notebook_set_show_tabs(GTK_NOTEBOOK(notebook), TRUE);
    gtk_notebook_set_group_name(
        GTK_NOTEBOOK(notebook), BEDIT_NOTEBOOK_GROUP_NAME
    );
    gtk_container_set_border_width(GTK_CONTAINER(notebook), 0);
}

/**
 * bedit_notebook_add_tab:
 * @notebook: a #BeditNotebook
 * @tab: a #BeditTab
 * @position: the position where the @tab should be added
 * @jump_to: %TRUE to set the @tab as active
 *
 * Adds the specified @tab to the @notebook.
 */
void bedit_notebook_add_tab(
    BeditNotebook *notebook, BeditTab *tab, gint position, gboolean jump_to
) {
    GtkWidget *tab_label;
    BeditView *view;
    GtkTargetList *target_list;

    g_return_if_fail(BEDIT_IS_NOTEBOOK(notebook));
    g_return_if_fail(BEDIT_IS_TAB(tab));

    tab_label = bedit_tab_label_new(tab);

    gtk_notebook_insert_page(
        GTK_NOTEBOOK(notebook), GTK_WIDGET(tab), tab_label, position
    );

    gtk_notebook_set_tab_reorderable(
        GTK_NOTEBOOK(notebook), GTK_WIDGET(tab), TRUE
    );

    gtk_notebook_set_tab_detachable(
        GTK_NOTEBOOK(notebook), GTK_WIDGET(tab), TRUE
    );

    gtk_container_child_set(
        GTK_CONTAINER(notebook), GTK_WIDGET(tab), "tab-expand", TRUE, NULL
    );

    /* Drag and drop support: move a tab to another notebook, with the drop
     * zone in the BeditView. The drop zone in the tab labels is already
     * implemented by GtkNotebook.
     */
    view = bedit_tab_get_view(tab);
    target_list = gtk_drag_dest_get_target_list(GTK_WIDGET(view));

    if (target_list != NULL) {
        gtk_target_list_add(
            target_list, gdk_atom_intern_static_string("GTK_NOTEBOOK_TAB"),
            GTK_TARGET_SAME_APP, TARGET_TAB
        );
    }

    /* The signal handler may have reordered the tabs */
    position = gtk_notebook_page_num(GTK_NOTEBOOK(notebook), GTK_WIDGET(tab));

    if (jump_to) {
        gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), position);
        gtk_widget_grab_focus(GTK_WIDGET(tab));
    }
}

/**
 * bedit_notebook_move_tab:
 * @src: a #BeditNotebook
 * @dest: a #BeditNotebook
 * @tab: a #BeditTab
 * @dest_position: the position for @tab
 *
 * Moves @tab from @src to @dest.
 * If @dest_position is greater than or equal to the number of tabs
 * of the destination nootebook or negative, tab will be moved to the
 * end of the tabs.
 */
void bedit_notebook_move_tab(
    BeditNotebook *src, BeditNotebook *dest, BeditTab *tab,
    gint dest_position
) {
    g_return_if_fail(BEDIT_IS_NOTEBOOK(src));
    g_return_if_fail(BEDIT_IS_NOTEBOOK(dest));
    g_return_if_fail(src != dest);
    g_return_if_fail(BEDIT_IS_TAB(tab));

    /* Make sure the tab isn't destroyed while we move it. */
    g_object_ref(tab);

    /* Make sure the @src notebook isn't destroyed during the tab
     * detachment, to prevent a crash in gtk_notebook_detach_tab(). In fact,
     * if @tab is the last tab of @src, and if @src is not the last notebook
     * of the BeditMultiNotebook, then @src will be destroyed when
     * gtk_container_remove() is called by gtk_notebook_detach_tab().
     */
    g_object_ref(src);
    gtk_notebook_detach_tab(GTK_NOTEBOOK(src), GTK_WIDGET(tab));
    g_object_unref(src);

    bedit_notebook_add_tab(dest, tab, dest_position, TRUE);

    g_object_unref(tab);
}

void bedit_notebook_set_active_tab(BeditNotebook *nb, BeditTab *tab) {
    gint page_num;

    g_return_if_fail(BEDIT_IS_NOTEBOOK(nb));
    g_return_if_fail(BEDIT_IS_TAB(tab));

    page_num = gtk_notebook_page_num(GTK_NOTEBOOK(nb), GTK_WIDGET(tab));
    g_return_if_fail(page_num != -1);

    gtk_notebook_set_current_page(GTK_NOTEBOOK(nb), page_num);
}

BeditTab *bedit_notebook_get_active_tab(BeditNotebook *nb) {
    gint page_num;

    g_return_val_if_fail(BEDIT_IS_NOTEBOOK(nb), NULL);

    page_num = gtk_notebook_get_current_page(GTK_NOTEBOOK(nb));
    if (page_num == -1) {
        return NULL;
    }

    return BEDIT_TAB(gtk_notebook_get_nth_page(GTK_NOTEBOOK(nb), page_num));
}

gint bedit_notebook_get_n_tabs(BeditNotebook *nb) {
    g_return_val_if_fail(BEDIT_IS_NOTEBOOK(nb), -1);

    return gtk_notebook_get_n_pages(GTK_NOTEBOOK(nb));
}

void bedit_notebook_foreach_tab(
    BeditNotebook *nb, GtkCallback callback, gpointer callback_data
) {

    g_return_if_fail(BEDIT_IS_NOTEBOOK(nb));

    gtk_container_foreach(GTK_CONTAINER(nb), callback, callback_data);
}

GList *bedit_notebook_get_all_tabs(BeditNotebook *nb) {
    g_return_val_if_fail(BEDIT_IS_NOTEBOOK(nb), NULL);

    return gtk_container_get_children(GTK_CONTAINER(nb));
}

void bedit_notebook_close_tabs(BeditNotebook *nb, const GList *tabs) {
    GList *l;

    g_return_if_fail(BEDIT_IS_NOTEBOOK(nb));

    for (l = (GList *)tabs; l != NULL; l = g_list_next(l)) {
        gint page_num;

        page_num = gtk_notebook_page_num(
            GTK_NOTEBOOK(nb), GTK_WIDGET(l->data)
        );

        if (page_num != -1) {
            gtk_container_remove(GTK_CONTAINER(nb), GTK_WIDGET(l->data));
        }
    }
}

/**
 * bedit_notebook_close_all_tabs:
 * @notebook: a #BeditNotebook
 *
 * Removes all #BeditTab from @notebook.
 */
void bedit_notebook_close_all_tabs(BeditNotebook *notebook) {
    GList *tabs;
    GList *t;

    g_return_if_fail(BEDIT_IS_NOTEBOOK(notebook));

    g_list_free(notebook->priv->focused_pages);
    notebook->priv->focused_pages = NULL;

    /* Remove tabs in reverse order since it is faster
     * due to how GtkNotebook works.
     */
    tabs = gtk_container_get_children(GTK_CONTAINER(notebook));
    for (t = g_list_last(tabs); t != NULL; t = t->prev) {
        GtkWidget *tab = t->data;
        gtk_container_remove(GTK_CONTAINER(notebook), tab);
    }

    g_list_free(tabs);
}

