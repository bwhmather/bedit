/*
 * bedit-view.c
 * This file is part of bedit
 *
 * Copyright (C) 1998-1999 - Alex Roberts, Evan Lawrence
 * Copyright (C) 2000-2002 - Chema Celorio, Paolo Maggi
 * Copyright (C) 2003-2005 - Paolo Maggi
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

#include "bedit-view.h"

#include <glib/gi18n.h>
#include <libpeas/peas-extension-set.h>

#include "bedit-app-private.h"
#include "bedit-app.h"
#include "bedit-debug.h"
#include "bedit-pango.h"
#include "bedit-plugins-engine.h"
#include "bedit-settings.h"
#include "bedit-utils.h"
#include "bedit-view-activatable.h"

#define BEDIT_VIEW_SCROLL_MARGIN 0.02

enum { TARGET_URI_LIST = 100, TARGET_XDNDDIRECTSAVE };

struct _BeditViewPrivate {
    GSettings *editor_settings;
    GtkTextBuffer *current_buffer;
    PeasExtensionSet *extensions;
    gchar *direct_save_uri;

    GtkCssProvider *css_provider;
    PangoFontDescription *font_desc;
};

G_DEFINE_TYPE_WITH_PRIVATE(BeditView, bedit_view, GTK_SOURCE_TYPE_VIEW)

enum { DROP_URIS, LAST_SIGNAL };

static guint view_signals[LAST_SIGNAL] = {0};

static void file_read_only_notify_handler(
    GtkSourceFile *file, GParamSpec *pspec, BeditView *view) {
    bedit_debug(DEBUG_VIEW);

    gtk_text_view_set_editable(
        GTK_TEXT_VIEW(view), !gtk_source_file_is_readonly(file));
}

static void current_buffer_removed(BeditView *view) {
    if (view->priv->current_buffer != NULL) {
        GtkSourceFile *file;

        file =
            bedit_document_get_file(BEDIT_DOCUMENT(view->priv->current_buffer));

        g_signal_handlers_disconnect_by_func(
            file, file_read_only_notify_handler, view);

        g_object_unref(view->priv->current_buffer);
        view->priv->current_buffer = NULL;
    }
}

static void on_notify_buffer_cb(
    BeditView *view, GParamSpec *pspec, gpointer userdata) {
    GtkTextBuffer *buffer;
    GtkSourceFile *file;

    current_buffer_removed(view);
    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));

    if (!BEDIT_IS_DOCUMENT(buffer)) {
        return;
    }

    file = bedit_document_get_file(BEDIT_DOCUMENT(buffer));

    view->priv->current_buffer = g_object_ref(buffer);
    g_signal_connect_object(
        file, "notify::read-only", G_CALLBACK(file_read_only_notify_handler),
        view, 0);

    gtk_text_view_set_editable(
        GTK_TEXT_VIEW(view), !gtk_source_file_is_readonly(file));
}

static void bedit_view_init(BeditView *view) {
    GtkTargetList *target_list;
    GtkStyleContext *context;

    bedit_debug(DEBUG_VIEW);

    view->priv = bedit_view_get_instance_private(view);

    view->priv->editor_settings =
        g_settings_new("com.bwhmather.bedit.preferences.editor");

    /* Drag and drop support */
    view->priv->direct_save_uri = NULL;
    target_list = gtk_drag_dest_get_target_list(GTK_WIDGET(view));

    if (target_list != NULL) {
        gtk_target_list_add(
            target_list, gdk_atom_intern("XdndDirectSave0", FALSE), 0,
            TARGET_XDNDDIRECTSAVE);
        gtk_target_list_add_uri_targets(target_list, TARGET_URI_LIST);
    }

    view->priv->extensions = peas_extension_set_new(
        PEAS_ENGINE(bedit_plugins_engine_get_default()),
        BEDIT_TYPE_VIEW_ACTIVATABLE, "view", view, NULL);

    /* Act on buffer change */
    g_signal_connect(
        view, "notify::buffer", G_CALLBACK(on_notify_buffer_cb), NULL);

    view->priv->css_provider = gtk_css_provider_new();
    context = gtk_widget_get_style_context(GTK_WIDGET(view));
    gtk_style_context_add_class(context, "bedit-view");

    gtk_style_context_add_provider(
        context, GTK_STYLE_PROVIDER(view->priv->css_provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

static void bedit_view_dispose(GObject *object) {
    BeditView *view = BEDIT_VIEW(object);

    g_clear_object(&view->priv->extensions);
    g_clear_object(&view->priv->editor_settings);

    current_buffer_removed(view);

    /* Disconnect notify buffer because the destroy of the textview will set
     * the buffer to NULL, and we call get_buffer in the notify which would
     * reinstate a buffer which we don't want.
     * There is no problem calling g_signal_handlers_disconnect_by_func()
     * several times (if dispose() is called several times).
     */
    g_signal_handlers_disconnect_by_func(view, on_notify_buffer_cb, NULL);

    g_clear_object(&view->priv->css_provider);
    g_clear_pointer(&view->priv->font_desc, pango_font_description_free);

    G_OBJECT_CLASS(bedit_view_parent_class)->dispose(object);
}

static void bedit_view_constructed(GObject *object) {
    BeditView *view;
    BeditViewPrivate *priv;
    gboolean use_default_font;

    view = BEDIT_VIEW(object);
    priv = view->priv;

    use_default_font = g_settings_get_boolean(
        view->priv->editor_settings, BEDIT_SETTINGS_USE_DEFAULT_FONT);

    if (use_default_font) {
        bedit_view_set_font(view, TRUE, NULL);
    } else {
        gchar *editor_font;

        editor_font = g_settings_get_string(
            view->priv->editor_settings, BEDIT_SETTINGS_EDITOR_FONT);

        bedit_view_set_font(view, FALSE, editor_font);

        g_free(editor_font);
    }

    g_settings_bind(
        priv->editor_settings, BEDIT_SETTINGS_DISPLAY_LINE_NUMBERS, view,
        "show-line-numbers", G_SETTINGS_BIND_GET);

    g_settings_bind(
        priv->editor_settings, BEDIT_SETTINGS_AUTO_INDENT, view, "auto-indent",
        G_SETTINGS_BIND_GET);

    g_settings_bind(
        priv->editor_settings, BEDIT_SETTINGS_TABS_SIZE, view, "tab-width",
        G_SETTINGS_BIND_GET);

    g_settings_bind(
        priv->editor_settings, BEDIT_SETTINGS_INSERT_SPACES, view,
        "insert-spaces-instead-of-tabs", G_SETTINGS_BIND_GET);

    g_settings_bind(
        priv->editor_settings, BEDIT_SETTINGS_DISPLAY_RIGHT_MARGIN, view,
        "show-right-margin", G_SETTINGS_BIND_GET);

    g_settings_bind(
        priv->editor_settings, BEDIT_SETTINGS_BACKGROUND_PATTERN, view,
        "background-pattern", G_SETTINGS_BIND_GET);

    g_settings_bind(
        priv->editor_settings, BEDIT_SETTINGS_RIGHT_MARGIN_POSITION, view,
        "right-margin-position", G_SETTINGS_BIND_GET);

    g_settings_bind(
        priv->editor_settings, BEDIT_SETTINGS_HIGHLIGHT_CURRENT_LINE, view,
        "highlight-current-line", G_SETTINGS_BIND_GET);

    g_settings_bind(
        priv->editor_settings, BEDIT_SETTINGS_WRAP_MODE, view, "wrap-mode",
        G_SETTINGS_BIND_GET);

    g_settings_bind(
        priv->editor_settings, BEDIT_SETTINGS_SMART_HOME_END, view,
        "smart-home-end", G_SETTINGS_BIND_GET);

    gtk_source_view_set_indent_on_tab(GTK_SOURCE_VIEW(view), TRUE);
    gtk_source_view_set_smart_backspace(GTK_SOURCE_VIEW(view), TRUE);

    G_OBJECT_CLASS(bedit_view_parent_class)->constructed(object);
}

static gboolean bedit_view_focus_out(GtkWidget *widget, GdkEventFocus *event) {
    gtk_widget_queue_draw(widget);

    GTK_WIDGET_CLASS(bedit_view_parent_class)->focus_out_event(widget, event);

    return GDK_EVENT_PROPAGATE;
}

static GdkAtom drag_get_uri_target(GtkWidget *widget, GdkDragContext *context) {
    GdkAtom target;
    GtkTargetList *target_list;

    target_list = gtk_target_list_new(NULL, 0);
    gtk_target_list_add_uri_targets(target_list, 0);

    target = gtk_drag_dest_find_target(widget, context, target_list);
    gtk_target_list_unref(target_list);

    return target;
}

static gboolean bedit_view_drag_motion(
    GtkWidget *widget, GdkDragContext *context, gint x, gint y,
    guint timestamp) {
    gboolean drop_zone;

    /* Chain up to allow textview to scroll and position dnd mark, note
     * that this needs to be checked if gtksourceview or gtktextview
     * changes drag_motion behaviour.
     */
    drop_zone = GTK_WIDGET_CLASS(bedit_view_parent_class)
                    ->drag_motion(widget, context, x, y, timestamp);

    /* If this is a URL, deal with it here */
    if (drag_get_uri_target(widget, context) != GDK_NONE) {
        gdk_drag_status(
            context, gdk_drag_context_get_suggested_action(context), timestamp);
        drop_zone = TRUE;
    }

    return drop_zone;
}

static void bedit_view_drag_data_received(
    GtkWidget *widget, GdkDragContext *context, gint x, gint y,
    GtkSelectionData *selection_data, guint info, guint timestamp) {
    /* If this is an URL emit DROP_URIS, otherwise chain up the signal */
    switch (info) {
    case TARGET_URI_LIST: {
        gchar **uri_list;

        uri_list = bedit_utils_drop_get_uris(selection_data);

        if (uri_list != NULL) {
            g_signal_emit(widget, view_signals[DROP_URIS], 0, uri_list);
            g_strfreev(uri_list);

            gtk_drag_finish(context, TRUE, FALSE, timestamp);
        }

        break;
    }
    case TARGET_XDNDDIRECTSAVE: {
        BeditView *view;

        view = BEDIT_VIEW(widget);

        /* Indicate that we don't provide "F" fallback */
        if (gtk_selection_data_get_format(selection_data) == 8 &&
            gtk_selection_data_get_length(selection_data) == 1 &&
            gtk_selection_data_get_data(selection_data)[0] == 'F') {
            gdk_property_change(
                gdk_drag_context_get_source_window(context),
                gdk_atom_intern("XdndDirectSave0", FALSE),
                gdk_atom_intern("text/plain", FALSE), 8, GDK_PROP_MODE_REPLACE,
                (const guchar *)"", 0);
        } else if (
            gtk_selection_data_get_format(selection_data) == 8 &&
            gtk_selection_data_get_length(selection_data) == 1 &&
            gtk_selection_data_get_data(selection_data)[0] == 'S' &&
            view->priv->direct_save_uri != NULL) {
            gchar **uris;

            uris = g_new(gchar *, 2);
            uris[0] = view->priv->direct_save_uri;
            uris[1] = NULL;
            g_signal_emit(widget, view_signals[DROP_URIS], 0, uris);
            g_free(uris);
        }

        g_free(view->priv->direct_save_uri);
        view->priv->direct_save_uri = NULL;

        gtk_drag_finish(context, TRUE, FALSE, timestamp);

        break;
    }
    default: {
        GTK_WIDGET_CLASS(bedit_view_parent_class)
            ->drag_data_received(
                widget, context, x, y, selection_data, info, timestamp);
        break;
    }
    }
}

static gboolean bedit_view_drag_drop(
    GtkWidget *widget, GdkDragContext *context, gint x, gint y,
    guint timestamp) {
    gboolean drop_zone;
    GdkAtom target;
    guint info;
    gboolean found;
    GtkTargetList *target_list;

    target_list = gtk_drag_dest_get_target_list(widget);
    target = gtk_drag_dest_find_target(widget, context, target_list);
    found = gtk_target_list_find(target_list, target, &info);

    if (found && (info == TARGET_URI_LIST || info == TARGET_XDNDDIRECTSAVE)) {
        if (info == TARGET_XDNDDIRECTSAVE) {
            gchar *uri;
            uri = bedit_utils_set_direct_save_filename(context);

            if (uri != NULL) {
                BeditView *view = BEDIT_VIEW(widget);
                g_free(view->priv->direct_save_uri);
                view->priv->direct_save_uri = uri;
            }
        }

        gtk_drag_get_data(widget, context, target, timestamp);
        drop_zone = TRUE;
    } else {
        /* Chain up */
        drop_zone = GTK_WIDGET_CLASS(bedit_view_parent_class)
                        ->drag_drop(widget, context, x, y, timestamp);
    }

    return drop_zone;
}

static void show_line_numbers_menu(BeditView *view, GdkEventButton *event) {
    GtkWidget *menu;
    GtkWidget *item;

    menu = gtk_menu_new();

    item = gtk_check_menu_item_new_with_mnemonic(_("_Display line numbers"));
    gtk_check_menu_item_set_active(
        GTK_CHECK_MENU_ITEM(item),
        gtk_source_view_get_show_line_numbers(GTK_SOURCE_VIEW(view)));

    g_settings_bind(
        view->priv->editor_settings, BEDIT_SETTINGS_DISPLAY_LINE_NUMBERS, item,
        "active", G_SETTINGS_BIND_SET);

    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    g_signal_connect(
        menu, "selection-done", G_CALLBACK(gtk_widget_destroy), NULL);

    gtk_widget_show_all(menu);
    gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent *)event);
}

