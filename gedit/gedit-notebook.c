/*
 * gedit-notebook.c
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

/* This file is a modified version of the epiphany file ephy-notebook.c
 * Here the relevant copyright:
 *
 *  Copyright (C) 2002 Christophe Fergeau
 *  Copyright (C) 2003 Marco Pesenti Gritti
 *  Copyright (C) 2003, 2004 Christian Persch
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib-object.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "gedit-notebook.h"
#include "gedit-tab.h"
#include "gedit-tab-label.h"
#include "gedit-window.h"
#include "gedit-marshal.h"

#define GEDIT_NOTEBOOK_GROUP_NAME "GeditNotebookGroup"

struct _GeditNotebookPrivate
{
	GList         *focused_pages;

	guint close_buttons_sensitive : 1;
	guint ignore_focused_page_update : 1;
};

G_DEFINE_TYPE_WITH_PRIVATE (GeditNotebook, gedit_notebook, GTK_TYPE_NOTEBOOK)

/* Signals */
enum
{
	TAB_CLOSE_REQUEST,
	SHOW_POPUP_MENU,
	CHANGE_TO_PAGE,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void
gedit_notebook_finalize (GObject *object)
{
	GeditNotebook *notebook = GEDIT_NOTEBOOK (object);

	g_list_free (notebook->priv->focused_pages);

	G_OBJECT_CLASS (gedit_notebook_parent_class)->finalize (object);
}

static void
gedit_notebook_grab_focus (GtkWidget *widget)
{
	GtkNotebook *nb = GTK_NOTEBOOK (widget);
	GtkWidget *tab;
	gint current_page;

	current_page = gtk_notebook_get_current_page (nb);
	tab = gtk_notebook_get_nth_page (nb, current_page);

	gtk_widget_grab_focus (tab);
}

static gint
find_tab_num_at_pos (GtkNotebook *notebook,
                     gint         screen_x,
                     gint         screen_y)
{
	GtkNotebook *nb = GTK_NOTEBOOK (notebook);
	GtkPositionType tab_pos;
	GtkWidget *page;
	GtkAllocation tab_allocation;
	gint page_num = 0;

	tab_pos = gtk_notebook_get_tab_pos (GTK_NOTEBOOK (notebook));

	while ((page = gtk_notebook_get_nth_page (nb, page_num)))
	{
		GtkWidget *tab;
		gint max_x, max_y, x_root, y_root;

		tab = gtk_notebook_get_tab_label (nb, page);
		g_return_val_if_fail (tab != NULL, -1);

		if (!gtk_widget_get_mapped (GTK_WIDGET (tab)))
		{
			page_num++;
			continue;
		}

		gdk_window_get_origin (gtk_widget_get_window (tab), &x_root, &y_root);

		gtk_widget_get_allocation (tab, &tab_allocation);
		max_x = x_root + tab_allocation.x + tab_allocation.width;
		max_y = y_root + tab_allocation.y + tab_allocation.height;

		if ((tab_pos == GTK_POS_TOP || tab_pos == GTK_POS_BOTTOM) && screen_x <= max_x)
		{
			return page_num;
		}

		if ((tab_pos == GTK_POS_LEFT || tab_pos == GTK_POS_RIGHT) && screen_y <= max_y)
		{
			return page_num;
		}

		page_num++;
	}

	return -1;
}

static gboolean
gedit_notebook_button_press (GtkWidget      *widget,
                             GdkEventButton *event)
{
	GtkNotebook *nb = GTK_NOTEBOOK (widget);

	if (event->type == GDK_BUTTON_PRESS &&
	    event->button == GDK_BUTTON_SECONDARY &&
	    (event->state & gtk_accelerator_get_default_mod_mask ()) == 0)
	{
		gint tab_clicked;

		tab_clicked = find_tab_num_at_pos (nb, event->x_root, event->y_root);
		if (tab_clicked >= 0)
		{
			GtkWidget *tab;

			tab = gtk_notebook_get_nth_page (nb, tab_clicked);

			g_signal_emit (G_OBJECT (widget), signals[SHOW_POPUP_MENU], 0, event, tab);

			return TRUE;
		}
	}

	return GTK_WIDGET_CLASS (gedit_notebook_parent_class)->button_press_event (widget, event);
}

/*
 * We need to override this because when we don't show the tabs, like in
 * fullscreen we need to have wrap around too
 */
static gboolean
gedit_notebook_change_current_page (GtkNotebook *notebook,
				    gint         offset)
{
	gboolean wrap_around;
	gint current;

	current = gtk_notebook_get_current_page (notebook);

	if (current != -1)
	{
		current = current + offset;

		g_object_get (gtk_widget_get_settings (GTK_WIDGET (notebook)),
			      "gtk-keynav-wrap-around", &wrap_around,
			      NULL);

		if (wrap_around)
		{
			if (current < 0)
			{
				current = gtk_notebook_get_n_pages (notebook) - 1;
			}
			else if (current >= gtk_notebook_get_n_pages (notebook))
			{
				current = 0;
			}
		}

		gtk_notebook_set_current_page (notebook, current);
	}
	else
	{
		gtk_widget_error_bell (GTK_WIDGET (notebook));
	}

	return TRUE;
}

static void
gedit_notebook_switch_page (GtkNotebook *notebook,
                            GtkWidget   *page,
                            guint        page_num)
{
	GeditNotebook *nb = GEDIT_NOTEBOOK (notebook);

	if (!nb->priv->ignore_focused_page_update)
	{
		gint prev_page;
		GtkWidget *previous_page;

		prev_page = gtk_notebook_get_current_page (notebook);
		previous_page = gtk_notebook_get_nth_page (notebook, prev_page);

		/* Remove the old page, we dont want to grow unnecessarily
		 * the list */
		if (nb->priv->focused_pages)
		{
			nb->priv->focused_pages =
				g_list_remove (nb->priv->focused_pages, previous_page);
		}

		nb->priv->focused_pages = g_list_append (nb->priv->focused_pages,
		                                         previous_page);
	}

	GTK_NOTEBOOK_CLASS (gedit_notebook_parent_class)->switch_page (notebook, page, page_num);

	/* give focus to the tab */
	gtk_widget_grab_focus (page);
}

static void
on_tab_label_destroyed (GtkWidget *tab_label,
                        GeditTab  *tab)
{
	g_object_set_data (G_OBJECT (tab), "tab-label", NULL);
}

static void
close_button_clicked_cb (GeditTabLabel *tab_label, GeditNotebook *notebook)
{
	GeditTab *tab;

	tab = gedit_tab_label_get_tab (tab_label);
	g_signal_emit (notebook, signals[TAB_CLOSE_REQUEST], 0, tab);
}

static void
smart_tab_switching_on_closure (GeditNotebook *nb,
				GeditTab      *tab)
{
	if (nb->priv->focused_pages)
	{
		GList *l;
		GtkWidget *child;
		gint page_num;

		/* activate the last focused tab */
		l = g_list_last (nb->priv->focused_pages);
		child = GTK_WIDGET (l->data);

		page_num = gtk_notebook_page_num (GTK_NOTEBOOK (nb),
		                                  child);

		gtk_notebook_set_current_page (GTK_NOTEBOOK (nb),
		                               page_num);
	}
}

static GtkWidget *
get_tab_label (GeditTab *tab)
{
	GObject *tab_label;

	tab_label = g_object_get_data (G_OBJECT (tab), "tab-label");

	return (tab_label != NULL) ? GTK_WIDGET (tab_label) : NULL;
}

static void
gedit_notebook_page_removed (GtkNotebook *notebook,
                             GtkWidget   *page,
                             guint        page_num)
{
	GeditNotebook *nb = GEDIT_NOTEBOOK (notebook);
	gint curr;
	GtkWidget *tab_label;

	tab_label = get_tab_label (GEDIT_TAB (page));

	if (tab_label != NULL)
	{
		g_signal_handlers_disconnect_by_func (tab_label,
		                                      G_CALLBACK (on_tab_label_destroyed),
		                                      page);
		g_signal_handlers_disconnect_by_func (tab_label,
		                                      G_CALLBACK (close_button_clicked_cb),
		                                      nb);
	}

	/* Remove the page from the focused pages list */
	nb->priv->focused_pages =  g_list_remove (nb->priv->focused_pages,
	                                          page);

	curr = gtk_notebook_get_current_page (notebook);

	if (page_num == curr)
	{
		smart_tab_switching_on_closure (nb, GEDIT_TAB (page));
	}
}

static void
gedit_notebook_page_added (GtkNotebook *notebook,
                           GtkWidget   *page,
                           guint        page_num)
{
	GeditNotebook *nb = GEDIT_NOTEBOOK (notebook);
	GtkWidget *tab_label;

	tab_label = get_tab_label (GEDIT_TAB (page));

	g_signal_connect (tab_label,
	                  "destroy",
	                  G_CALLBACK (on_tab_label_destroyed),
	                  page);

	g_signal_connect (tab_label,
	                  "close-clicked",
	                  G_CALLBACK (close_button_clicked_cb),
	                  nb);
}

static void
gedit_notebook_remove (GtkContainer *container,
                       GtkWidget    *widget)
{
	GeditNotebook *nb;

	/* This is where GtkNotebook will remove the page. By doing so, it
	   will also switch to a new page, messing up our focus list. So we
	   set a flag here to ignore the switch temporarily */

	nb = GEDIT_NOTEBOOK (container);

	nb->priv->ignore_focused_page_update = TRUE;

	GTK_CONTAINER_CLASS (gedit_notebook_parent_class)->remove (container,
	                                                           widget);

	nb->priv->ignore_focused_page_update = FALSE;
}

static gboolean
gedit_notebook_change_to_page (GeditNotebook *notebook,
                               gint           page_num)
{
	gint n_pages;

	n_pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (notebook));

