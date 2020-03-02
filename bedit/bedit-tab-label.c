/*
 * bedit-tab-label.c
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * Based on gedit-tab-label.c from Gedit.
 *
 * Copyright (C) 2010 - Jesse van den Kieboom, Steve Frécinaux
 * Copyright (C) 2010-2011 - Garrett Regier
 * Copyright (C) 2010-2014 - Ignacio Casal Quinteiro
 * Copyright (C) 2010-2015 - Paolo Borelli
 * Copyright (C) 2013 - William Jon McCann
 * Copyright (C) 2013-2015 - Sébastien Wilmet
 * Copyright (C) 2014 - Robert Roth
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

#include "bedit-tab-label.h"
#include "bedit-tab-private.h"

struct _BeditTabLabel {
    GtkBox parent_instance;

    BeditTab *tab;

    GtkWidget *spinner;
    GtkWidget *icon;
    GtkWidget *label;
    GtkWidget *close_button;
};

enum { PROP_0, PROP_TAB, LAST_PROP };

static GParamSpec *properties[LAST_PROP];

enum { CLOSE_CLICKED, LAST_SIGNAL };

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE(BeditTabLabel, bedit_tab_label, GTK_TYPE_BOX)

static void bedit_tab_label_set_property(
    GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec
) {
    BeditTabLabel *tab_label = BEDIT_TAB_LABEL(object);

    switch (prop_id) {
    case PROP_TAB:
        g_return_if_fail(tab_label->tab == NULL);
        tab_label->tab = BEDIT_TAB(g_value_get_object(value));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void bedit_tab_label_get_property(
    GObject *object, guint prop_id, GValue *value, GParamSpec *pspec
) {
    BeditTabLabel *tab_label = BEDIT_TAB_LABEL(object);

    switch (prop_id) {
    case PROP_TAB:
        g_value_set_object(value, tab_label->tab);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void close_button_clicked_cb(
    GtkWidget *widget, BeditTabLabel *tab_label
) {
    g_signal_emit(tab_label, signals[CLOSE_CLICKED], 0, NULL);
}

static void sync_tooltip(BeditTab *tab, BeditTabLabel *tab_label) {
    gchar *str;

    str = _bedit_tab_get_tooltip(tab);
    g_return_if_fail(str != NULL);

    gtk_widget_set_tooltip_markup(GTK_WIDGET(tab_label), str);
    g_free(str);
}

static void sync_name(
    BeditTab *tab, GParamSpec *pspec, BeditTabLabel *tab_label
) {
    gchar *str;

    g_return_if_fail(tab == tab_label->tab);

    str = _bedit_tab_get_name(tab);
    g_return_if_fail(str != NULL);

    gtk_label_set_text(GTK_LABEL(tab_label->label), str);
    g_free(str);

    sync_tooltip(tab, tab_label);
}

static void update_close_button_sensitivity(BeditTabLabel *tab_label) {
    BeditTabState state = bedit_tab_get_state(tab_label->tab);

    gtk_widget_set_sensitive(
        tab_label->close_button,
        (state != BEDIT_TAB_STATE_CLOSING) &&
        (state != BEDIT_TAB_STATE_SAVING) &&
        (state != BEDIT_TAB_STATE_SHOWING_PRINT_PREVIEW) &&
        (state != BEDIT_TAB_STATE_PRINTING) &&
        (state != BEDIT_TAB_STATE_SAVING_ERROR)
    );
}

static void sync_state(
    BeditTab *tab, GParamSpec *pspec, BeditTabLabel *tab_label
) {
    BeditTabState state;

    g_return_if_fail(tab == tab_label->tab);

    update_close_button_sensitivity(tab_label);

    state = bedit_tab_get_state(tab);

    if (
        (state == BEDIT_TAB_STATE_LOADING) ||
        (state == BEDIT_TAB_STATE_SAVING) ||
        (state == BEDIT_TAB_STATE_REVERTING)
    ) {
        gtk_widget_hide(tab_label->icon);

        gtk_widget_show(tab_label->spinner);
        gtk_spinner_start(GTK_SPINNER(tab_label->spinner));
    } else {
        GdkPixbuf *pixbuf;

        pixbuf = _bedit_tab_get_icon(tab);

        if (pixbuf != NULL) {
            gtk_image_set_from_pixbuf(GTK_IMAGE(tab_label->icon), pixbuf);

            g_clear_object(&pixbuf);

            gtk_widget_show(tab_label->icon);
        } else {
            gtk_widget_hide(tab_label->icon);
        }

        gtk_spinner_stop(GTK_SPINNER(tab_label->spinner));
        gtk_widget_hide(tab_label->spinner);
    }

    /* sync tip since encoding is known only after load/save end */
    sync_tooltip(tab, tab_label);
}

static void bedit_tab_label_constructed(GObject *object) {
    BeditTabLabel *tab_label = BEDIT_TAB_LABEL(object);

    if (tab_label->tab == NULL) {
        g_critical("The tab label was not properly constructed");
        return;
    }

    sync_name(tab_label->tab, NULL, tab_label);
    sync_state(tab_label->tab, NULL, tab_label);

    g_signal_connect_object(
        tab_label->tab, "notify::name",
        G_CALLBACK(sync_name), tab_label, 0
    );

    g_signal_connect_object(
        tab_label->tab, "notify::state",
        G_CALLBACK(sync_state), tab_label, 0
    );

    G_OBJECT_CLASS(bedit_tab_label_parent_class)->constructed(object);
}

static void bedit_tab_label_close_clicked(BeditTabLabel *tab_label) {}

static void bedit_tab_label_class_init(BeditTabLabelClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    object_class->set_property = bedit_tab_label_set_property;
    object_class->get_property = bedit_tab_label_get_property;
    object_class->constructed = bedit_tab_label_constructed;

    properties[PROP_TAB] = g_param_spec_object(
        "tab", "Tab", "The BeditTab", BEDIT_TYPE_TAB,
        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY
    );

    g_object_class_install_properties(object_class, LAST_PROP, properties);

    signals[CLOSE_CLICKED] = g_signal_new_class_handler(
        "close-clicked", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST,
        G_CALLBACK(bedit_tab_label_close_clicked), NULL, NULL, NULL,
        G_TYPE_NONE, 0
    );

    /* Bind class to template */
    gtk_widget_class_set_template_from_resource(
        widget_class, "/com/bwhmather/bedit/ui/bedit-tab-label.ui"
    );
    gtk_widget_class_bind_template_child(
        widget_class, BeditTabLabel, spinner
    );
    gtk_widget_class_bind_template_child(
        widget_class, BeditTabLabel, icon
    );
    gtk_widget_class_bind_template_child(
        widget_class, BeditTabLabel, label
    );
    gtk_widget_class_bind_template_child(
        widget_class, BeditTabLabel, close_button
    );
}

static void bedit_tab_label_init(BeditTabLabel *tab_label) {
    gtk_widget_init_template(GTK_WIDGET(tab_label));

    g_signal_connect(
        tab_label->close_button, "clicked",
        G_CALLBACK(close_button_clicked_cb), tab_label
    );
}

BeditTab *bedit_tab_label_get_tab(BeditTabLabel *tab_label) {
    g_return_val_if_fail(BEDIT_IS_TAB_LABEL(tab_label), NULL);

    return tab_label->tab;
}

GtkWidget *bedit_tab_label_new(BeditTab *tab) {
    return g_object_new(BEDIT_TYPE_TAB_LABEL, "tab", tab, NULL);
}