static gboolean bedit_view_button_press_event(
    GtkWidget *widget, GdkEventButton *event) {
    if ((event->type == GDK_BUTTON_PRESS) &&
        (event->button == GDK_BUTTON_SECONDARY) &&
        (event->window ==
         gtk_text_view_get_window(
             GTK_TEXT_VIEW(widget), GTK_TEXT_WINDOW_LEFT))) {
        show_line_numbers_menu(BEDIT_VIEW(widget), event);

        return GDK_EVENT_STOP;
    }

    return GTK_WIDGET_CLASS(bedit_view_parent_class)
        ->button_press_event(widget, event);
}

static void extension_added(
    PeasExtensionSet *extensions, PeasPluginInfo *info, PeasExtension *exten,
    BeditView *view) {
    bedit_view_activatable_activate(BEDIT_VIEW_ACTIVATABLE(exten));
}

static void extension_removed(
    PeasExtensionSet *extensions, PeasPluginInfo *info, PeasExtension *exten,
    BeditView *view) {
    bedit_view_activatable_deactivate(BEDIT_VIEW_ACTIVATABLE(exten));
}

static void bedit_view_realize(GtkWidget *widget) {
    BeditView *view = BEDIT_VIEW(widget);

    GTK_WIDGET_CLASS(bedit_view_parent_class)->realize(widget);

    g_signal_connect(
        view->priv->extensions, "extension-added", G_CALLBACK(extension_added),
        view);

    g_signal_connect(
        view->priv->extensions, "extension-removed",
        G_CALLBACK(extension_removed), view);

    /* We only activate the extensions when the view is realized,
     * because most plugins will expect this behaviour, and we won't
     * change the buffer later anyway.
     */
    peas_extension_set_foreach(
        view->priv->extensions, (PeasExtensionSetForeachFunc)extension_added,
        view);
}

