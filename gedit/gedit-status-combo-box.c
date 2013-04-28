/*
 * gedit-status-combo-box.c
 * This file is part of gedit
 *
 * Copyright (C) 2008 - Jesse van den Kieboom
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

#include "gedit-status-combo-box.h"

#define COMBO_BOX_TEXT_DATA "GeditStatusComboBoxTextData"

#define GEDIT_STATUS_COMBO_BOX_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE((object), GEDIT_TYPE_STATUS_COMBO_BOX, GeditStatusComboBoxPrivate))

struct _GeditStatusComboBoxPrivate
{
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *item;
	GtkWidget *arrow;
};

struct _GeditStatusComboBoxClassPrivate
{
	GtkCssProvider *css;
};

/* Properties */
enum
{
	PROP_0,
	PROP_LABEL
};

G_DEFINE_TYPE_WITH_CODE (GeditStatusComboBox, gedit_status_combo_box, GTK_TYPE_MENU_BUTTON,
                         g_type_add_class_private (g_define_type_id, sizeof (GeditStatusComboBoxClassPrivate)))

static void
gedit_status_combo_box_finalize (GObject *object)
{
	G_OBJECT_CLASS (gedit_status_combo_box_parent_class)->finalize (object);
}

static void
gedit_status_combo_box_set_label (GeditStatusComboBox *combo,
				  const gchar         *label)
{
	gtk_label_set_markup (GTK_LABEL (combo->priv->label), label);
}

static const gchar *
gedit_status_combo_box_get_label (GeditStatusComboBox *combo)
{
	return gtk_label_get_label (GTK_LABEL (combo->priv->label));
}

static void
gedit_status_combo_box_get_property (GObject    *object,
			             guint       prop_id,
			             GValue     *value,
			             GParamSpec *pspec)
{
	GeditStatusComboBox *obj = GEDIT_STATUS_COMBO_BOX (object);

	switch (prop_id)
	{
		case PROP_LABEL:
			g_value_set_string (value, gedit_status_combo_box_get_label (obj));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gedit_status_combo_box_set_property (GObject      *object,
			             guint         prop_id,
			             const GValue *value,
			             GParamSpec   *pspec)
{
	GeditStatusComboBox *obj = GEDIT_STATUS_COMBO_BOX (object);

	switch (prop_id)
	{
		case PROP_LABEL:
			gedit_status_combo_box_set_label (obj, g_value_get_string (value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gedit_status_combo_box_class_init (GeditStatusComboBoxClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	static const gchar style[] =
		"* {\n"
		  "-GtkButton-default-border : 0;\n"
		  "-GtkButton-default-outside-border : 0;\n"
		  "-GtkButton-inner-border: 0;\n"
		  "-GtkWidget-focus-line-width : 0;\n"
		  "-GtkWidget-focus-padding : 0;\n"
		  "padding: 1px 8px 2px 4px;\n"
		"}";

	object_class->finalize = gedit_status_combo_box_finalize;
	object_class->get_property = gedit_status_combo_box_get_property;
	object_class->set_property = gedit_status_combo_box_set_property;

	g_object_class_override_property (object_class, PROP_LABEL, "label");

	g_type_class_add_private (object_class, sizeof (GeditStatusComboBoxPrivate));

	klass->priv = G_TYPE_CLASS_GET_PRIVATE (klass, GEDIT_TYPE_STATUS_COMBO_BOX, GeditStatusComboBoxClassPrivate);

	klass->priv->css = gtk_css_provider_new ();
	gtk_css_provider_load_from_data (klass->priv->css, style, -1, NULL);
}

static void
gedit_status_combo_box_init (GeditStatusComboBox *self)
{
	GtkStyleContext *context;

	self->priv = GEDIT_STATUS_COMBO_BOX_GET_PRIVATE (self);

	gtk_button_set_relief (GTK_BUTTON (self), GTK_RELIEF_NONE);
	gtk_menu_button_set_direction (GTK_MENU_BUTTON (self), GTK_ARROW_UP);

	self->priv->hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 3);
	gtk_widget_show (self->priv->hbox);
	gtk_container_add (GTK_CONTAINER (self), self->priv->hbox);

	self->priv->label = gtk_label_new ("");
	gtk_widget_show (self->priv->label);

	gtk_label_set_single_line_mode (GTK_LABEL (self->priv->label), TRUE);
	gtk_widget_set_halign (self->priv->label, GTK_ALIGN_START);

	gtk_box_pack_start (GTK_BOX (self->priv->hbox), self->priv->label, FALSE, TRUE, 0);

	self->priv->item = gtk_label_new ("");
	gtk_widget_show (self->priv->item);

	gtk_label_set_single_line_mode (GTK_LABEL (self->priv->item), TRUE);
	gtk_widget_set_halign (self->priv->item, GTK_ALIGN_START);

	gtk_box_pack_start (GTK_BOX (self->priv->hbox), self->priv->item, TRUE, TRUE, 0);

	self->priv->arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_NONE);
	gtk_widget_show (self->priv->arrow);

	gtk_box_pack_start (GTK_BOX (self->priv->hbox), self->priv->arrow, FALSE, TRUE, 0);

	/* make it as small as possible */
	context = gtk_widget_get_style_context (GTK_WIDGET (self));
	gtk_style_context_add_provider (context,
	                                GTK_STYLE_PROVIDER (GEDIT_STATUS_COMBO_BOX_GET_CLASS (self)->priv->css),
	                                GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

/**
 * gedit_status_combo_box_new:
 * @label: (allow-none):
 */
GtkWidget *
gedit_status_combo_box_new (const gchar *label)
{
	return g_object_new (GEDIT_TYPE_STATUS_COMBO_BOX, "label", label, NULL);
}

/* ex:set ts=8 noet: */
