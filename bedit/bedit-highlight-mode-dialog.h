/*
 * bedit-highlight-mode-dialog.h
 * This file is part of bedit
 *
 * Copyright (C) 2013 - Ignacio Casal Quinteiro
 *
 * bedit is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * bedit is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with bedit. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GEDIT_HIGHLIGHT_MODE_DIALOG_H
#define GEDIT_HIGHLIGHT_MODE_DIALOG_H

#include <glib.h>
#include "bedit-highlight-mode-selector.h"

G_BEGIN_DECLS

#define GEDIT_TYPE_HIGHLIGHT_MODE_DIALOG                                       \
    (bedit_highlight_mode_dialog_get_type())

G_DECLARE_FINAL_TYPE(
    BeditHighlightModeDialog, bedit_highlight_mode_dialog, GEDIT,
    HIGHLIGHT_MODE_DIALOG, GtkDialog)

GtkWidget *bedit_highlight_mode_dialog_new(GtkWindow *parent);

BeditHighlightModeSelector *bedit_highlight_mode_dialog_get_selector(
    BeditHighlightModeDialog *dlg);

G_END_DECLS

#endif /* GEDIT_HIGHLIGHT_MODE_DIALOG_H */

/* ex:set ts=8 noet: */
