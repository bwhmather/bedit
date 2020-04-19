/*
 * bedit-progress-info-bar.c
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * Based on gedit-progress-info-bar.c from Gedit.
 *
 * Copyright (C) 2005 - Paolo Maggi
 * Copyright (C) 2010 - Steve Frécinaux
 * Copyright (C) 2010-2013 - Garrett Regier
 * Copyright (C) 2011 - Benjamin Otte, Cosimo Cecchi
 * Copyright (C) 2011-2013 - Ignacio Casal Quinteiro
 * Copyright (C) 2013 - Matthias Clasen
 * Copyright (C) 2013-2015 - Sébastien Wilmet
 * Copyright (C) 2014 - Robert Roth
 * Copyright (C) 2014-2015 - Paolo Borelli
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

#include "bedit-progress-info-bar.h"
#include <glib/gi18n.h>

enum { PROP_0, PROP_HAS_CANCEL_BUTTON, LAST_PROP };

static GParamSpec *properties[LAST_PROP];

struct _BeditProgressInfoBar {
    GtkInfoBar parent_instance;

    GtkWidget *image;
    GtkWidget *label;
    GtkWidget *progress;
};

G_DEFINE_TYPE(BeditProgressInfoBar, bedit_progress_info_bar, GTK_TYPE_INFO_BAR)

static void bedit_progress_info_bar_set_has_cancel_button(
    BeditProgressInfoBar *bar, gboolean has_button
) {
    if (has_button) {
        gtk_info_bar_add_button(
            GTK_INFO_BAR(bar), _("_Cancel"), GTK_RESPONSE_CANCEL
        );
    }
}

static void bedit_progress_info_bar_set_property(
    GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec
) {
    BeditProgressInfoBar *bar;

    bar = BEDIT_PROGRESS_INFO_BAR(object);

    switch (prop_id) {
    case PROP_HAS_CANCEL_BUTTON:
        bedit_progress_info_bar_set_has_cancel_button(
            bar, g_value_get_boolean(value)
        );
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void bedit_progress_info_bar_class_init(
    BeditProgressInfoBarClass *klass
) {
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    gobject_class->set_property = bedit_progress_info_bar_set_property;

    properties[PROP_HAS_CANCEL_BUTTON] = g_param_spec_boolean(
        "has-cancel-button", "Has Cancel Button",
        "If the message bar has a cancel button", TRUE,
        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS
    );

    g_object_class_install_properties(gobject_class, LAST_PROP, properties);

    /* Bind class to template */
    gtk_widget_class_set_template_from_resource(
        widget_class, "/com/bwhmather/bedit/ui/bedit-progress-info-bar.ui"
    );
    gtk_widget_class_bind_template_child(
        widget_class, BeditProgressInfoBar, image
    );
    gtk_widget_class_bind_template_child(
        widget_class, BeditProgressInfoBar, label
    );
    gtk_widget_class_bind_template_child(
        widget_class, BeditProgressInfoBar, progress
    );
}

static void bedit_progress_info_bar_init(BeditProgressInfoBar *bar) {
    gtk_widget_init_template(GTK_WIDGET(bar));
}

GtkWidget *bedit_progress_info_bar_new(
    const gchar *icon_name, const gchar *markup, gboolean has_cancel
) {
    BeditProgressInfoBar *bar;

    g_return_val_if_fail(icon_name != NULL, NULL);
    g_return_val_if_fail(markup != NULL, NULL);

    bar = BEDIT_PROGRESS_INFO_BAR(g_object_new(
        BEDIT_TYPE_PROGRESS_INFO_BAR,
        "has-cancel-button", has_cancel,
        NULL
    ));

    bedit_progress_info_bar_set_icon_name(bar, icon_name);
    bedit_progress_info_bar_set_markup(bar, markup);

    return GTK_WIDGET(bar);
}

void bedit_progress_info_bar_set_icon_name(
    BeditProgressInfoBar *bar, const gchar *icon_name
) {
    g_return_if_fail(BEDIT_IS_PROGRESS_INFO_BAR(bar));
    g_return_if_fail(icon_name != NULL);

    gtk_image_set_from_icon_name(
        GTK_IMAGE(bar->image), icon_name, GTK_ICON_SIZE_SMALL_TOOLBAR
    );
}

void bedit_progress_info_bar_set_markup(
    BeditProgressInfoBar *bar, const gchar *markup
) {
    g_return_if_fail(BEDIT_IS_PROGRESS_INFO_BAR(bar));
    g_return_if_fail(markup != NULL);

    gtk_label_set_markup(GTK_LABEL(bar->label), markup);
}

void bedit_progress_info_bar_set_text(
    BeditProgressInfoBar *bar, const gchar *text
) {
    g_return_if_fail(BEDIT_IS_PROGRESS_INFO_BAR(bar));
    g_return_if_fail(text != NULL);

    gtk_label_set_text(GTK_LABEL(bar->label), text);
}

void bedit_progress_info_bar_set_fraction(
    BeditProgressInfoBar *bar, gdouble fraction
) {
    g_return_if_fail(BEDIT_IS_PROGRESS_INFO_BAR(bar));

    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(bar->progress), fraction);
}

void bedit_progress_info_bar_pulse(BeditProgressInfoBar *bar) {
    g_return_if_fail(BEDIT_IS_PROGRESS_INFO_BAR(bar));

    gtk_progress_bar_pulse(GTK_PROGRESS_BAR(bar->progress));
}