	if (page_num > n_pages - 1)
	{
		return FALSE;
	}

	gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook),
	                               page_num);

	return TRUE;
}

static void
gedit_notebook_class_init (GeditNotebookClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *gtkwidget_class = GTK_WIDGET_CLASS (klass);
	GtkNotebookClass *notebook_class = GTK_NOTEBOOK_CLASS (klass);
	GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);
	GtkBindingSet *binding_set;
	gint i;

	object_class->finalize = gedit_notebook_finalize;

	gtkwidget_class->grab_focus = gedit_notebook_grab_focus;
	gtkwidget_class->button_press_event = gedit_notebook_button_press;

	notebook_class->change_current_page = gedit_notebook_change_current_page;
	notebook_class->switch_page = gedit_notebook_switch_page;
	notebook_class->page_removed = gedit_notebook_page_removed;
	notebook_class->page_added = gedit_notebook_page_added;

	container_class->remove = gedit_notebook_remove;

	klass->change_to_page = gedit_notebook_change_to_page;

	signals[TAB_CLOSE_REQUEST] =
		g_signal_new ("tab-close-request",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GeditNotebookClass, tab_close_request),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1,
			      GEDIT_TYPE_TAB);

	signals[SHOW_POPUP_MENU] =
		g_signal_new ("show-popup-menu",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GeditNotebookClass, show_popup_menu),
			      NULL, NULL,
			      gedit_marshal_VOID__BOXED_OBJECT,
			      G_TYPE_NONE,
			      2,
			      GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE,
			      GEDIT_TYPE_TAB);
	signals[CHANGE_TO_PAGE] =
		g_signal_new ("change-to-page",
		              G_TYPE_FROM_CLASS (object_class),
		              G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		              G_STRUCT_OFFSET (GeditNotebookClass, change_to_page),
		              NULL, NULL,
		              gedit_marshal_BOOLEAN__INT,
		              G_TYPE_BOOLEAN, 1,
		              G_TYPE_INT);

	binding_set = gtk_binding_set_by_class (klass);
	for (i = 1; i < 10; i++)
	{
		gtk_binding_entry_add_signal (binding_set,
		                              GDK_KEY_0 + i, GDK_MOD1_MASK,
		                              "change-to-page", 1,
		                              G_TYPE_INT, i - 1);
	}
}