static void bedit_view_unrealize(GtkWidget *widget) {
    BeditView *view = BEDIT_VIEW(widget);

    g_signal_handlers_disconnect_by_func(
        view->priv->extensions, extension_added, view);
    g_signal_handlers_disconnect_by_func(
        view->priv->extensions, extension_removed, view);

    /* We need to deactivate the extension on unrealize because it is not
     * mandatory that a view has been realized when we dispose it, leading
     * to deactivating the plugin without being activated.
     */
    peas_extension_set_foreach(
        view->priv->extensions, (PeasExtensionSetForeachFunc)extension_removed,
        view);

    GTK_WIDGET_CLASS(bedit_view_parent_class)->unrealize(widget);
}

static void delete_line(GtkTextView *text_view, gint count) {
    GtkTextIter start;
    GtkTextIter end;
    GtkTextBuffer *buffer;

    buffer = gtk_text_view_get_buffer(text_view);

    gtk_text_view_reset_im_context(text_view);

    /* If there is a selection delete the selected lines and ignore count. */
    if (gtk_text_buffer_get_selection_bounds(buffer, &start, &end)) {
        gtk_text_iter_order(&start, &end);

        if (gtk_text_iter_starts_line(&end)) {
            /* Do not delete the line with the cursor if the cursor
             * is at the beginning of the line.
             */
            count = 0;
        } else {
            count = 1;
        }
    }

    gtk_text_iter_set_line_offset(&start, 0);

    if (count > 0) {
        gtk_text_iter_forward_lines(&end, count);

        if (gtk_text_iter_is_end(&end)) {
            if (gtk_text_iter_backward_line(&start) &&
                !gtk_text_iter_ends_line(&start)) {
                gtk_text_iter_forward_to_line_end(&start);
            }
        }
    } else if (count < 0) {
        if (!gtk_text_iter_ends_line(&end)) {
            gtk_text_iter_forward_to_line_end(&end);
        }

        while (count < 0) {
            if (!gtk_text_iter_backward_line(&start)) {
                break;
            }

            count++;
        }

        if (count == 0) {
            if (!gtk_text_iter_ends_line(&start)) {
                gtk_text_iter_forward_to_line_end(&start);
            }
        } else {
            gtk_text_iter_forward_line(&end);
        }
    }

    if (!gtk_text_iter_equal(&start, &end)) {
        GtkTextIter cur = start;
        gtk_text_iter_set_line_offset(&cur, 0);

        gtk_text_buffer_begin_user_action(buffer);

        gtk_text_buffer_place_cursor(buffer, &cur);

        gtk_text_buffer_delete_interactive(
            buffer, &start, &end, gtk_text_view_get_editable(text_view));

        gtk_text_buffer_end_user_action(buffer);

        gtk_text_view_scroll_mark_onscreen(
            text_view, gtk_text_buffer_get_insert(buffer));
    } else {
        gtk_widget_error_bell(GTK_WIDGET(text_view));
    }
}

