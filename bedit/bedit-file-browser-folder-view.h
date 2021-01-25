/*
 * bedit-file-browser-view.h
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * Based on gedit-file-browser-view.h from Gedit.
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

#ifndef BEDIT_FILE_BROWSER_FOLDER_VIEW_H
#define BEDIT_FILE_BROWSER_FOLDER_VIEW_H

#include <gtk/gtk.h>

G_BEGIN_DECLS
#define BEDIT_TYPE_FILE_BROWSER_FOLDER_VIEW                                 \
    (bedit_file_browser_folder_view_get_type())
#define BEDIT_FILE_BROWSER_FOLDER_VIEW(obj)                                 \
    (G_TYPE_CHECK_INSTANCE_CAST(                                            \
        (obj), BEDIT_TYPE_FILE_BROWSER_FOLDER_VIEW,                         \
        BeditFileBrowserFolderView                                          \
    ))
#define BEDIT_FILE_BROWSER_FOLDER_VIEW_CONST(obj)                           \
    (G_TYPE_CHECK_INSTANCE_CAST(                                            \
        (obj), BEDIT_TYPE_FILE_BROWSER_FOLDER_VIEW,                         \
        BeditFileBrowserFolderView const                                    \
    ))
#define BEDIT_FILE_BROWSER_FOLDER_VIEW_CLASS(klass)                         \
    (G_TYPE_CHECK_CLASS_CAST(                                               \
        (klass), BEDIT_TYPE_FILE_BROWSER_FOLDER_VIEW,                       \
        BeditFileBrowserFolderViewClass                                     \
    ))
#define BEDIT_IS_FILE_BROWSER_FOLDER_VIEW(obj)                              \
    (G_TYPE_CHECK_INSTANCE_TYPE(                                            \
        (obj), BEDIT_TYPE_FILE_BROWSER_FOLDER_VIEW                          \
    ))
#define BEDIT_IS_FILE_BROWSER_FOLDER_VIEW_CLASS(klass)                      \
    (G_TYPE_CHECK_CLASS_TYPE(                                               \
        (klass), BEDIT_TYPE_FILE_BROWSER_FOLDER_VIEW                        \
    ))
#define BEDIT_FILE_BROWSER_FOLDER_VIEW_GET_CLASS(obj)                       \
    (G_TYPE_INSTANCE_GET_CLASS(                                             \
        (obj), BEDIT_TYPE_FILE_BROWSER_FOLDER_VIEW,                         \
        BeditFileBrowserFolderViewClass                                     \
    ))

typedef struct _BeditFileBrowserFolderView
    BeditFileBrowserFolderView;
typedef struct _BeditFileBrowserFolderViewClass
    BeditFileBrowserFolderViewClass;
typedef struct _BeditFileBrowserFolderViewPrivate
    BeditFileBrowserFolderViewPrivate;

typedef enum {
    BEDIT_FILE_BROWSER_FOLDER_VIEW_CLICK_POLICY_SINGLE,
    BEDIT_FILE_BROWSER_FOLDER_VIEW_CLICK_POLICY_DOUBLE
} BeditFileBrowserFolderViewClickPolicy;

struct _BeditFileBrowserFolderView {
    GtkTreeView parent;

    BeditFileBrowserFolderViewPrivate *priv;
};

struct _BeditFileBrowserFolderViewClass {
    GtkTreeViewClass parent_class;

    /* Signals */
    void (*error)(
        BeditFileBrowserFolderView *filetree, guint code, gchar const *message
    );
    void (*file_activated)(
        BeditFileBrowserFolderView *filetree, GtkTreeIter *iter
    );
    void (*directory_activated)(
        BeditFileBrowserFolderView *filetree, GtkTreeIter *iter
    );
    void (*bookmark_activated)(
        BeditFileBrowserFolderView *filetree, GtkTreeIter *iter
    );
};

GType bedit_file_browser_folder_view_get_type(void) G_GNUC_CONST;

GtkWidget *bedit_file_browser_folder_view_new(void);
void bedit_file_browser_folder_view_set_model(
    BeditFileBrowserFolderView *folder_view, GtkTreeModel *model
);
void bedit_file_browser_folder_view_start_rename(
    BeditFileBrowserFolderView *folder_view, GtkTreeIter *iter
);
void bedit_file_browser_folder_view_set_click_policy(
    BeditFileBrowserFolderView *folder_view,
    BeditFileBrowserFolderViewClickPolicy policy
);
void bedit_file_browser_folder_view_set_restore_expand_state(
    BeditFileBrowserFolderView *folder_view, gboolean restore_expand_state
);

G_END_DECLS

#endif /* BEDIT_FILE_BROWSER_FOLDER_VIEW_H */

