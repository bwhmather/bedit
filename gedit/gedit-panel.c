/*
 * gedit-panel.c
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the gedit Team, 2005. See the AUTHORS file for a
 * list of people on the gedit Team.
 * See the ChangeLog files for a list of changes.
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gedit-panel.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>

#include "gedit-small-button.h"
#include "gedit-window.h"
#include "gedit-debug.h"

struct _GeditPanelPrivate
{
	GtkOrientation orientation;

	GtkWidget *main_box;

	/* Notebook */
	GtkWidget *stack;
};

/* Properties */
enum {
	PROP_0,
	PROP_ORIENTATION,
};

/* Signals */
enum {
	ITEM_ADDED,
	ITEM_REMOVED,
	CLOSE,
	FOCUS_DOCUMENT,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void	gedit_panel_constructed	(GObject *object);

G_DEFINE_TYPE_WITH_PRIVATE (GeditPanel, gedit_panel, GTK_TYPE_BIN)

static void
gedit_panel_finalize (GObject *object)
{
	G_OBJECT_CLASS (gedit_panel_parent_class)->finalize (object);
}

static void
gedit_panel_get_property (GObject    *object,
			  guint       prop_id,
			  GValue     *value,
			  GParamSpec *pspec)
{
	GeditPanel *panel = GEDIT_PANEL (object);

	switch (prop_id)
	{
		case PROP_ORIENTATION:
			g_value_set_enum (value, panel->priv->orientation);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gedit_panel_set_property (GObject      *object,
			  guint         prop_id,
			  const GValue *value,
			  GParamSpec   *pspec)
{
	GeditPanel *panel = GEDIT_PANEL (object);

	switch (prop_id)
	{
		case PROP_ORIENTATION:
			panel->priv->orientation = g_value_get_enum (value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gedit_panel_close (GeditPanel *panel)
{
	gtk_widget_hide (GTK_WIDGET (panel));
}

static void
gedit_panel_focus_document (GeditPanel *panel)
{
	GtkWidget *toplevel;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (panel));

	if (gtk_widget_is_toplevel (toplevel) && GEDIT_IS_WINDOW (toplevel))
	{
		GeditView *view;

		view = gedit_window_get_active_view (GEDIT_WINDOW (toplevel));
		if (view != NULL)
		{
			gtk_widget_grab_focus (GTK_WIDGET (view));
		}
	}
}

static void
gedit_panel_get_size (GtkWidget     *widget,
                      GtkOrientation orientation,
                      gint          *minimum,
                      gint          *natural)
{
	GtkBin *bin = GTK_BIN (widget);
	GtkWidget *child;

	if (minimum)
		*minimum = 0;

	if (natural)
		*natural = 0;

	child = gtk_bin_get_child (bin);
	if (child && gtk_widget_get_visible (child))
	{
		if (orientation == GTK_ORIENTATION_HORIZONTAL)
		{
			gtk_widget_get_preferred_width (child, minimum, natural);
		}
		else
		{
			gtk_widget_get_preferred_height (child, minimum, natural);
		}
	}
}

static void
gedit_panel_get_preferred_width (GtkWidget *widget,
                                 gint      *minimum,
                                 gint      *natural)
{
	gedit_panel_get_size (widget, GTK_ORIENTATION_HORIZONTAL, minimum, natural);
}

static void
gedit_panel_get_preferred_height (GtkWidget *widget,
                                  gint      *minimum,
                                  gint      *natural)
{
	gedit_panel_get_size (widget, GTK_ORIENTATION_VERTICAL, minimum, natural);
}

static void
gedit_panel_size_allocate (GtkWidget     *widget,
                           GtkAllocation *allocation)
{
	GtkBin *bin = GTK_BIN (widget);
	GtkWidget *child;

	GTK_WIDGET_CLASS (gedit_panel_parent_class)->size_allocate (widget, allocation);

	child = gtk_bin_get_child (bin);
	if (child && gtk_widget_get_visible (child))
	{
		gtk_widget_size_allocate (child, allocation);
	}
}

static void
gedit_panel_grab_focus (GtkWidget *w)
{
	GeditPanel *panel = GEDIT_PANEL (w);
	GtkWidget *tab;

	tab = gtk_stack_get_visible_child (GTK_STACK (panel->priv->stack));
	g_return_if_fail (tab != NULL);

	gtk_widget_grab_focus (tab);
}

static void
gedit_panel_class_init (GeditPanelClass *klass)
{
	GtkBindingSet *binding_set;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->constructed = gedit_panel_constructed;
	object_class->finalize = gedit_panel_finalize;
	object_class->get_property = gedit_panel_get_property;
	object_class->set_property = gedit_panel_set_property;

	widget_class->get_preferred_width = gedit_panel_get_preferred_width;
	widget_class->get_preferred_height = gedit_panel_get_preferred_height;
	widget_class->size_allocate = gedit_panel_size_allocate;
	widget_class->grab_focus = gedit_panel_grab_focus;

	klass->close = gedit_panel_close;
	klass->focus_document = gedit_panel_focus_document;

	g_object_class_install_property (object_class,
	                                 PROP_ORIENTATION,
	                                 g_param_spec_enum ("orientation",
	                                                    "Panel Orientation",
	                                                    "The panel's orientation",
	                                                    GTK_TYPE_ORIENTATION,
	                                                    GTK_ORIENTATION_VERTICAL,
	                                                    G_PARAM_READWRITE |
	                                                    G_PARAM_CONSTRUCT_ONLY |
	                                                    G_PARAM_STATIC_STRINGS));

	signals[ITEM_ADDED] =
		g_signal_new ("item_added",
			      G_OBJECT_CLASS_TYPE (klass),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GeditPanelClass, item_added),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1,
			      GTK_TYPE_WIDGET);
	signals[ITEM_REMOVED] =
		g_signal_new ("item_removed",
			      G_OBJECT_CLASS_TYPE (klass),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GeditPanelClass, item_removed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1,
			      GTK_TYPE_WIDGET);

	/* Keybinding signals */
	signals[CLOSE] =
		g_signal_new ("close",
			      G_OBJECT_CLASS_TYPE (klass),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (GeditPanelClass, close),
		  	      NULL, NULL,
		  	      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	signals[FOCUS_DOCUMENT] =
		g_signal_new ("focus_document",
			      G_OBJECT_CLASS_TYPE (klass),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (GeditPanelClass, focus_document),
		  	      NULL, NULL,
		  	      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	binding_set = gtk_binding_set_by_class (klass);

	gtk_binding_entry_add_signal (binding_set,
				      GDK_KEY_Escape,
				      0,
				      "close",
				      0);
	gtk_binding_entry_add_signal (binding_set,
				      GDK_KEY_Return,
				      GDK_CONTROL_MASK,
				      "focus_document",
				      0);
}

static void
gedit_panel_init (GeditPanel *panel)
{
	panel->priv = gedit_panel_get_instance_private (panel);

	panel->priv->main_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_show (panel->priv->main_box);
	gtk_container_add (GTK_CONTAINER (panel), panel->priv->main_box);
}

static void
close_button_clicked_cb (GtkWidget *widget,
			 GtkWidget *panel)
{
	gtk_widget_hide (panel);
}

static GtkWidget *
create_close_button (GeditPanel *panel)
{
	GtkWidget *button;

	button = gedit_close_button_new ();

	gtk_widget_set_tooltip_text (button, _("Hide panel"));

	g_signal_connect (button,
			  "clicked",
			  G_CALLBACK (close_button_clicked_cb),
			  panel);

	return button;
}

static void
build_horizontal_panel (GeditPanel *panel)
{
	GtkWidget *box;
	GtkWidget *stack_box;
	GtkWidget *switcher;
	GtkWidget *sidebar;
	GtkWidget *close_button;

	box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

	stack_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_box_pack_start (GTK_BOX (box),
			    stack_box,
			    TRUE,
			    TRUE,
			    0);

	gtk_box_pack_start (GTK_BOX (stack_box),
			    panel->priv->stack,
			    TRUE,
			    TRUE,
			    0);

	switcher = gtk_stack_switcher_new ();
	gtk_stack_switcher_set_stack (GTK_STACK_SWITCHER (switcher),
				      GTK_STACK (panel->priv->stack));

	gtk_box_pack_end (GTK_BOX (stack_box),
			  switcher,
			  FALSE,
			  FALSE,
			  0);

	/* Toolbar, close button and first separator */
	sidebar = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_container_set_border_width (GTK_CONTAINER (sidebar), 4);

	gtk_box_pack_start (GTK_BOX (box),
			    sidebar,
			    FALSE,
			    FALSE,
			    0);

	close_button = create_close_button (panel);

	gtk_box_pack_start (GTK_BOX (sidebar),
			    close_button,
			    FALSE,
			    FALSE,
			    0);

	gtk_widget_show_all (box);

	gtk_box_pack_start (GTK_BOX (panel->priv->main_box),
			    box,
			    TRUE,
			    TRUE,
			    0);
}

static void
build_vertical_panel (GeditPanel *panel)
{
	gtk_box_pack_start (GTK_BOX (panel->priv->main_box),
			    panel->priv->stack,
			    TRUE,
			    TRUE,
			    0);
}

static void
gedit_panel_constructed (GObject *object)
{
	GeditPanel *panel = GEDIT_PANEL (object);
	GtkStyleContext *context;

	/* Build the panel, now that we know the orientation
			   (_init has been called previously) */

	context = gtk_widget_get_style_context (GTK_WIDGET (panel));

	/* Create the panel stack */
	panel->priv->stack = gtk_stack_new ();
	gtk_widget_show (panel->priv->stack);

	if (panel->priv->orientation == GTK_ORIENTATION_HORIZONTAL)
	{
		build_horizontal_panel (panel);
		gtk_style_context_add_class (context, GTK_STYLE_CLASS_HORIZONTAL);
	}
	else
	{
		build_vertical_panel (panel);
		gtk_style_context_add_class (context, GTK_STYLE_CLASS_VERTICAL);
	}

	G_OBJECT_CLASS (gedit_panel_parent_class)->constructed (object);
}

/**
 * gedit_panel_new:
 * @orientation: a #GtkOrientation
 *
 * Creates a new #GeditPanel with the given @orientation. You shouldn't create
 * a new panel use gedit_window_get_side_panel() or gedit_window_get_bottom_panel()
 * instead.
 *
 * Returns: a new #GeditPanel object.
 */
GtkWidget *
gedit_panel_new (GtkOrientation orientation)
{
	return GTK_WIDGET (g_object_new (GEDIT_TYPE_PANEL,
					 "orientation", orientation,
					 NULL));
}

static gboolean
item_exists (GeditPanel  *panel,
	     const gchar *id)
{
	gchar *child_id;
	GList *items, *l;
	gboolean exists = FALSE;

	items = gtk_container_get_children (GTK_CONTAINER (panel->priv->stack));

	for (l = items; !exists && l != NULL; l = g_list_next (l))
	{
		gtk_container_child_get (GTK_CONTAINER (panel->priv->stack),
					 GTK_WIDGET (l->data),
					 "name", &child_id,
					 NULL);

		if (strcmp (child_id, id) == 0)
			exists = TRUE;

		g_free (child_id);
	}

	g_list_free (items);

	return exists;
}

/**
 * gedit_panel_add_item:
 * @panel: a #GeditPanel
 * @item: the #GtkWidget to add to the @panel
 * @id: unique name for the new item
 * @display_name: the name to be shown in the @panel
 * @icon_name: (allow-none): the name of the icon to be shown in the @panel
 *
 * Adds a new item to the @panel.
 *
 * Returns: %TRUE is the item was successfully added.
 */
gboolean
gedit_panel_add_item (GeditPanel  *panel,
		      GtkWidget   *item,
		      const gchar *id,
		      const gchar *display_name,
		      const gchar *icon_name)
{
	g_return_val_if_fail (GEDIT_IS_PANEL (panel), FALSE);
	g_return_val_if_fail (GTK_IS_WIDGET (item), FALSE);
	g_return_val_if_fail (id != NULL, FALSE);
	g_return_val_if_fail (display_name != NULL, FALSE);

	if (item_exists (panel, id))
	{
		g_critical ("You are trying to add an item with an id that already exists");
		return FALSE;
	}

	if (!gtk_widget_get_visible (item))
	{
		gtk_widget_show (item);
	}

	gtk_container_add_with_properties (GTK_CONTAINER (panel->priv->stack),
					   item,
					   "name", id,
					   "icon-name", panel->priv->orientation == GTK_ORIENTATION_VERTICAL ? icon_name : NULL,
					   "title", display_name,
					   NULL);

	g_signal_emit (G_OBJECT (panel), signals[ITEM_ADDED], 0, item);

	return TRUE;
}

/**
 * gedit_panel_remove_item:
 * @panel: a #GeditPanel
 * @item: the item to be removed from the panel
 *
 * Removes the widget @item from the panel if it is in the @panel and returns
 * %TRUE if there was not any problem.
 *
 * Returns: %TRUE if it was well removed.
 */
gboolean
gedit_panel_remove_item (GeditPanel *panel,
			 GtkWidget  *item)
{
	g_return_val_if_fail (GEDIT_IS_PANEL (panel), FALSE);
	g_return_val_if_fail (GTK_IS_WIDGET (item), FALSE);

	/* ref the item to keep it alive during signal emission */
	g_object_ref (G_OBJECT (item));

	gtk_container_remove (GTK_CONTAINER (panel->priv->stack), item);

	g_signal_emit (G_OBJECT (panel), signals[ITEM_REMOVED], 0, item);

	g_object_unref (G_OBJECT (item));

	return TRUE;
}

/**
 * gedit_panel_activate_item:
 * @panel: a #GeditPanel
 * @item: the item to be activated
 *
 * Switches to the page that contains @item.
 *
 * Returns: %TRUE if it was activated
 */
gboolean
gedit_panel_activate_item (GeditPanel *panel,
			   GtkWidget  *item)
{
	g_return_val_if_fail (GEDIT_IS_PANEL (panel), FALSE);
	g_return_val_if_fail (GTK_IS_WIDGET (item), FALSE);

	gtk_stack_set_visible_child (GTK_STACK (panel->priv->stack), item);

	return TRUE;
}

/**
 * gedit_panel_get_active:
 * @panel: a #GeditPanel
 *
 * Gets the active item in @panel
 *
 * Returns: (transfer none): the active item in @panel
 */
GtkWidget *
gedit_panel_get_active (GeditPanel *panel)
{
	g_return_val_if_fail (GEDIT_IS_PANEL (panel), NULL);

	return gtk_stack_get_visible_child (GTK_STACK (panel->priv->stack));
}

/**
 * gedit_panel_item_is_active:
 * @panel: a #GeditPanel
 * @item: a #GtkWidget
 *
 * Returns whether @item is the active widget in @panel
 *
 * Returns: %TRUE if @item is the active widget
 */
gboolean
gedit_panel_item_is_active (GeditPanel *panel,
			    GtkWidget  *item)
{
	g_return_val_if_fail (GEDIT_IS_PANEL (panel), FALSE);
	g_return_val_if_fail (GTK_IS_WIDGET (item), FALSE);

	return item == gtk_stack_get_visible_child (GTK_STACK (panel->priv->stack));
}

/**
 * gedit_panel_get_orientation:
 * @panel: a #GeditPanel
 *
 * Gets the orientation of the @panel.
 *
 * Returns: the #GtkOrientation of #GeditPanel
 */
GtkOrientation
gedit_panel_get_orientation (GeditPanel *panel)
{
	g_return_val_if_fail (GEDIT_IS_PANEL (panel), GTK_ORIENTATION_VERTICAL);

	return panel->priv->orientation;
}

/**
 * gedit_panel_get_n_items:
 * @panel: a #GeditPanel
 *
 * Gets the number of items in a @panel.
 *
 * Returns: the number of items contained in #GeditPanel
 */
gint
gedit_panel_get_n_items (GeditPanel *panel)
{
	GList *children;
	int n;

	g_return_val_if_fail (GEDIT_IS_PANEL (panel), -1);

	children = gtk_container_get_children (GTK_CONTAINER (panel->priv->stack));
	n = g_list_length (children);
	g_list_free (children);

	return n;
}

gint
_gedit_panel_get_active_item_id (GeditPanel *panel)
{
	const gchar *child_name;

	g_return_val_if_fail (GEDIT_IS_PANEL (panel), 0);

	child_name = gtk_stack_get_visible_child_name (GTK_STACK (panel->priv->stack));

	if (!child_name)
		return 0;

	return g_str_hash (child_name);
}

void
_gedit_panel_set_active_item_by_id (GeditPanel *panel,
				    gint        id)
{
	GList *items, *l;
	gboolean found = FALSE;

	g_return_if_fail (GEDIT_IS_PANEL (panel));

	if (id == 0)
		return;

	items = gtk_container_get_children (GTK_CONTAINER (panel->priv->stack));

	for (l = items; !found && l != NULL; l = g_list_next (l))
	{
		gchar *child_id;

		gtk_container_child_get (GTK_CONTAINER (panel->priv->stack),
					 GTK_WIDGET (l->data),
					 "name", &child_id,
					 NULL);

		if (child_id && g_str_hash (child_id) == id)
		{
			found = TRUE;
			gtk_stack_set_visible_child_name (GTK_STACK (panel->priv->stack),
							  child_id);
		}

		g_free (child_id);
	}

	g_list_free (items);
}

GtkWidget
*_gedit_panel_get_stack (GeditPanel *panel)
{
	return panel->priv->stack;
}

/* ex:set ts=8 noet: */