static void bedit_view_delete_from_cursor(
    GtkTextView *text_view, GtkDeleteType type, gint count) {
    /* We override the standard handler for delete_from_cursor since
     * the GTK_DELETE_PARAGRAPHS case is not implemented as we like (i.e. it
     * does not remove the carriage return in the previous line).
     */
    switch (type) {
    case GTK_DELETE_PARAGRAPHS:
        delete_line(text_view, count);
        break;

    default:
        GTK_TEXT_VIEW_CLASS(bedit_view_parent_class)
            ->delete_from_cursor(text_view, type, count);
        break;
    }
}

static GtkTextBuffer *bedit_view_create_buffer(GtkTextView *text_view) {
    return GTK_TEXT_BUFFER(bedit_document_new());
}

static void bedit_view_class_init(BeditViewClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
    GtkTextViewClass *text_view_class = GTK_TEXT_VIEW_CLASS(klass);
    GtkBindingSet *binding_set;

    object_class->dispose = bedit_view_dispose;
    object_class->constructed = bedit_view_constructed;

    /* Override the gtk_text_view_drag_motion and drag_drop
     * functions to get URIs
     *
     * If the mime type is text/uri-list, then we will accept
     * the potential drop, or request the data (depending on the
     * function).
     *
     * If the drag context has any other mime type, then pass the
     * information onto the GtkTextView's standard handlers.
     *
     * See bug #89881 for details
     */
    widget_class->drag_motion = bedit_view_drag_motion;
    widget_class->drag_data_received = bedit_view_drag_data_received;
    widget_class->drag_drop = bedit_view_drag_drop;

    widget_class->focus_out_event = bedit_view_focus_out;
    widget_class->button_press_event = bedit_view_button_press_event;
    widget_class->realize = bedit_view_realize;
    widget_class->unrealize = bedit_view_unrealize;

    text_view_class->delete_from_cursor = bedit_view_delete_from_cursor;
    text_view_class->create_buffer = bedit_view_create_buffer;

    /**
     * BeditView::drop-uris:
     * @view: a #BeditView.
     * @uri_list: a %NULL-terminated list of URIs.
     *
     * The #BeditView::drop-uris signal allows plugins to intercept the
     * default drag-and-drop behaviour of 'text/uri-list'. #BeditView
     * handles drag-and-drop in the default handlers of
     * #GtkWidget::drag-drop, #GtkWidget::drag-motion and
     * #GtkWidget::drag-data-received. The view emits the
     * #BeditView::drop-uris signal from #GtkWidget::drag-data-received if
     * valid URIs have been dropped.  Plugins should connect to
     * #GtkWidget::drag-motion, #GtkWidget::drag-drop and
     * #GtkWidget::drag-data-received to change this default behaviour. They
     * should NOT use this signal because this will not prevent bedit from
     * loading the URI.
     */
    view_signals[DROP_URIS] = g_signal_new(
        "drop-uris", G_TYPE_FROM_CLASS(object_class),
        G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
        G_STRUCT_OFFSET(BeditViewClass, drop_uris), NULL, NULL, NULL,
        G_TYPE_NONE, 1, G_TYPE_STRV);

    binding_set = gtk_binding_set_by_class(klass);

    gtk_binding_entry_add_signal(
        binding_set, GDK_KEY_d, GDK_CONTROL_MASK, "delete_from_cursor", 2,
        G_TYPE_ENUM, GTK_DELETE_PARAGRAPHS, G_TYPE_INT, 1);

    gtk_binding_entry_add_signal(
        binding_set, GDK_KEY_u, GDK_CONTROL_MASK, "change_case", 1, G_TYPE_ENUM,
        GTK_SOURCE_CHANGE_CASE_UPPER);

    gtk_binding_entry_add_signal(
        binding_set, GDK_KEY_l, GDK_CONTROL_MASK, "change_case", 1, G_TYPE_ENUM,
        GTK_SOURCE_CHANGE_CASE_LOWER);

    gtk_binding_entry_add_signal(
        binding_set, GDK_KEY_asciitilde, GDK_CONTROL_MASK, "change_case", 1,
        G_TYPE_ENUM, GTK_SOURCE_CHANGE_CASE_TOGGLE);
}

