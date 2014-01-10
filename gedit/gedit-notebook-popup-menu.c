/*
 * gedit-notebook-popup-menu.c
 * This file is part of gedit
 *
 * Copyright (C) 2011 - Ignacio Casal Quinteiro
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


#include "gedit-notebook-popup-menu.h"
#include "gedit-multi-notebook.h"
#include "gedit-commands.h"
#include <glib/gi18n.h>

struct _GeditNotebookPopupMenuPrivate
{
	GeditWindow *window;
	GeditTab *tab;

	GtkWidget *move_left_item;
	GtkWidget *move_right_item;
	GtkWidget *move_to_new_tab_group_item;
	GtkWidget *move_to_new_window_item;
	GtkWidget *close_item;
};

enum
{
	PROP_0,
	PROP_WINDOW,
	PROP_TAB
};

G_DEFINE_TYPE_WITH_PRIVATE (GeditNotebookPopupMenu, gedit_notebook_popup_menu, GTK_TYPE_MENU)

static void
gedit_notebook_popup_menu_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
	GeditNotebookPopupMenu *menu = GEDIT_NOTEBOOK_POPUP_MENU (object);

	switch (prop_id)
	{
		case PROP_WINDOW:
			menu->priv->window = GEDIT_WINDOW (g_value_get_object (value));
			break;

		case PROP_TAB:
			menu->priv->tab = GEDIT_TAB (g_value_get_object (value));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gedit_notebook_popup_menu_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
	GeditNotebookPopupMenu *menu = GEDIT_NOTEBOOK_POPUP_MENU (object);

	switch (prop_id)
	{
		case PROP_WINDOW:
			g_value_set_object (value, menu->priv->window);
			break;

		case PROP_TAB:
			g_value_set_object (value, menu->priv->tab);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gedit_notebook_popup_menu_finalize (GObject *object)
{
	G_OBJECT_CLASS (gedit_notebook_popup_menu_parent_class)->finalize (object);
}

static void
update_sensitivity (GeditNotebookPopupMenu *menu)
{
	GeditTabState state;
	GeditMultiNotebook *mnb;
	GtkNotebook *notebook;
	gint page_num;
	gint n_pages;
	guint n_tabs;

	state = gedit_tab_get_state (menu->priv->tab);

	mnb = GEDIT_MULTI_NOTEBOOK (_gedit_window_get_multi_notebook (menu->priv->window));

	notebook = GTK_NOTEBOOK (gedit_multi_notebook_get_notebook_for_tab (mnb, menu->priv->tab));
	n_pages = gtk_notebook_get_n_pages (notebook);
	page_num = gtk_notebook_page_num (notebook, GTK_WIDGET (menu->priv->tab));

	gtk_widget_set_sensitive (menu->priv->close_item,
				  (state != GEDIT_TAB_STATE_CLOSING) &&
				  (state != GEDIT_TAB_STATE_SAVING) &&
				  (state != GEDIT_TAB_STATE_SHOWING_PRINT_PREVIEW) &&
				  (state != GEDIT_TAB_STATE_PRINTING) &&
				  (state != GEDIT_TAB_STATE_PRINT_PREVIEWING) &&
				  (state != GEDIT_TAB_STATE_SAVING_ERROR));

	gtk_widget_set_sensitive (menu->priv->move_to_new_tab_group_item, n_pages > 1);

	n_tabs = gedit_multi_notebook_get_n_tabs(mnb);
	gtk_widget_set_sensitive (menu->priv->move_to_new_window_item, n_tabs > 1);

	gtk_widget_set_sensitive (menu->priv->move_left_item, page_num > 0);
	gtk_widget_set_sensitive (menu->priv->move_right_item, page_num < n_pages - 1);
}

static void
gedit_notebook_popup_menu_constructed (GObject *object)
{
	GeditNotebookPopupMenu *menu = GEDIT_NOTEBOOK_POPUP_MENU (object);

	update_sensitivity (menu);

	G_OBJECT_CLASS (gedit_notebook_popup_menu_parent_class)->constructed (object);
}

static void
gedit_notebook_popup_menu_class_init (GeditNotebookPopupMenuClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->get_property = gedit_notebook_popup_menu_get_property;
	object_class->set_property = gedit_notebook_popup_menu_set_property;
	object_class->constructed = gedit_notebook_popup_menu_constructed;
	object_class->finalize = gedit_notebook_popup_menu_finalize;

	g_object_class_install_property (object_class,
	                                 PROP_WINDOW,
	                                 g_param_spec_object ("window",
	                                                      "Window",
	                                                      "The GeditWindow",
	                                                      GEDIT_TYPE_WINDOW,
	                                                      G_PARAM_READWRITE |
	                                                      G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (object_class,
	                                 PROP_TAB,
	                                 g_param_spec_object ("tab",
	                                                      "Tab",
	                                                      "The GeditTab",
	                                                      GEDIT_TYPE_TAB,
	                                                      G_PARAM_READWRITE |
	                                                      G_PARAM_CONSTRUCT_ONLY));
}

static void
on_file_close_menuitem_activate (GtkMenuItem            *menuitem,
                                 GeditNotebookPopupMenu *menu)
{
	_gedit_cmd_file_close_tab (menu->priv->tab, menu->priv->window);
}

static void
on_move_to_new_tab_group_menuitem_activate (GtkMenuItem            *menuitem,
                                            GeditNotebookPopupMenu *menu)
{
	_gedit_window_move_tab_to_new_tab_group (menu->priv->window,
	                                         menu->priv->tab);
}

static void
on_move_to_new_window_menuitem_activate (GtkMenuItem            *menuitem,
                                         GeditNotebookPopupMenu *menu)
{
	_gedit_window_move_tab_to_new_window (menu->priv->window,
	                                      menu->priv->tab);
}

static void
on_move_right_menuitem_activate (GtkMenuItem            *menuitem,
                                 GeditNotebookPopupMenu *menu)
{
	GeditMultiNotebook *mnb;
	GtkNotebook *notebook;
	gint page_num;
	gint n_pages;

	mnb = GEDIT_MULTI_NOTEBOOK (_gedit_window_get_multi_notebook (menu->priv->window));

	notebook = GTK_NOTEBOOK (gedit_multi_notebook_get_notebook_for_tab (mnb, menu->priv->tab));
	n_pages = gtk_notebook_get_n_pages (notebook);
	page_num = gtk_notebook_page_num (notebook, GTK_WIDGET (menu->priv->tab));

	if (page_num <  (n_pages - 1))
	{
		gtk_notebook_reorder_child (notebook, GTK_WIDGET (menu->priv->tab),
		                            page_num + 1);
	}
}

static void
on_move_left_menuitem_activate (GtkMenuItem            *menuitem,
                                GeditNotebookPopupMenu *menu)
{
	GeditMultiNotebook *mnb;
	GtkNotebook *notebook;
	gint page_num;

	mnb = GEDIT_MULTI_NOTEBOOK (_gedit_window_get_multi_notebook (menu->priv->window));

	notebook = GTK_NOTEBOOK (gedit_multi_notebook_get_notebook_for_tab (mnb, menu->priv->tab));
	page_num = gtk_notebook_page_num (notebook, GTK_WIDGET (menu->priv->tab));

	if (page_num > 0)
	{
		gtk_notebook_reorder_child (notebook, GTK_WIDGET (menu->priv->tab),
		                            page_num - 1);
	}
}

static void
gedit_notebook_popup_menu_init (GeditNotebookPopupMenu *menu)
{
	GtkWidget *menu_item;

	menu->priv = gedit_notebook_popup_menu_get_instance_private (menu);

	/* Keep in sync with the respective GtkActions */
	menu_item = gtk_menu_item_new_with_mnemonic (_("_Close"));
	g_signal_connect (menu_item, "activate",
	                  G_CALLBACK (on_file_close_menuitem_activate),
	                  menu);
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), menu_item);
	gtk_widget_show (menu_item);
	menu->priv->close_item = menu_item;

	menu_item = gtk_separator_menu_item_new ();
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), menu_item);
	gtk_widget_show (menu_item);

	menu_item = gtk_menu_item_new_with_mnemonic (_("_Move to New Tab Group"));
	g_signal_connect (menu_item, "activate",
	                  G_CALLBACK (on_move_to_new_tab_group_menuitem_activate),
	                  menu);
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), menu_item);
	gtk_widget_show (menu_item);
	menu->priv->move_to_new_tab_group_item = menu_item;

	menu_item = gtk_menu_item_new_with_mnemonic (_("_Move to New Window"));
	g_signal_connect (menu_item, "activate",
	                  G_CALLBACK (on_move_to_new_window_menuitem_activate),
	                  menu);
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), menu_item);
	gtk_widget_show (menu_item);
	menu->priv->move_to_new_window_item = menu_item;

	menu_item = gtk_separator_menu_item_new ();
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), menu_item);
	gtk_widget_show (menu_item);

	menu_item = gtk_menu_item_new_with_mnemonic (_("_Move Right"));
	g_signal_connect (menu_item, "activate",
	                  G_CALLBACK (on_move_right_menuitem_activate),
	                  menu);
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), menu_item);
	gtk_widget_show (menu_item);
	menu->priv->move_right_item = menu_item;

	menu_item = gtk_menu_item_new_with_mnemonic (_("_Move Left"));
	g_signal_connect (menu_item, "activate",
	                  G_CALLBACK (on_move_left_menuitem_activate),
	                  menu);
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), menu_item);
	gtk_widget_show (menu_item);
	menu->priv->move_left_item = menu_item;
}

GtkWidget *
gedit_notebook_popup_menu_new (GeditWindow *window,
                               GeditTab    *tab)
{
	return g_object_new (GEDIT_TYPE_NOTEBOOK_POPUP_MENU,
	                     "window", window,
	                     "tab", tab,
	                     NULL);
}