/**
 * gedit_notebook_new:
 *
 * Creates a new #GeditNotebook object.
 *
 * Returns: a new #GeditNotebook
 */
GtkWidget *
gedit_notebook_new (void)
{
	return GTK_WIDGET (g_object_new (GEDIT_TYPE_NOTEBOOK, NULL));
}

static void
gedit_notebook_init (GeditNotebook *notebook)
{
	GeditNotebookPrivate *priv;

	notebook->priv = gedit_notebook_get_instance_private (notebook);
	priv = notebook->priv;

	priv->close_buttons_sensitive = TRUE;

	gtk_notebook_set_scrollable (GTK_NOTEBOOK (notebook), TRUE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (notebook), FALSE);
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (notebook), TRUE);
	gtk_notebook_set_group_name (GTK_NOTEBOOK (notebook),
	                             GEDIT_NOTEBOOK_GROUP_NAME);
	gtk_container_set_border_width (GTK_CONTAINER (notebook), 0);
}

static GtkWidget *
create_tab_label (GeditNotebook *nb,
		  GeditTab      *tab)
{
	GtkWidget *tab_label;

	tab_label = gedit_tab_label_new (tab);

	g_object_set_data (G_OBJECT (tab), "tab-label", tab_label);

	return tab_label;
}

/**
 * gedit_notebook_add_tab:
 * @nb: a #GeditNotebook
 * @tab: a #GeditTab
 * @position: the position where the @tab should be added
 * @jump_to: %TRUE to set the @tab as active
 *
 * Adds the specified @tab to the @nb.
 */