/**
 * bedit_view_new:
 * @doc: a #BeditDocument
 *
 * Creates a new #BeditView object displaying the @doc document.
 * @doc cannot be %NULL.
 *
 * Returns: a new #BeditView.
 */
GtkWidget *bedit_view_new(BeditDocument *doc) {
    g_return_val_if_fail(BEDIT_IS_DOCUMENT(doc), NULL);

    return GTK_WIDGET(g_object_new(BEDIT_TYPE_VIEW, "buffer", doc, NULL));
}

void bedit_view_cut_clipboard(BeditView *view) {
    GtkTextBuffer *buffer;
    GtkClipboard *clipboard;

    bedit_debug(DEBUG_VIEW);

    g_return_if_fail(BEDIT_IS_VIEW(view));

    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));

    clipboard =
        gtk_widget_get_clipboard(GTK_WIDGET(view), GDK_SELECTION_CLIPBOARD);

    gtk_text_buffer_cut_clipboard(
        buffer, clipboard, gtk_text_view_get_editable(GTK_TEXT_VIEW(view)));

    gtk_text_view_scroll_to_mark(
        GTK_TEXT_VIEW(view), gtk_text_buffer_get_insert(buffer),
        BEDIT_VIEW_SCROLL_MARGIN, FALSE, 0.0, 0.0);
}

