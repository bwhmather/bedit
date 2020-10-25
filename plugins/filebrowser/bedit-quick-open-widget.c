#include "config.h"

#include "bedit-quick-open-widget.h"

#include <gtk/gtk.h>


struct _BeditQuickOpenWidget {
    GtkBin parent_instance;

    GtkSearchEntry *search_entry;
    GtkButton *cancel_button;

    GFile *virtual_root;
};

enum {
    PROP_0,
    PROP_VIRTUAL_ROOT,
    LAST_PROP,
};

static GParamSpec *properties[LAST_PROP];

G_DEFINE_DYNAMIC_TYPE(
    BeditQuickOpenWidget, bedit_quick_open_widget, GTK_TYPE_BIN
)

static void bedit_quick_open_widget_get_property(
    GObject *object, guint prop_id, GValue *value, GParamSpec *pspec
) {
    BeditQuickOpenWidget *obj = BEDIT_QUICK_OPEN_WIDGET(object);

    switch (prop_id) {
    case PROP_VIRTUAL_ROOT:
        g_value_take_object(
            value, bedit_quick_open_widget_get_virtual_root(obj)
        );
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void bedit_quick_open_widget_set_property(
    GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec
) {
    BeditQuickOpenWidget *obj = BEDIT_QUICK_OPEN_WIDGET(object);

    switch (prop_id) {
    case PROP_VIRTUAL_ROOT:
        bedit_quick_open_widget_set_virtual_root(
            obj, G_FILE(g_value_dup_object(value))
        );
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void bedit_quick_open_widget_class_init(BeditQuickOpenWidgetClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    object_class->get_property = bedit_quick_open_widget_get_property;
    object_class->set_property = bedit_quick_open_widget_set_property;

    g_object_class_install_property(
        object_class, PROP_VIRTUAL_ROOT,
        g_param_spec_object(
            "virtual-root", "Virtual Root",
            "The location in the filesystem that widget is currently showing",
            G_TYPE_FILE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS
        )
    );
    g_object_class_install_properties(object_class, LAST_PROP, properties);

    gtk_widget_class_set_template_from_resource(
        widget_class, "/com/bwhmather/bedit/ui/bedit-quick-open-widget.ui"
    );

    gtk_widget_class_bind_template_child(
        widget_class, BeditQuickOpenWidget, search_entry
    );

    gtk_widget_class_bind_template_child(
        widget_class, BeditQuickOpenWidget, cancel_button
    );
}

static void bedit_quick_open_widget_class_finalize(
    BeditQuickOpenWidgetClass *klass
) {}


static void bedit_quick_open_widget_init(BeditQuickOpenWidget *widget) {
    gtk_widget_init_template(GTK_WIDGET(widget));
}


/**
 * bedit_quick_open_widget_new:
 *
 * Creates a new #BeditQuickOpenWidget.
 *
 * Return value: the new #BeditQuickOpenWidget object
 **/
GtkWidget *bedit_quick_open_widget_new(void) {
    return GTK_WIDGET(g_object_new(BEDIT_TYPE_QUICK_OPEN_WIDGET, NULL));
}

void bedit_quick_open_widget_set_virtual_root(
    BeditQuickOpenWidget *widget, GFile *virtual_root
) {
    gboolean updated = FALSE;

    g_return_if_fail(BEDIT_IS_QUICK_OPEN_WIDGET(widget));
    g_return_if_fail(G_IS_FILE(virtual_root));

    if (widget->virtual_root != NULL) {
        updated = !g_file_equal(virtual_root, widget->virtual_root);
        g_object_unref(widget->virtual_root);
    } else {
        updated = virtual_root != NULL;
    }

    widget->virtual_root = virtual_root;

    if (updated) {
        g_object_notify(G_OBJECT(widget), "virtual-root");
    }
}

GFile *bedit_quick_open_widget_get_virtual_root(BeditQuickOpenWidget *widget) {
    g_return_val_if_fail(BEDIT_IS_QUICK_OPEN_WIDGET(widget), NULL);

    if (widget->virtual_root != NULL) {
        g_object_ref(widget->virtual_root);
    }

    return widget->virtual_root;
}

void _bedit_quick_open_widget_register_type(GTypeModule *type_module) {
    bedit_quick_open_widget_register_type(type_module);
}
