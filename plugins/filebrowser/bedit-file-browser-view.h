/*
 * bedit-file-browser-view.h - Bedit plugin providing easy file access
 * from the sidepanel
 *
 * Copyright (C) 2006 - Jesse van den Kieboom <jesse@icecrew.nl>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef BEDIT_FILE_BROWSER_VIEW_H
#define BEDIT_FILE_BROWSER_VIEW_H

#include <gtk/gtk.h>

G_BEGIN_DECLS
#define BEDIT_TYPE_FILE_BROWSER_VIEW (bedit_file_browser_view_get_type())
#define BEDIT_FILE_BROWSER_VIEW(obj)                                           \
    (G_TYPE_CHECK_INSTANCE_CAST(                                               \
        (obj), BEDIT_TYPE_FILE_BROWSER_VIEW, BeditFileBrowserView))
#define BEDIT_FILE_BROWSER_VIEW_CONST(obj)                                     \
    (G_TYPE_CHECK_INSTANCE_CAST(                                               \
        (obj), BEDIT_TYPE_FILE_BROWSER_VIEW, BeditFileBrowserView const))
#define BEDIT_FILE_BROWSER_VIEW_CLASS(klass)                                   \
    (G_TYPE_CHECK_CLASS_CAST(                                                  \
        (klass), BEDIT_TYPE_FILE_BROWSER_VIEW, BeditFileBrowserViewClass))
#define BEDIT_IS_FILE_BROWSER_VIEW(obj)                                        \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), BEDIT_TYPE_FILE_BROWSER_VIEW))
#define BEDIT_IS_FILE_BROWSER_VIEW_CLASS(klass)                                \
    (G_TYPE_CHECK_CLASS_TYPE((klass), BEDIT_TYPE_FILE_BROWSER_VIEW))
#define BEDIT_FILE_BROWSER_VIEW_GET_CLASS(obj)                                 \
    (G_TYPE_INSTANCE_GET_CLASS(                                                \
        (obj), BEDIT_TYPE_FILE_BROWSER_VIEW, BeditFileBrowserViewClass))

typedef struct _BeditFileBrowserView BeditFileBrowserView;
typedef struct _BeditFileBrowserViewClass BeditFileBrowserViewClass;
typedef struct _BeditFileBrowserViewPrivate BeditFileBrowserViewPrivate;

typedef enum {
    BEDIT_FILE_BROWSER_VIEW_CLICK_POLICY_SINGLE,
    BEDIT_FILE_BROWSER_VIEW_CLICK_POLICY_DOUBLE
} BeditFileBrowserViewClickPolicy;

struct _BeditFileBrowserView {
    GtkTreeView parent;

    BeditFileBrowserViewPrivate *priv;
};

struct _BeditFileBrowserViewClass {
    GtkTreeViewClass parent_class;

    /* Signals */
    void (*error)(
        BeditFileBrowserView *filetree, guint code, gchar const *message);
    void (*file_activated)(BeditFileBrowserView *filetree, GtkTreeIter *iter);
    void (*directory_activated)(
        BeditFileBrowserView *filetree, GtkTreeIter *iter);
    void (*bookmark_activated)(
        BeditFileBrowserView *filetree, GtkTreeIter *iter);
};

GType bedit_file_browser_view_get_type(void) G_GNUC_CONST;

GtkWidget *bedit_file_browser_view_new(void);
void bedit_file_browser_view_set_model(
    BeditFileBrowserView *tree_view, GtkTreeModel *model);
void bedit_file_browser_view_start_rename(
    BeditFileBrowserView *tree_view, GtkTreeIter *iter);
void bedit_file_browser_view_set_click_policy(
    BeditFileBrowserView *tree_view, BeditFileBrowserViewClickPolicy policy);
void bedit_file_browser_view_set_restore_expand_state(
    BeditFileBrowserView *tree_view, gboolean restore_expand_state);

void _bedit_file_browser_view_register_type(GTypeModule *type_module);

G_END_DECLS

#endif /* BEDIT_FILE_BROWSER_VIEW_H */