void bedit_view_copy_clipboard(BeditView *view) {
    GtkTextBuffer *buffer;
    GtkClipboard *clipboard;

    bedit_debug(DEBUG_VIEW);

    g_return_if_fail(BEDIT_IS_VIEW(view));

    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));

    clipboard =
        gtk_widget_get_clipboard(GTK_WIDGET(view), GDK_SELECTION_CLIPBOARD);

    gtk_text_buffer_copy_clipboard(buffer, clipboard);

    /* on copy do not scroll, we are already on screen */
}

void bedit_view_paste_clipboard(BeditView *view) {
    GtkTextBuffer *buffer;
    GtkClipboard *clipboard;

    bedit_debug(DEBUG_VIEW);

    g_return_if_fail(BEDIT_IS_VIEW(view));

    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));

    clipboard =
        gtk_widget_get_clipboard(GTK_WIDGET(view), GDK_SELECTION_CLIPBOARD);

    gtk_text_buffer_paste_clipboard(
        buffer, clipboard, NULL,
        gtk_text_view_get_editable(GTK_TEXT_VIEW(view)));

    gtk_text_view_scroll_to_mark(
        GTK_TEXT_VIEW(view), gtk_text_buffer_get_insert(buffer),
        BEDIT_VIEW_SCROLL_MARGIN, FALSE, 0.0, 0.0);
}

