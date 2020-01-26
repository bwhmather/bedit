/*
 * bedit-highlight-mode-selector.h
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

#ifndef GEDIT_HIGHLIGHT_MODE_SELECTOR_H
#define GEDIT_HIGHLIGHT_MODE_SELECTOR_H

#include <glib-object.h>
#include <gtksourceview/gtksource.h>
#include "bedit-window.h"

G_BEGIN_DECLS

#define GEDIT_TYPE_HIGHLIGHT_MODE_SELECTOR                                     \
    (bedit_highlight_mode_selector_get_type())

G_DECLARE_FINAL_TYPE(
    BeditHighlightModeSelector, bedit_highlight_mode_selector, GEDIT,
    HIGHLIGHT_MODE_SELECTOR, GtkGrid)

BeditHighlightModeSelector *bedit_highlight_mode_selector_new(void);

void bedit_highlight_mode_selector_select_language(
    BeditHighlightModeSelector *selector, GtkSourceLanguage *language);

void bedit_highlight_mode_selector_activate_selected_language(
    BeditHighlightModeSelector *selector);

G_END_DECLS

#endif /* GEDIT_HIGHLIGHT_MODE_SELECTOR_H */

/* ex:set ts=8 noet: */
