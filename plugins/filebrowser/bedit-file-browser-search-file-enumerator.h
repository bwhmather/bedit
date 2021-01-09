/*
 * bedit-file-browser-search-file-enumerator.h
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
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

#ifndef BEDIT_FILE_BROWSER_SEARCH_FILE_ENUMERATOR_H
#define BEDIT_FILE_BROWSER_SEARCH_FILE_ENUMERATOR_H

#include <gio/gio.h>
#include <glib-object.h>
#include <gmodule.h>

#include "bedit-file-browser-search-dir-enumerator.h"

G_BEGIN_DECLS

#define BEDIT_TYPE_FILE_BROWSER_SEARCH_FILE_ENUMERATOR                  \
    (bedit_file_browser_search_file_enumerator_get_type())

G_DECLARE_FINAL_TYPE(
    BeditFileBrowserSearchFileEnumerator,
    bedit_file_browser_search_file_enumerator,
    BEDIT, FILE_BROWSER_SEARCH_FILE_ENUMERATOR,
    GObject
)

GType bedit_file_browser_search_file_enumerator_get_type(void) G_GNUC_CONST;

BeditFileBrowserSearchFileEnumerator
*bedit_file_browser_search_file_enumerator_new(
    BeditFileBrowserSearchDirEnumerator *source, gchar *pattern
);

gboolean bedit_file_browser_search_file_enumerator_iterate(
    BeditFileBrowserSearchFileEnumerator *enumerator,
    GFile **out_file,
    GFileInfo **out_info,
    gchar **out_markup,
    GCancellable *cancellable,
    GError **error
);

void _bedit_file_browser_search_file_enumerator_register_type(
    GTypeModule *type_module
);

G_END_DECLS

#endif /* BEDIT_FILE_BROWSER_SEARCH_FILE_ENUMERATOR_H */
