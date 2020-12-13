/*
 * bedit-file-browser-dir.h
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

#ifndef BEDIT_FILE_BROWSER_DIR_H
#define BEDIT_FILE_BROWSER_DIR_H

#include <glib.h>

G_BEGIN_DECLS

#define BEDIT_TYPE_FILE_BROWSER_DIR (bedit_file_browser_dir_get_type())

G_DECLARE_FINAL_TYPE(
    BeditFileBrowserDir, bedit_file_browser_dir,
    BEDIT, FILE_BROWSER_DIR,
    GInitiallyUnowned
)

GType bedit_file_browser_dir_get_type(void) G_GNUC_CONST;

typedef struct _BeditFileBrowserDirIter BeditFileBrowserDirIter;
struct _BeditFileBrowserDirIter {
    GHashTableIter iter;
    GFile *file;
    GFileInfo *info;
};

BeditFileBrowserDir *bedit_file_browser_dir_new(GFile *file);

GFile *bedit_file_browser_dir_get_file(BeditFileBrowserDir *dir);

gboolean bedit_file_browser_dir_is_loading(BeditFileBrowserDir *dir);

void bedit_file_browser_dir_iter_init(
    BeditFileBrowserDirIter *iter, BeditFileBrowserDir *dir
);
gboolean bedit_file_browser_dir_iter_next(BeditFileBrowserDirIter *iter);
GFile *bedit_file_browser_dir_iter_get_file(BeditFileBrowserDirIter *iter);
GFileInfo *bedit_file_browser_dir_iter_get_info(BeditFileBrowserDirIter *iter);

void bedit_file_browser_dir_refresh(BeditFileBrowserDir *dir);

void _bedit_file_browser_dir_register_type(GTypeModule *type_module);

G_END_DECLS

#endif /* BEDIT_FILE_BROWSER_DIR_H */