void
gedit_notebook_add_tab (GeditNotebook *nb,
		        GeditTab      *tab,
		        gint           position,
		        gboolean       jump_to)
{
	GtkWidget *tab_label;

	g_return_if_fail (GEDIT_IS_NOTEBOOK (nb));
	g_return_if_fail (GEDIT_IS_TAB (tab));

	tab_label = create_tab_label (nb, tab);

	gtk_notebook_insert_page (GTK_NOTEBOOK (nb),
				  GTK_WIDGET (tab),
				  tab_label,
				  position);
	gtk_notebook_set_tab_reorderable (GTK_NOTEBOOK (nb),
	                                  GTK_WIDGET (tab),
	                                  TRUE);
	gtk_notebook_set_tab_detachable (GTK_NOTEBOOK (nb),
	                                 GTK_WIDGET (tab),
	                                 TRUE);
	gtk_container_child_set (GTK_CONTAINER (nb),
				 GTK_WIDGET (tab),
				 "tab-expand", TRUE,
				 NULL);

	/* The signal handler may have reordered the tabs */
	position = gtk_notebook_page_num (GTK_NOTEBOOK (nb),
					  GTK_WIDGET (tab));

	if (jump_to)
	{
		gtk_notebook_set_current_page (GTK_NOTEBOOK (nb), position);
		gtk_widget_grab_focus (GTK_WIDGET (tab));
	}
}

/**
 * gedit_notebook_move_tab:
 * @src: a #GeditNotebook
 * @dest: a #GeditNotebook
 * @tab: a #GeditTab
 * @dest_position: the position for @tab
 *
 * Moves @tab from @src to @dest.
 * If dest_position is greater than or equal to the number of tabs
 * of the destination nootebook or negative, tab will be moved to the
 * end of the tabs.
 */
void
gedit_notebook_move_tab (GeditNotebook *src,
                         GeditNotebook *dest,
                         GeditTab      *tab,
                         gint           dest_position)
{
	g_return_if_fail (GEDIT_IS_NOTEBOOK (src));
	g_return_if_fail (GEDIT_IS_NOTEBOOK (dest));
	g_return_if_fail (src != dest);
	g_return_if_fail (GEDIT_IS_TAB (tab));

	/* make sure the tab isn't destroyed while we move it */
	g_object_ref (tab);
	gtk_container_remove (GTK_CONTAINER (src), GTK_WIDGET (tab));
	gedit_notebook_add_tab (dest, tab, dest_position, TRUE);
	g_object_unref (tab);
}

/**
 * gedit_notebook_remove_all_tabs:
 * @nb: a #GeditNotebook
 *
 * Removes all #GeditTab from @nb.
 */
void
gedit_notebook_remove_all_tabs (GeditNotebook *nb)
{
	GList *tabs, *t;

	g_return_if_fail (GEDIT_IS_NOTEBOOK (nb));

	g_list_free (nb->priv->focused_pages);
	nb->priv->focused_pages = NULL;

	/* Remove tabs in reverse order since it is faster
	 * due to how gtknotebook works */
	tabs = gtk_container_get_children (GTK_CONTAINER (nb));
	for (t = g_list_last (tabs); t != NULL; t = t->prev)
	{
		gtk_container_remove (GTK_CONTAINER (nb), GTK_WIDGET (t->data));
	}

	g_list_free (tabs);
}

static void
set_close_buttons_sensitivity (GeditTab      *tab,
                               GeditNotebook *nb)
{
	GtkWidget *tab_label;

	tab_label = get_tab_label (tab);

	gedit_tab_label_set_close_button_sensitive (GEDIT_TAB_LABEL (tab_label),
						    nb->priv->close_buttons_sensitive);
}

/**
 * gedit_notebook_set_close_buttons_sensitive:
 * @nb: a #GeditNotebook
 * @sensitive: %TRUE to make the buttons sensitive
 *
 * Sets whether the close buttons in the tabs of @nb are sensitive.
 */
void
gedit_notebook_set_close_buttons_sensitive (GeditNotebook *nb,
					    gboolean       sensitive)
{
	g_return_if_fail (GEDIT_IS_NOTEBOOK (nb));

	sensitive = (sensitive != FALSE);

	if (sensitive == nb->priv->close_buttons_sensitive)
		return;

	nb->priv->close_buttons_sensitive = sensitive;

	gtk_container_foreach (GTK_CONTAINER (nb),
			       (GtkCallback)set_close_buttons_sensitivity,
			       nb);
}

/**
 * gedit_notebook_get_close_buttons_sensitive:
 * @nb: a #GeditNotebook
 *
 * Whether the close buttons are sensitive.
 *
 * Returns: %TRUE if the close buttons are sensitive
 */
gboolean
gedit_notebook_get_close_buttons_sensitive (GeditNotebook *nb)
{
	g_return_val_if_fail (GEDIT_IS_NOTEBOOK (nb), TRUE);

	return nb->priv->close_buttons_sensitive;
}

/* ex:set ts=8 noet: */
