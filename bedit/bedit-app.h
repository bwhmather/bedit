/*
 * bedit-app.h
 * This file is part of bedit
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef BEDIT_APP_H
#define BEDIT_APP_H

#include <bedit/bedit-window.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define BEDIT_TYPE_APP (bedit_app_get_type())

G_DECLARE_DERIVABLE_TYPE(BeditApp, bedit_app, BEDIT, APP, GtkApplication)

struct _BeditAppClass {
    GtkApplicationClass parent_class;

    gboolean (*show_help)(
        BeditApp *app, GtkWindow *parent, const gchar *name,
        const gchar *link_id);

    gchar *(*help_link_id)(
        BeditApp *app, const gchar *name, const gchar *link_id);

    void (*set_window_title)(
        BeditApp *app, BeditWindow *window, const gchar *title);

    BeditWindow *(*create_window)(BeditApp *app);

    gboolean (*process_window_event)(
        BeditApp *app, BeditWindow *window, GdkEvent *event);
};

typedef enum {
    BEDIT_LOCKDOWN_COMMAND_LINE = 1 << 0,
    BEDIT_LOCKDOWN_PRINTING = 1 << 1,
    BEDIT_LOCKDOWN_PRINT_SETUP = 1 << 2,
    BEDIT_LOCKDOWN_SAVE_TO_DISK = 1 << 3
} BeditLockdownMask;

/* We need to define this here to avoid problems with bindings and gsettings */
#define BEDIT_LOCKDOWN_ALL 0xF

BeditWindow *bedit_app_create_window(BeditApp *app, GdkScreen *screen);

GList *bedit_app_get_main_windows(BeditApp *app);

GList *bedit_app_get_documents(BeditApp *app);

GList *bedit_app_get_views(BeditApp *app);

/* Lockdown state */
BeditLockdownMask bedit_app_get_lockdown(BeditApp *app);

gboolean bedit_app_show_help(
    BeditApp *app, GtkWindow *parent, const gchar *name, const gchar *link_id);

void bedit_app_set_window_title(
    BeditApp *app, BeditWindow *window, const gchar *title);
gboolean bedit_app_process_window_event(
    BeditApp *app, BeditWindow *window, GdkEvent *event);

G_END_DECLS

#endif /* BEDIT_APP_H */

