/*
 * bedit-file-browser-dir-cache.h
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

#ifndef BEDIT_FILE_BROWSER_DIR_CACHE_H
#define BEDIT_FILE_BROWSER_DIR_CACHE_H

#include <glib.h>

#include "bedit-file-browser-dir.h"

G_BEGIN_DECLS

#define BEDIT_TYPE_FILE_BROWSER_DIR_CACHE                                   \
    (bedit_file_browser_dir_cache_get_type())

G_DECLARE_FINAL_TYPE(
    BeditFileBrowserDirCache, bedit_file_browser_dir_cache,
    BEDIT, FILE_BROWSER_DIR_CACHE,
    GInitiallyUnowned
)

GType bedit_file_browser_dir_cache_get_type(void) G_GNUC_CONST;

typedef struct _BeditFileBrowserDirCacheIter BeditFileBrowserDirCacheIter;
struct _BeditFileBrowserDirCacheIter {
    GHashTableIter iter;
    GFile *file;
    GFileInfo *info;
};

BeditFileBrowserDirCache *bedit_file_browser_dir_cache_new(GFile *file);

BeditFileBrowserDir *bedit_file_browser_dir_cache_get_dir(
    BeditFileBrowserDirCache *dir_cache, GFile *file
);

void _bedit_file_browser_dir_cache_register_type(GTypeModule *type_module);

G_END_DECLS

#endif /* BEDIT_FILE_BROWSER_DIR_CACHE_H */