/**
 * bedit_view_delete_selection:
 * @view: a #BeditView
 *
 * Deletes the text currently selected in the #GtkTextBuffer associated
 * to the view and scroll to the cursor position.
 */
void bedit_view_delete_selection(BeditView *view) {
    GtkTextBuffer *buffer;

    bedit_debug(DEBUG_VIEW);

    g_return_if_fail(BEDIT_IS_VIEW(view));

    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));

    gtk_text_buffer_delete_selection(
        buffer, TRUE, gtk_text_view_get_editable(GTK_TEXT_VIEW(view)));

    gtk_text_view_scroll_to_mark(
        GTK_TEXT_VIEW(view), gtk_text_buffer_get_insert(buffer),
        BEDIT_VIEW_SCROLL_MARGIN, FALSE, 0.0, 0.0);
}

/**
 * bedit_view_select_all:
 * @view: a #BeditView
 *
 * Selects all the text.
 */
void bedit_view_select_all(BeditView *view) {
    GtkTextBuffer *buffer;
    GtkTextIter start;
    GtkTextIter end;

    bedit_debug(DEBUG_VIEW);

    g_return_if_fail(BEDIT_IS_VIEW(view));

    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));

    gtk_text_buffer_get_bounds(buffer, &start, &end);
    gtk_text_buffer_select_range(buffer, &start, &end);
}

/**
 * bedit_view_scroll_to_cursor:
 * @view: a #BeditView
 *
 * Scrolls the @view to the cursor position.
 */
void bedit_view_scroll_to_cursor(BeditView *view) {
    GtkTextBuffer *buffer;

    bedit_debug(DEBUG_VIEW);

    g_return_if_fail(BEDIT_IS_VIEW(view));

    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));

    gtk_text_view_scroll_to_mark(
        GTK_TEXT_VIEW(view), gtk_text_buffer_get_insert(buffer), 0.25, FALSE,
        0.0, 0.0);
}

static void update_css_provider(BeditView *view) {
    gchar *str;
    gchar *css;

    g_assert(BEDIT_IS_VIEW(view));
    g_assert(view->priv->font_desc != NULL);

    str = bedit_pango_font_description_to_css(view->priv->font_desc);
    css = g_strdup_printf("textview { %s }", str ? str : "");
    gtk_css_provider_load_from_data(view->priv->css_provider, css, -1, NULL);

    g_free(css);
    g_free(str);
}

/**
 * bedit_view_set_font:
 * @view: a #BeditView
 * @default_font: whether to reset to the default font
 * @font_name: the name of the font to use
 *
 * If @default_font is #TRUE, resets the font of the @view to the default font.
 * Otherwise sets it to @font_name.
 */
void bedit_view_set_font(
    BeditView *view, gboolean default_font, const gchar *font_name) {
    bedit_debug(DEBUG_VIEW);

    g_return_if_fail(BEDIT_IS_VIEW(view));

    g_clear_pointer(&view->priv->font_desc, pango_font_description_free);

    if (default_font) {
        BeditSettings *settings;
        gchar *font;

        settings =
            _bedit_app_get_settings(BEDIT_APP(g_application_get_default()));
        font = bedit_settings_get_system_font(settings);

        view->priv->font_desc = pango_font_description_from_string(font);
        g_free(font);
    } else {
        g_return_if_fail(font_name != NULL);

        view->priv->font_desc = pango_font_description_from_string(font_name);
    }

    g_return_if_fail(view->priv->font_desc != NULL);

    update_css_provider(view);
}

