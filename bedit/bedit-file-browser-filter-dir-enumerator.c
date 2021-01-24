/*
 * bedit-file-browser-filter-dir-enumerator.c
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *sea
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

#include "bedit-file-browser-filter-dir-enumerator.h"

#include <glib-object.h>
#include <gmodule.h>

G_DEFINE_INTERFACE(
    BeditFileBrowserFilterDirEnumerator,
    bedit_file_browser_filter_dir_enumerator,
    G_TYPE_OBJECT
);

static void bedit_file_browser_filter_dir_enumerator_default_init(
    BeditFileBrowserFilterDirEnumeratorInterface *iface
) {
    // Pass.
}

/**
 * bedit_file_browser_filter_dir_enumerator_iterate
 * @dir: (out)
 * @path_markup: (out)
 * @cancellable:
 * @error: (out)
 *
 * Advances iter, writing the properties of the next directory to the out
 * parameters.
 *
 * Returns: %FALSE if the end of the enumerator has been reached.  If this
 *   happens, dir and path_markup will be set to NULL.
 */
gboolean bedit_file_browser_filter_dir_enumerator_iterate(
    BeditFileBrowserFilterDirEnumerator *enumerator,
    GFile **dir,
    gchar **path_markup,
    GCancellable *cancellable,
    GError **error
) {
    BeditFileBrowserFilterDirEnumeratorInterface *iface;

    g_return_val_if_fail(
        BEDIT_IS_FILE_BROWSER_FILTER_DIR_ENUMERATOR(enumerator), FALSE
    );

    iface = BEDIT_FILE_BROWSER_FILTER_DIR_ENUMERATOR_GET_IFACE(enumerator);

    g_return_val_if_fail(iface->iterate != NULL, FALSE);
    return iface->iterate(enumerator, dir, path_markup, cancellable, error);
}
