/*
 * bedit-print-job.h
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * Based on gedit-print-job.h from Gedit.
 *
 * Copyright (C) 2000-2001 - Chema Celorio
 * Copyright (C) 2008-2015 - Paolo Borelli
 * Copyright (C) 2009 - Marek Kašík
 * Copyright (C) 2010 - Garrett Regier, Jesse van den Kieboom, Steve Frécinaux
 * Copyright (C) 2013-2016 - Sébastien Wilmet
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

#ifndef BEDIT_PRINT_JOB_H
#define BEDIT_PRINT_JOB_H

#include <bedit/bedit-view.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define BEDIT_TYPE_PRINT_JOB (bedit_print_job_get_type())

G_DECLARE_FINAL_TYPE(BeditPrintJob, bedit_print_job, BEDIT, PRINT_JOB, GObject)

typedef enum {
    BEDIT_PRINT_JOB_STATUS_PAGINATING,
    BEDIT_PRINT_JOB_STATUS_DRAWING
} BeditPrintJobStatus;

typedef enum {
    BEDIT_PRINT_JOB_RESULT_OK,
    BEDIT_PRINT_JOB_RESULT_CANCEL,
    BEDIT_PRINT_JOB_RESULT_ERROR
} BeditPrintJobResult;

BeditPrintJob *bedit_print_job_new(BeditView *view);

GtkPrintOperationResult bedit_print_job_print(
    BeditPrintJob *job, GtkPrintOperationAction action,
    GtkPageSetup *page_setup, GtkPrintSettings *settings, GtkWindow *parent,
    GError **error
);

void bedit_print_job_cancel(BeditPrintJob *job);

const gchar *bedit_print_job_get_status_string(BeditPrintJob *job);

gdouble bedit_print_job_get_progress(BeditPrintJob *job);

GtkPrintSettings *bedit_print_job_get_print_settings(BeditPrintJob *job);

GtkPageSetup *bedit_print_job_get_page_setup(BeditPrintJob *job);

G_END_DECLS

#endif /* BEDIT_PRINT_JOB_H */

