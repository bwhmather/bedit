/*
 * gedit-menu-stack-switcher.c
 * This file is part of gedit
 *
 * Copyright (C) 2014 - Steve Frécinaux
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gedit-menu-stack-switcher.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

struct _GeditMenuStackSwitcherPrivate
{
  GtkWidget *label;

  GtkStack *stack;
  GSimpleActionGroup *action_group;

  GMenu *menu;
};

enum {
  PROP_0,
  PROP_STACK
};

G_DEFINE_TYPE_WITH_PRIVATE (GeditMenuStackSwitcher, gedit_menu_stack_switcher, GTK_TYPE_MENU_BUTTON)

static void
gedit_menu_stack_switcher_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  GeditMenuStackSwitcher *switcher = GEDIT_MENU_STACK_SWITCHER (object);
  GeditMenuStackSwitcherPrivate *priv = switcher->priv;

  switch (prop_id)
    {
    case PROP_STACK:
      g_value_set_object (value, priv->stack);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gedit_menu_stack_switcher_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  GeditMenuStackSwitcher *switcher = GEDIT_MENU_STACK_SWITCHER (object);

  switch (prop_id)
    {
    case PROP_STACK:
      gedit_menu_stack_switcher_set_stack (switcher, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gedit_menu_stack_switcher_dispose (GObject *object)
{
  GeditMenuStackSwitcher *switcher = GEDIT_MENU_STACK_SWITCHER (object);

  gedit_menu_stack_switcher_set_stack (switcher, NULL);

  G_OBJECT_CLASS (gedit_menu_stack_switcher_parent_class)->dispose (object);
}

static void
add_menu_entry (GtkWidget              *widget,
                GeditMenuStackSwitcher *switcher)
{
  GeditMenuStackSwitcherPrivate *priv = switcher->priv;
  gchar *title, *name;
  GMenuItem *item;

  gtk_container_child_get (GTK_CONTAINER (priv->stack), widget,
                           "title", &title,
                           "name", &name,
                           NULL);

  item = g_menu_item_new (title, NULL);
  g_menu_item_set_action_and_target (item, "switcher.set-visible-child", "s", name);

  g_free (title);
  g_free (name);

  g_menu_append_item (priv->menu, item);
  g_object_unref (item);
}

static gboolean
destroy_menu (GtkWidget *switcher)
{
  gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (switcher), NULL);

  /* GtkMenuButton makes the widget insensitive when we remove the model... */
  gtk_widget_set_sensitive (switcher, TRUE);

  return FALSE;
}

static void
gedit_menu_stack_switcher_toggled (GtkToggleButton *button)
{
  GeditMenuStackSwitcher *switcher = GEDIT_MENU_STACK_SWITCHER (button);
  GeditMenuStackSwitcherPrivate *priv = switcher->priv;

  if (!priv->stack)
    return;

  if (gtk_toggle_button_get_active (button))
    {
      priv->menu = g_menu_new ();
      gtk_container_foreach (GTK_CONTAINER (priv->stack), (GtkCallback) add_menu_entry, switcher);

      gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (switcher), G_MENU_MODEL (priv->menu));
    }

  GTK_TOGGLE_BUTTON_CLASS (gedit_menu_stack_switcher_parent_class)->toggled (button);

  if (!gtk_toggle_button_get_active (button))
    {
      g_idle_add ((GSourceFunc) destroy_menu, switcher);
      priv->menu = NULL;
    }
}

static void
gedit_menu_stack_switcher_class_init (GeditMenuStackSwitcherClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkToggleButtonClass *toggle_button_class = GTK_TOGGLE_BUTTON_CLASS (klass);

  object_class->get_property = gedit_menu_stack_switcher_get_property;
  object_class->set_property = gedit_menu_stack_switcher_set_property;
  object_class->dispose = gedit_menu_stack_switcher_dispose;

  toggle_button_class->toggled = gedit_menu_stack_switcher_toggled;

  g_object_class_install_property (object_class,
                                   PROP_STACK,
                                   g_param_spec_object ("stack",
                                                        "Stack",
                                                        "Stack",
                                                        GTK_TYPE_STACK,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT));
}

static void
gedit_menu_stack_switcher_init (GeditMenuStackSwitcher *switcher)
{
  GeditMenuStackSwitcherPrivate *priv;
  GtkWidget *box;
  GtkWidget *arrow;

  priv = gedit_menu_stack_switcher_get_instance_private (switcher);
  switcher->priv = priv;

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);

  arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_NONE);
  g_object_bind_property (switcher, "direction", arrow, "arrow-type", 0);
  gtk_box_pack_end (GTK_BOX (box), arrow, FALSE, TRUE, 6);

  priv->label = gtk_label_new (NULL);
  gtk_box_pack_start (GTK_BOX (box), priv->label, TRUE, TRUE, 6);

  gtk_widget_show_all (box);
  gtk_container_add (GTK_CONTAINER (switcher), box);
}

static void
update_label (GeditMenuStackSwitcher *switcher)
{
  GeditMenuStackSwitcherPrivate *priv = switcher->priv;
  GtkWidget *child;
  gchar *title = NULL;

  if (switcher->priv->stack)
    {
      child = gtk_stack_get_visible_child (GTK_STACK (priv->stack));
      if (child)
        gtk_container_child_get (GTK_CONTAINER (priv->stack), child,
                                 "title", &title, NULL);
    }

  gtk_label_set_label (GTK_LABEL (priv->label), title);
  g_free (title);
}

static void
on_visible_child_changed (GtkWidget              *widget,
                          GParamSpec             *pspec,
                          GeditMenuStackSwitcher *switcher)
{
  update_label (switcher);
}

void
gedit_menu_stack_switcher_set_stack (GeditMenuStackSwitcher *switcher,
                                     GtkStack               *stack)
{
  GeditMenuStackSwitcherPrivate *priv;
  GPropertyAction *action;

  g_return_if_fail (GEDIT_IS_MENU_STACK_SWITCHER (switcher));
  g_return_if_fail (stack == NULL || GTK_IS_STACK (stack));

  priv = switcher->priv;

  if (priv->stack == stack)
    return;

  if (priv->stack)
    {
      gtk_widget_insert_action_group (GTK_WIDGET (switcher), "switcher", NULL);
      g_clear_object (&priv->action_group);
      g_clear_object (&priv->stack);
    }

  if (stack)
    {
      priv->stack = g_object_ref (stack);
      g_signal_connect (priv->stack, "notify::visible-child",
                        G_CALLBACK (on_visible_child_changed), switcher);

      priv->action_group = g_simple_action_group_new ();
      gtk_widget_insert_action_group (GTK_WIDGET (switcher), "switcher", G_ACTION_GROUP (priv->action_group));

      action = g_property_action_new ("set-visible-child", priv->stack, "visible-child-name");
      g_action_map_add_action (G_ACTION_MAP (priv->action_group), G_ACTION (action));
      g_object_unref (action);
    }

  update_label (switcher);
  gtk_widget_set_sensitive (GTK_WIDGET (switcher), stack != NULL);

  g_object_notify (G_OBJECT (switcher), "stack");
}

GtkStack *
gedit_menu_stack_switcher_get_stack (GeditMenuStackSwitcher *switcher)
{
  g_return_val_if_fail (GEDIT_IS_MENU_STACK_SWITCHER (switcher), NULL);

  return switcher->priv->stack;
}

GtkWidget *
gedit_menu_stack_switcher_new (void)
{
  return g_object_new (GEDIT_TYPE_MENU_STACK_SWITCHER, NULL);
}

/* ex:set ts=2 sw=2 et: */
