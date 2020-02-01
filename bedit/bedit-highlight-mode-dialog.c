/*
 * bedit-highlight-mode-dialog.c
 * This file is part of bedit
 *
 * Copyright (C) 2013 - Ignacio Casal Quinteiro
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

#include "bedit-highlight-mode-dialog.h"

#include <gtk/gtk.h>

struct _BeditHighlightModeDialog {
    GtkDialog parent_instance;

    BeditHighlightModeSelector *selector;
    gulong on_language_selected_id;
};

G_DEFINE_TYPE(
    BeditHighlightModeDialog, bedit_highlight_mode_dialog, GTK_TYPE_DIALOG)

static void bedit_highlight_mode_dialog_response(
    GtkDialog *dialog, gint response_id) {
    BeditHighlightModeDialog *dlg = BEDIT_HIGHLIGHT_MODE_DIALOG(dialog);

    if (response_id == GTK_RESPONSE_OK) {
        g_signal_handler_block(dlg->selector, dlg->on_language_selected_id);
        bedit_highlight_mode_selector_activate_selected_language(dlg->selector);
        g_signal_handler_unblock(dlg->selector, dlg->on_language_selected_id);
    }

    gtk_widget_destroy(GTK_WIDGET(dialog));
}

static void on_language_selected(
    BeditHighlightModeSelector *sel, GtkSourceLanguage *language,
    BeditHighlightModeDialog *dlg) {
    g_signal_handler_block(dlg->selector, dlg->on_language_selected_id);
    bedit_highlight_mode_selector_activate_selected_language(dlg->selector);
    g_signal_handler_unblock(dlg->selector, dlg->on_language_selected_id);

    gtk_widget_destroy(GTK_WIDGET(dlg));
}

static void bedit_highlight_mode_dialog_class_init(
    BeditHighlightModeDialogClass *klass) {
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
    GtkDialogClass *dialog_class = GTK_DIALOG_CLASS(klass);

    dialog_class->response = bedit_highlight_mode_dialog_response;

    /* Bind class to template */
    gtk_widget_class_set_template_from_resource(
        widget_class, "/com/bwhmather/bedit/ui/bedit-highlight-mode-dialog.ui");
    gtk_widget_class_bind_template_child(
        widget_class, BeditHighlightModeDialog, selector);
}

static void bedit_highlight_mode_dialog_init(BeditHighlightModeDialog *dlg) {
    gtk_widget_init_template(GTK_WIDGET(dlg));
    gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_OK);

    dlg->on_language_selected_id = g_signal_connect(
        dlg->selector, "language-selected", G_CALLBACK(on_language_selected),
        dlg);
}

GtkWidget *bedit_highlight_mode_dialog_new(GtkWindow *parent) {
    return GTK_WIDGET(g_object_new(
        BEDIT_TYPE_HIGHLIGHT_MODE_DIALOG, "transient-for", parent,
        "use-header-bar", TRUE, NULL));
}

BeditHighlightModeSelector *bedit_highlight_mode_dialog_get_selector(
    BeditHighlightModeDialog *dlg) {
    g_return_val_if_fail(BEDIT_IS_HIGHLIGHT_MODE_DIALOG(dlg), NULL);

    return dlg->selector;
}

