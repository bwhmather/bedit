/*
 * bedit-print-job.c
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * Based on gedit-print-job.c from Gedit.
 *
 * Copyright (C) 2000-2001 - Chema Celorio
 * Copyright (C) 2000-2008 - Paolo Maggi
 * Copyright (C) 2008-2010 - Jesse van den Kieboom
 * Copyright (C) 2008-2015 - Paolo Borelli
 * Copyright (C) 2009 - Marek Kašík
 * Copyright (C) 2009-2012 - Ignacio Casal Quinteiro
 * Copyright (C) 2010 - Steve Frécinaux
 * Copyright (C) 2010-2011 - Garrett Regier
 * Copyright (C) 2011 - Laura Lazzati
 * Copyright (C) 2013-2019 - Sébastien Wilmet
 * Copyright (C) 2014 - Robert Roth
 * Copyright (C) 2015 - Maks Naumov
 * Copyright (C) 2016-2017 - Piotr Drąg
 * Copyright (C) 2018 - Sebastien Lafargue
 * Copyright (C) 2019 - Michael Catanzaro
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

#include "bedit-print-job.h"

#include <glib/gi18n.h>

#include "bedit-debug.h"
#include "bedit-dirs.h"
#include "bedit-print-preview.h"
#include "bedit-settings.h"
#include "bedit-utils.h"

struct _BeditPrintJob {
    GObject parent_instance;

    GSettings *gsettings;

    BeditView *view;

    GtkPrintOperation *operation;
    GtkSourcePrintCompositor *compositor;

    GtkWidget *preview;

    gchar *status_string;
    gdouble progress;

    /* Widgets part of the custom print preferences widget.
     * These pointers are valid just when the dialog is displayed.
     */
    GtkToggleButton *syntax_checkbutton;
    GtkToggleButton *page_header_checkbutton;
    GtkToggleButton *line_numbers_checkbutton;
    GtkSpinButton *line_numbers_spinbutton;
    GtkToggleButton *text_wrapping_checkbutton;
    GtkToggleButton *do_not_split_checkbutton;
    GtkFontButton *body_fontbutton;
    GtkFontButton *headers_fontbutton;
    GtkFontButton *numbers_fontbutton;

    guint is_preview : 1;
};

enum { PROP_0, PROP_VIEW, LAST_PROP };

static GParamSpec *properties[LAST_PROP];

enum { PRINTING, SHOW_PREVIEW, DONE, LAST_SIGNAL };

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE(BeditPrintJob, bedit_print_job, G_TYPE_OBJECT)

