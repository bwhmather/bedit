/*
 * bedit-file-browser-plugin.c
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * Based on gedit-file-browser-plugin.c from Gedit.
 *
 * Copyright (C) 2006 - Jesse van den Kieboom <jesse@icecrew.nl>
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

#include "config.h"

#include <bedit/bedit-app.h>
#include <bedit/bedit-commands.h>
#include <bedit/bedit-debug.h>
#include <bedit/bedit-utils.h>
#include <bedit/bedit-app-activatable.h>
#include <bedit/bedit-window-activatable.h>
#include <bedit/bedit-window.h>
#include <glib/gi18n-lib.h>
#include <gmodule.h>
#include <string.h>

#include "bedit-file-browser-app-activatable.h"
#include "bedit-file-browser-window-activatable.h"
#include "bedit-file-browser-enum-types.h"
#include "bedit-file-browser-error.h"
#include "bedit-file-browser-messages.h"
#include "bedit-file-browser-plugin.h"
#include "bedit-file-browser-utils.h"
#include "bedit-file-browser-location.h"
#include "bedit-file-browser-widget.h"
#include "bedit-file-browser-search-root-dir-enumerator.h"
#include "bedit-file-browser-search-child-dir-enumerator.h"
#include "bedit-file-browser-search-file-enumerator.h"
#include "bedit-file-browser-search-view.h"

G_MODULE_EXPORT void peas_register_types(PeasObjectModule *module) {
    bedit_file_browser_enum_and_flag_register_type(G_TYPE_MODULE(module));
    _bedit_file_browser_app_activatable_register_type(G_TYPE_MODULE(module));
    _bedit_file_browser_window_activatable_register_type(G_TYPE_MODULE(module));
    _bedit_file_browser_bookmarks_store_register_type(G_TYPE_MODULE(module));
    _bedit_file_browser_store_register_type(G_TYPE_MODULE(module));
    _bedit_file_browser_view_register_type(G_TYPE_MODULE(module));
    _bedit_file_browser_location_register_type(G_TYPE_MODULE(module));
    _bedit_file_browser_widget_register_type(G_TYPE_MODULE(module));
    _bedit_file_browser_search_root_dir_enumerator_register_type(
        G_TYPE_MODULE(module)
    );
    _bedit_file_browser_search_child_dir_enumerator_register_type(
        G_TYPE_MODULE(module)
    );
    _bedit_file_browser_search_file_enumerator_register_type(
        G_TYPE_MODULE(module)
    );
    _bedit_file_browser_search_view_register_type(G_TYPE_MODULE(module));

    peas_object_module_register_extension_type(
        module, BEDIT_TYPE_APP_ACTIVATABLE,
        BEDIT_TYPE_FILE_BROWSER_APP_ACTIVATABLE
    );
    peas_object_module_register_extension_type(
        module, BEDIT_TYPE_WINDOW_ACTIVATABLE,
        BEDIT_TYPE_FILE_BROWSER_WINDOW_ACTIVATABLE
    );
}

