/*
 * bedit-notebook-stack-switcher.h
 * This file is part of bedit
 *
 * Copyright (C) 2014 - Paolo Borelli
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

#ifndef GEDIT_NOTEBOOK_STACK_SWITCHER_H
#define GEDIT_NOTEBOOK_STACK_SWITCHER_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GEDIT_TYPE_NOTEBOOK_STACK_SWITCHER                                     \
    (bedit_notebook_stack_switcher_get_type())
#define GEDIT_NOTEBOOK_STACK_SWITCHER(obj)                                     \
    (G_TYPE_CHECK_INSTANCE_CAST(                                               \
        (obj), GEDIT_TYPE_NOTEBOOK_STACK_SWITCHER,                             \
        BeditNotebookStackSwitcher))
#define GEDIT_NOTEBOOK_STACK_SWITCHER_CLASS(klass)                             \
    (G_TYPE_CHECK_CLASS_CAST(                                                  \
        (klass), GEDIT_TYPE_NOTEBOOK_STACK_SWITCHER,                           \
        BeditNotebookStackSwitcherClass))
#define GEDIT_IS_NOTEBOOK_STACK_SWITCHER(obj)                                  \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GEDIT_TYPE_NOTEBOOK_STACK_SWITCHER))
#define GEDIT_IS_NOTEBOOK_STACK_SWITCHER_CLASS(klass)                          \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GEDIT_TYPE_NOTEBOOK_STACK_SWITCHER))
#define GEDIT_NOTEBOOK_STACK_SWITCHER_GET_CLASS(obj)                           \
    (G_TYPE_INSTANCE_GET_CLASS(                                                \
        (obj), GEDIT_TYPE_NOTEBOOK_STACK_SWITCHER,                             \
        BeditNotebookStackSwitcherClass))

typedef struct _BeditNotebookStackSwitcher BeditNotebookStackSwitcher;
typedef struct _BeditNotebookStackSwitcherClass BeditNotebookStackSwitcherClass;
typedef struct _BeditNotebookStackSwitcherPrivate
    BeditNotebookStackSwitcherPrivate;

struct _BeditNotebookStackSwitcher {
    GtkBin parent;

    /*< private >*/
    BeditNotebookStackSwitcherPrivate *priv;
};

struct _BeditNotebookStackSwitcherClass {
    GtkBinClass parent_class;

    /* Padding for future expansion */
    void (*_bedit_reserved1)(void);
    void (*_bedit_reserved2)(void);
    void (*_bedit_reserved3)(void);
    void (*_bedit_reserved4)(void);
};

GType bedit_notebook_stack_switcher_get_type(void) G_GNUC_CONST;

GtkWidget *bedit_notebook_stack_switcher_new(void);

void bedit_notebook_stack_switcher_set_stack(
    BeditNotebookStackSwitcher *switcher, GtkStack *stack);

GtkStack *bedit_notebook_stack_switcher_get_stack(
    BeditNotebookStackSwitcher *switcher);

G_END_DECLS

#endif /* GEDIT_NOTEBOOK_STACK_SWITCHER_H  */