static void bedit_print_job_get_property(
    GObject *object, guint prop_id, GValue *value, GParamSpec *pspec
) {
    BeditPrintJob *job = BEDIT_PRINT_JOB(object);

    switch (prop_id) {
    case PROP_VIEW:
        g_value_set_object(value, job->view);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void bedit_print_job_set_property(
    GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec
) {
    BeditPrintJob *job = BEDIT_PRINT_JOB(object);

    switch (prop_id) {
    case PROP_VIEW:
        job->view = g_value_get_object(value);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void bedit_print_job_dispose(GObject *object) {
    BeditPrintJob *job = BEDIT_PRINT_JOB(object);

    g_clear_object(&job->gsettings);
    g_clear_object(&job->operation);
    g_clear_object(&job->compositor);
    g_clear_object(&job->preview);

    G_OBJECT_CLASS(bedit_print_job_parent_class)->dispose(object);
}

static void bedit_print_job_finalize(GObject *object) {
    BeditPrintJob *job = BEDIT_PRINT_JOB(object);

    g_free(job->status_string);

    G_OBJECT_CLASS(bedit_print_job_parent_class)->finalize(object);
}

static void bedit_print_job_printing(
    BeditPrintJob *job, BeditPrintJobStatus status
) {}

static void bedit_print_job_show_preview(
    BeditPrintJob *job, GtkWidget *preview
) {}

static void bedit_print_job_done(
    BeditPrintJob *job, BeditPrintJobResult result, const GError *error
) {}

static void bedit_print_job_class_init(BeditPrintJobClass *klass) {
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS(klass);

    object_class->get_property = bedit_print_job_get_property;
    object_class->set_property = bedit_print_job_set_property;
    object_class->dispose = bedit_print_job_dispose;
    object_class->finalize = bedit_print_job_finalize;

    properties[PROP_VIEW] = g_param_spec_object(
        "view", "Bedit View", "Bedit View to print", BEDIT_TYPE_VIEW,
        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY
    );

    g_object_class_install_properties(object_class, LAST_PROP, properties);

    signals[PRINTING] = g_signal_new_class_handler(
        "printing", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST,
        G_CALLBACK(bedit_print_job_printing), NULL, NULL, NULL, G_TYPE_NONE,
        1, G_TYPE_UINT
    );

    signals[SHOW_PREVIEW] = g_signal_new_class_handler(
        "show-preview", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST,
        G_CALLBACK(bedit_print_job_show_preview), NULL, NULL, NULL, G_TYPE_NONE,
        1, GTK_TYPE_WIDGET
    );

    signals[DONE] = g_signal_new_class_handler(
        "done", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST,
        G_CALLBACK(bedit_print_job_done), NULL, NULL, NULL, G_TYPE_NONE,
        2, G_TYPE_UINT, G_TYPE_POINTER
    );
}

static void bedit_print_job_init(BeditPrintJob *job) {
    job->gsettings = g_settings_new("com.bwhmather.bedit.preferences.print");

    job->status_string = g_strdup(_("Preparing…"));
}

static void restore_button_clicked(GtkButton *button, BeditPrintJob *job)

{
    g_settings_reset(job->gsettings, BEDIT_SETTINGS_PRINT_FONT_BODY_PANGO);
    g_settings_reset(job->gsettings, BEDIT_SETTINGS_PRINT_FONT_HEADER_PANGO);
    g_settings_reset(job->gsettings, BEDIT_SETTINGS_PRINT_FONT_NUMBERS_PANGO);
}

static GObject *create_custom_widget_cb(
    GtkPrintOperation *operation, BeditPrintJob *job
) {
    GtkBuilder *builder;
    GtkWidget *contents;
    GtkWidget *line_numbers_hbox;
    GtkWidget *restore_button;
    guint line_numbers;
    GtkWrapMode wrap_mode;

    builder = gtk_builder_new_from_resource(
        "/com/bwhmather/bedit/ui/bedit-print-preferences.ui"
    );
    contents = GTK_WIDGET(gtk_builder_get_object(builder, "contents"));
    g_object_ref(contents);
    job->syntax_checkbutton = GTK_TOGGLE_BUTTON(
        gtk_builder_get_object(builder, "syntax_checkbutton")
    );
    job->line_numbers_checkbutton = GTK_TOGGLE_BUTTON(
        gtk_builder_get_object(builder, "line_numbers_checkbutton")
    );
    line_numbers_hbox =
        GTK_WIDGET(gtk_builder_get_object(builder, "line_numbers_hbox")
    );
    job->line_numbers_spinbutton = GTK_SPIN_BUTTON(
        gtk_builder_get_object(builder, "line_numbers_spinbutton")
    );
    job->page_header_checkbutton = GTK_TOGGLE_BUTTON(
        gtk_builder_get_object(builder, "page_header_checkbutton")
    );
    job->text_wrapping_checkbutton = GTK_TOGGLE_BUTTON(
        gtk_builder_get_object(builder, "text_wrapping_checkbutton")
    );
    job->do_not_split_checkbutton = GTK_TOGGLE_BUTTON(
        gtk_builder_get_object(builder, "do_not_split_checkbutton")
    );
    job->body_fontbutton =
        GTK_FONT_BUTTON(gtk_builder_get_object(builder, "body_fontbutton")
    );
    job->headers_fontbutton =
        GTK_FONT_BUTTON(gtk_builder_get_object(builder, "headers_fontbutton")
    );
    job->numbers_fontbutton =
        GTK_FONT_BUTTON(gtk_builder_get_object(builder, "numbers_fontbutton")
    );
    restore_button =
        GTK_WIDGET(gtk_builder_get_object(builder, "restore_button")
    );
    g_object_unref(builder);

    /* Syntax highlighting */
    g_settings_bind(
        job->gsettings, BEDIT_SETTINGS_PRINT_SYNTAX_HIGHLIGHTING,
        job->syntax_checkbutton,
        "active", G_SETTINGS_BIND_GET
    );

    /* Print header */
    g_settings_bind(
        job->gsettings, BEDIT_SETTINGS_PRINT_HEADER,
        job->page_header_checkbutton,
        "active", G_SETTINGS_BIND_GET
    );

    /* Line numbers */
    g_settings_get(
        job->gsettings, BEDIT_SETTINGS_PRINT_LINE_NUMBERS, "u", &line_numbers
    );

    if (line_numbers > 0) {
        gtk_spin_button_set_value(job->line_numbers_spinbutton, line_numbers);
    } else {
        gtk_spin_button_set_value(job->line_numbers_spinbutton, 1);
    }

    gtk_toggle_button_set_active(
        job->line_numbers_checkbutton, line_numbers > 0
    );

    g_object_bind_property(
        job->line_numbers_checkbutton, "active",
        line_numbers_hbox, "sensitive",
        G_BINDING_SYNC_CREATE
    );

    /* Fonts */
    g_settings_bind(
        job->gsettings, BEDIT_SETTINGS_PRINT_FONT_BODY_PANGO,
        job->body_fontbutton, "font-name", G_SETTINGS_BIND_GET
    );

    g_settings_bind(
        job->gsettings, BEDIT_SETTINGS_PRINT_FONT_HEADER_PANGO,
        job->headers_fontbutton, "font-name", G_SETTINGS_BIND_GET
    );

    g_settings_bind(
        job->gsettings, BEDIT_SETTINGS_PRINT_FONT_NUMBERS_PANGO,
        job->numbers_fontbutton, "font-name", G_SETTINGS_BIND_GET
    );

    /* Wrap mode */
    wrap_mode = g_settings_get_enum(
        job->gsettings, BEDIT_SETTINGS_PRINT_WRAP_MODE
    );

    switch (wrap_mode) {
    case GTK_WRAP_WORD:
        gtk_toggle_button_set_active(job->text_wrapping_checkbutton, TRUE);
        gtk_toggle_button_set_active(job->do_not_split_checkbutton, TRUE);
        break;

    case GTK_WRAP_CHAR:
        gtk_toggle_button_set_active(job->text_wrapping_checkbutton, TRUE);
        gtk_toggle_button_set_active(job->do_not_split_checkbutton, FALSE);
        break;

    default:
        gtk_toggle_button_set_active(job->text_wrapping_checkbutton, FALSE);
        break;
    }

    g_object_bind_property(
        job->text_wrapping_checkbutton, "active",
        job->do_not_split_checkbutton, "sensitive",
        G_BINDING_SYNC_CREATE
    );

    g_object_bind_property(
        job->text_wrapping_checkbutton, "active",
        job->do_not_split_checkbutton, "inconsistent",
        G_BINDING_INVERT_BOOLEAN | G_BINDING_SYNC_CREATE
    );

    /* Restore */
    g_signal_connect(
        restore_button, "clicked",
        G_CALLBACK(restore_button_clicked), job
    );

    return G_OBJECT(contents);
}

static void custom_widget_apply_cb(
    GtkPrintOperation *operation, GtkWidget *widget, BeditPrintJob *job
) {
    gboolean syntax;
    gboolean page_header;
    const gchar *body_font;
    const gchar *header_font;
    const gchar *numbers_font;
    GtkWrapMode wrap_mode;

    syntax = gtk_toggle_button_get_active(job->syntax_checkbutton);
    page_header = gtk_toggle_button_get_active(job->page_header_checkbutton);
    body_font = gtk_font_chooser_get_font(
        GTK_FONT_CHOOSER(job->body_fontbutton)
    );
    header_font = gtk_font_chooser_get_font(
        GTK_FONT_CHOOSER(job->headers_fontbutton)
    );
    numbers_font = gtk_font_chooser_get_font(
        GTK_FONT_CHOOSER(job->numbers_fontbutton)
    );

    g_settings_set_boolean(
        job->gsettings, BEDIT_SETTINGS_PRINT_SYNTAX_HIGHLIGHTING, syntax
    );

    g_settings_set_boolean(
        job->gsettings, BEDIT_SETTINGS_PRINT_HEADER, page_header
    );

    g_settings_set_string(
        job->gsettings, BEDIT_SETTINGS_PRINT_FONT_BODY_PANGO, body_font
    );

    g_settings_set_string(
        job->gsettings, BEDIT_SETTINGS_PRINT_FONT_HEADER_PANGO, header_font
    );

    g_settings_set_string(
        job->gsettings, BEDIT_SETTINGS_PRINT_FONT_NUMBERS_PANGO, numbers_font
    );

    if (gtk_toggle_button_get_active(job->line_numbers_checkbutton)) {
        gint num;

        num = gtk_spin_button_get_value_as_int(job->line_numbers_spinbutton);

        g_settings_set(
            job->gsettings, BEDIT_SETTINGS_PRINT_LINE_NUMBERS, "u",
            MAX(1, num)
        );
    } else {
        g_settings_set(
            job->gsettings, BEDIT_SETTINGS_PRINT_LINE_NUMBERS, "u", 0
        );
    }

    if (gtk_toggle_button_get_active(job->text_wrapping_checkbutton)) {
        if (gtk_toggle_button_get_active(job->do_not_split_checkbutton)) {
            wrap_mode = GTK_WRAP_WORD;
        } else {
            wrap_mode = GTK_WRAP_CHAR;
        }
    } else {
        wrap_mode = GTK_WRAP_NONE;
    }

    g_settings_set_enum(
        job->gsettings, BEDIT_SETTINGS_PRINT_WRAP_MODE, wrap_mode
    );
}

static void preview_ready(
    GtkPrintOperationPreview *gtk_preview, GtkPrintContext *context,
    BeditPrintJob *job
) {
    job->is_preview = TRUE;

    g_signal_emit(job, signals[SHOW_PREVIEW], 0, job->preview);

    g_clear_object(&job->preview);
}

static gboolean preview_cb(
    GtkPrintOperation *op, GtkPrintOperationPreview *gtk_preview,
    GtkPrintContext *context, GtkWindow *parent, BeditPrintJob *job
) {
    g_clear_object(&job->preview);
    job->preview = bedit_print_preview_new(op, gtk_preview, context);
    g_object_ref_sink(job->preview);

    g_signal_connect_after(
        gtk_preview, "ready", G_CALLBACK(preview_ready), job
    );

    return TRUE;
}

static void create_compositor(BeditPrintJob *job) {
    GtkSourceBuffer *buf;
    gchar *print_font_body;
    gchar *print_font_header;
    gchar *print_font_numbers;
    gboolean syntax_hl;
    GtkWrapMode wrap_mode;
    guint print_line_numbers;
    gboolean print_header;
    guint tab_width;
    gdouble margin;

    buf = GTK_SOURCE_BUFFER(gtk_text_view_get_buffer(
        GTK_TEXT_VIEW(job->view)
    ));

    print_font_body = g_settings_get_string(
        job->gsettings, BEDIT_SETTINGS_PRINT_FONT_BODY_PANGO
    );

    print_font_header = g_settings_get_string(
        job->gsettings, BEDIT_SETTINGS_PRINT_FONT_HEADER_PANGO
    );

    print_font_numbers = g_settings_get_string(
        job->gsettings, BEDIT_SETTINGS_PRINT_FONT_NUMBERS_PANGO
    );

    g_settings_get(
        job->gsettings, BEDIT_SETTINGS_PRINT_LINE_NUMBERS, "u",
        &print_line_numbers
    );

    print_header = g_settings_get_boolean(
        job->gsettings, BEDIT_SETTINGS_PRINT_HEADER
    );

    wrap_mode = g_settings_get_enum(
        job->gsettings, BEDIT_SETTINGS_PRINT_WRAP_MODE
    );

    syntax_hl = g_settings_get_boolean(
        job->gsettings, BEDIT_SETTINGS_PRINT_SYNTAX_HIGHLIGHTING
    );

    syntax_hl &= gtk_source_buffer_get_highlight_syntax(buf);

    tab_width = gtk_source_view_get_tab_width(GTK_SOURCE_VIEW(job->view));

    job->compositor = GTK_SOURCE_PRINT_COMPOSITOR(g_object_new(
        GTK_SOURCE_TYPE_PRINT_COMPOSITOR,
        "buffer", buf,
        "tab-width", tab_width,
        "highlight-syntax", syntax_hl,
        "wrap-mode", wrap_mode,
        "print-line-numbers", print_line_numbers,
        "print-header", print_header,
        "print-footer", FALSE,
        "body-font-name", print_font_body,
        "line-numbers-font-name", print_font_numbers,
        "header-font-name", print_font_header,
        NULL
    ));

    margin = g_settings_get_double(
        job->gsettings, BEDIT_SETTINGS_PRINT_MARGIN_LEFT
    );
    gtk_source_print_compositor_set_left_margin(
        job->compositor, margin, GTK_UNIT_MM
    );

    margin = g_settings_get_double(
        job->gsettings, BEDIT_SETTINGS_PRINT_MARGIN_TOP
    );
    gtk_source_print_compositor_set_top_margin(
        job->compositor, margin, GTK_UNIT_MM
    );

    margin = g_settings_get_double(
        job->gsettings, BEDIT_SETTINGS_PRINT_MARGIN_RIGHT
    );
    gtk_source_print_compositor_set_right_margin(
        job->compositor, margin, GTK_UNIT_MM
    );

    margin = g_settings_get_double(
        job->gsettings, BEDIT_SETTINGS_PRINT_MARGIN_BOTTOM
    );
    gtk_source_print_compositor_set_bottom_margin(
        job->compositor, margin, GTK_UNIT_MM
    );

    if (print_header) {
        gchar *doc_name;
        gchar *name_to_display;
        gchar *left;

        doc_name = bedit_document_get_uri_for_display(BEDIT_DOCUMENT(buf));
        name_to_display = tepl_utils_str_middle_truncate(doc_name, 60);

        left = g_strdup_printf(_("File: %s"), name_to_display);

        gtk_source_print_compositor_set_header_format(
            job->compositor, TRUE, left, NULL,
            /* Translators: %N is the current page number, %Q is the total
             * number of pages (ex. Page 2 of 10)
             */
            _("Page %N of %Q")
        );

        g_free(doc_name);
        g_free(name_to_display);
        g_free(left);
    }

    g_free(print_font_body);
    g_free(print_font_header);
    g_free(print_font_numbers);
}

static void begin_print_cb(
    GtkPrintOperation *operation,
    GtkPrintContext *context,
    BeditPrintJob *job
) {
    create_compositor(job);

    job->progress = 0.0;

    g_signal_emit(
        job, signals[PRINTING], 0, BEDIT_PRINT_JOB_STATUS_PAGINATING
    );
}

static gboolean paginate_cb(
    GtkPrintOperation *operation,
    GtkPrintContext *context,
    BeditPrintJob *job
) {
    gboolean finished;

    finished = gtk_source_print_compositor_paginate(job->compositor, context);

    if (finished) {
        gint n_pages;

        n_pages = gtk_source_print_compositor_get_n_pages(job->compositor);
        gtk_print_operation_set_n_pages(job->operation, n_pages);
    }

    job->progress = gtk_source_print_compositor_get_pagination_progress(
        job->compositor
    );

    /* When previewing, the progress is just for pagination, when printing
     * it's split between pagination and rendering.
     */
    if (!job->is_preview) {
        job->progress /= 2.0;
    }

    g_signal_emit(
        job, signals[PRINTING], 0, BEDIT_PRINT_JOB_STATUS_PAGINATING
    );

    return finished;
}

static void draw_page_cb(
    GtkPrintOperation *operation, GtkPrintContext *context,
    gint page_nr, BeditPrintJob *job
) {
    /* In preview, pages are drawn on the fly, so rendering is
     * not part of the progress.
     */
    if (!job->is_preview) {
        gint n_pages;

        n_pages = gtk_source_print_compositor_get_n_pages(job->compositor);

        g_free(job->status_string);
        job->status_string = g_strdup_printf(
            _("Rendering page %d of %d…"), page_nr + 1, n_pages
        );

        job->progress = page_nr / (2.0 * n_pages) + 0.5;

        g_signal_emit(
            job, signals[PRINTING], 0, BEDIT_PRINT_JOB_STATUS_DRAWING
        );
    }

    gtk_source_print_compositor_draw_page(job->compositor, context, page_nr);
}

static void end_print_cb(
    GtkPrintOperation *operation,
    GtkPrintContext *context,
    BeditPrintJob *job
) {
    g_clear_object(&job->compositor);
}

static void done_cb(
    GtkPrintOperation *operation, GtkPrintOperationResult result,
    BeditPrintJob *job
) {
    GError *error = NULL;
    BeditPrintJobResult print_result;

    switch (result) {
    case GTK_PRINT_OPERATION_RESULT_CANCEL:
        print_result = BEDIT_PRINT_JOB_RESULT_CANCEL;
        break;

    case GTK_PRINT_OPERATION_RESULT_APPLY:
        print_result = BEDIT_PRINT_JOB_RESULT_OK;
        break;

    case GTK_PRINT_OPERATION_RESULT_ERROR:
        print_result = BEDIT_PRINT_JOB_RESULT_ERROR;
        gtk_print_operation_get_error(operation, &error);
        break;

    default:
        g_return_if_reached();
    }

    /* Avoid that job is destroyed in the handler of the "done" message. */
    g_object_ref(job);
    g_signal_emit(job, signals[DONE], 0, print_result, error);
    g_object_unref(job);
}

BeditPrintJob *bedit_print_job_new(BeditView *view) {
    g_return_val_if_fail(BEDIT_IS_VIEW(view), NULL);

    return g_object_new(BEDIT_TYPE_PRINT_JOB, "view", view, NULL);
}

/* Note that bedit_print_job_print() can only be called once on a given
 * BeditPrintJob.
 */
GtkPrintOperationResult bedit_print_job_print(
    BeditPrintJob *job, GtkPrintOperationAction action,
    GtkPageSetup *page_setup, GtkPrintSettings *settings, GtkWindow *parent,
    GError **error
) {
    BeditDocument *doc;
    gchar *job_name;

    g_return_val_if_fail(
        job->operation == NULL, GTK_PRINT_OPERATION_RESULT_ERROR
    );
    g_return_val_if_fail(
        job->compositor == NULL, GTK_PRINT_OPERATION_RESULT_ERROR
    );

    job->operation = gtk_print_operation_new();

    job->is_preview = action == GTK_PRINT_OPERATION_ACTION_PREVIEW;

    if (settings != NULL) {
        gtk_print_operation_set_print_settings(job->operation, settings);
    }

    if (page_setup != NULL) {
        gtk_print_operation_set_default_page_setup(job->operation, page_setup);
    }

    doc = BEDIT_DOCUMENT(gtk_text_view_get_buffer(GTK_TEXT_VIEW(job->view)));
    job_name = bedit_document_get_short_name_for_display(doc);
    gtk_print_operation_set_job_name(job->operation, job_name);
    g_free(job_name);

    gtk_print_operation_set_embed_page_setup(job->operation, TRUE);

    gtk_print_operation_set_custom_tab_label(job->operation, _("Text Editor"));

    gtk_print_operation_set_allow_async(job->operation, TRUE);

    g_signal_connect(
        job->operation, "create-custom-widget",
        G_CALLBACK(create_custom_widget_cb), job
    );

    g_signal_connect(
        job->operation, "custom-widget-apply",
        G_CALLBACK(custom_widget_apply_cb), job
    );

    g_signal_connect(
        job->operation, "preview",
        G_CALLBACK(preview_cb), job
    );

    g_signal_connect(
        job->operation, "begin-print",
        G_CALLBACK(begin_print_cb), job
    );

    g_signal_connect(
        job->operation, "paginate",
        G_CALLBACK(paginate_cb), job
    );

    g_signal_connect(
        job->operation, "draw-page",
        G_CALLBACK(draw_page_cb), job
    );

    g_signal_connect_object(
        job->operation, "end-print",
        G_CALLBACK(end_print_cb), job, 0
    );

    g_signal_connect_object(
        job->operation, "done",
        G_CALLBACK(done_cb), job, 0
    );

    return gtk_print_operation_run(job->operation, action, parent, error);
}

void bedit_print_job_cancel(BeditPrintJob *job) {
    g_return_if_fail(BEDIT_IS_PRINT_JOB(job));

    gtk_print_operation_cancel(job->operation);
}

const gchar *bedit_print_job_get_status_string(BeditPrintJob *job) {
    g_return_val_if_fail(BEDIT_IS_PRINT_JOB(job), NULL);
    g_return_val_if_fail(job->status_string != NULL, NULL);

    return job->status_string;
}

gdouble bedit_print_job_get_progress(BeditPrintJob *job) {
    g_return_val_if_fail(BEDIT_IS_PRINT_JOB(job), 0.0);

    return job->progress;
}

GtkPrintSettings *bedit_print_job_get_print_settings(BeditPrintJob *job) {
    g_return_val_if_fail(BEDIT_IS_PRINT_JOB(job), NULL);

    return gtk_print_operation_get_print_settings(job->operation);
}

GtkPageSetup *bedit_print_job_get_page_setup(BeditPrintJob *job) {
    g_return_val_if_fail(BEDIT_IS_PRINT_JOB(job), NULL);

    return gtk_print_operation_get_default_page_setup(job->operation);
}

