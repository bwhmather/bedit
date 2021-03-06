/*
 * bedit-app-private.h
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * Based on gedit-app-private.h from Gedit.
 *
 * Copyright (C) 2015 - Lars Uebernickel
 * Copyright (C) 2015-2019 - Sébastien Wilmet
 * Copyright (C) 2016 - Paolo Borelli
 * Copyright (C) 2019 - Andrea Azzarone
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

#ifndef BEDIT_APP_PRIVATE_H
#define BEDIT_APP_PRIVATE_H

#include "bedit-app.h"
#include "bedit-menu-extension.h"
#include "bedit-settings.h"

G_BEGIN_DECLS

/* global print config */
GtkPageSetup *_bedit_app_get_default_page_setup(BeditApp *app);
void _bedit_app_set_default_page_setup(
    BeditApp *app, GtkPageSetup *page_setup
);
GtkPrintSettings *_bedit_app_get_default_print_settings(BeditApp *app);
void _bedit_app_set_default_print_settings(
    BeditApp *app, GtkPrintSettings *settings
);

BeditSettings *_bedit_app_get_settings(BeditApp *app);

GMenuModel *_bedit_app_get_hamburger_menu(BeditApp *app);

GMenuModel *_bedit_app_get_notebook_menu(BeditApp *app);

GMenuModel *_bedit_app_get_tab_width_menu(BeditApp *app);

GMenuModel *_bedit_app_get_line_col_menu(BeditApp *app);

BeditMenuExtension *_bedit_app_extend_menu(
    BeditApp *app, const gchar *extension_point
);

G_END_DECLS

#endif /* BEDIT_APP_PRIVATE_H */

