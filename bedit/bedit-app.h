/*
 * bedit-app.h
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * Based on gedit-app.h from Gedit.
 *
 * Copyright (C) 2005-2015 - Paolo Borelli
 * Copyright (C) 2005-2006 - Paolo Maggi
 * Copyright (C) 2006-2014 - Steve Frécinaux
 * Copyright (C) 2009-2012 - Ignacio Casal Quinteiro
 * Copyright (C) 2010 - Garrett Regier
 * Copyright (C) 2010-2012 - Jesse van den Kieboom
 * Copyright (C) 2014 - Sebastien Lafargue
 * Copyright (C) 2015-2016 - Sébastien Wilmet
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
        const gchar *link_id
    );

    gchar *(*help_link_id)(
        BeditApp *app, const gchar *name, const gchar *link_id
    );

    void (*set_window_title)(
        BeditApp *app, BeditWindow *window, const gchar *title
    );

    BeditWindow *(*create_window)(BeditApp *app);

    gboolean (*process_window_event)(
        BeditApp *app, BeditWindow *window, GdkEvent *event
    );
};

BeditWindow *bedit_app_create_window(BeditApp *app, GdkScreen *screen);

GList *bedit_app_get_main_windows(BeditApp *app);

GList *bedit_app_get_documents(BeditApp *app);

GList *bedit_app_get_views(BeditApp *app);

gboolean bedit_app_show_help(
    BeditApp *app, GtkWindow *parent, const gchar *name, const gchar *link_id
);

void bedit_app_set_window_title(
    BeditApp *app, BeditWindow *window, const gchar *title
);
gboolean bedit_app_process_window_event(
    BeditApp *app, BeditWindow *window, GdkEvent *event
);

G_END_DECLS

#endif /* BEDIT_APP_H */

