/*
 * bedit-app-private.h
 * This file is part of bedit
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

void _bedit_app_set_lockdown(BeditApp *app, BeditLockdownMask lockdown);

void _bedit_app_set_lockdown_bit(
    BeditApp *app, BeditLockdownMask bit, gboolean value);

/* This one is a bedit-window function, but we declare it here to avoid
 * #include headaches since it needs the BeditLockdownMask declaration.
 */
void _bedit_window_set_lockdown(
    BeditWindow *window, BeditLockdownMask lockdown);

/* global print config */
GtkPageSetup *_bedit_app_get_default_page_setup(BeditApp *app);
void _bedit_app_set_default_page_setup(BeditApp *app, GtkPageSetup *page_setup);
GtkPrintSettings *_bedit_app_get_default_print_settings(BeditApp *app);
void _bedit_app_set_default_print_settings(
    BeditApp *app, GtkPrintSettings *settings);

BeditSettings *_bedit_app_get_settings(BeditApp *app);

GMenuModel *_bedit_app_get_hamburger_menu(BeditApp *app);

GMenuModel *_bedit_app_get_notebook_menu(BeditApp *app);

GMenuModel *_bedit_app_get_tab_width_menu(BeditApp *app);

GMenuModel *_bedit_app_get_line_col_menu(BeditApp *app);

BeditMenuExtension *_bedit_app_extend_menu(
    BeditApp *app, const gchar *extension_point);

G_END_DECLS

#endif /* BEDIT_APP_PRIVATE_H */

