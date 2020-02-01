/*
 * bedit-settings.h
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * Based on gedit-settings.h from Gedit.
 *
 * Copyright (C) 2002 - Paolo Maggi
 * Copyright (C) 2009-2015 - Ignacio Casal Quinteiro
 * Copyright (C) 2010 - Garrett Regier, Steve Frécinaux
 * Copyright (C) 2013-2019 - Sébastien Wilmet
 * Copyright (C) 2014 - Sebastien Lafargue
 * Copyright (C) 2014-2015 - Paolo Borelli
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

#ifndef BEDIT_SETTINGS_H
#define BEDIT_SETTINGS_H

#include <glib-object.h>
#include <glib.h>
#include "bedit-app.h"

G_BEGIN_DECLS

#define BEDIT_TYPE_SETTINGS (bedit_settings_get_type())

G_DECLARE_FINAL_TYPE(BeditSettings, bedit_settings, BEDIT, SETTINGS, GObject)

BeditSettings *bedit_settings_new(void);

BeditLockdownMask bedit_settings_get_lockdown(BeditSettings *gs);

gchar *bedit_settings_get_system_font(BeditSettings *gs);

GSList *bedit_settings_get_candidate_encodings(gboolean *default_candidates);

/* Utility functions */
GSList *bedit_settings_get_list(GSettings *settings, const gchar *key);

void bedit_settings_set_list(
    GSettings *settings, const gchar *key, const GSList *list);

/* key constants */
#define BEDIT_SETTINGS_USE_DEFAULT_FONT "use-default-font"
#define BEDIT_SETTINGS_EDITOR_FONT "editor-font"
#define BEDIT_SETTINGS_SCHEME "scheme"
#define BEDIT_SETTINGS_CREATE_BACKUP_COPY "create-backup-copy"
#define BEDIT_SETTINGS_AUTO_SAVE "auto-save"
#define BEDIT_SETTINGS_AUTO_SAVE_INTERVAL "auto-save-interval"
#define BEDIT_SETTINGS_MAX_UNDO_ACTIONS "max-undo-actions"
#define BEDIT_SETTINGS_WRAP_MODE "wrap-mode"
#define BEDIT_SETTINGS_WRAP_LAST_SPLIT_MODE "wrap-last-split-mode"
#define BEDIT_SETTINGS_TABS_SIZE "tabs-size"
#define BEDIT_SETTINGS_INSERT_SPACES "insert-spaces"
#define BEDIT_SETTINGS_AUTO_INDENT "auto-indent"
#define BEDIT_SETTINGS_DISPLAY_LINE_NUMBERS "display-line-numbers"
#define BEDIT_SETTINGS_HIGHLIGHT_CURRENT_LINE "highlight-current-line"
#define BEDIT_SETTINGS_BRACKET_MATCHING "bracket-matching"
#define BEDIT_SETTINGS_DISPLAY_RIGHT_MARGIN "display-right-margin"
#define BEDIT_SETTINGS_RIGHT_MARGIN_POSITION "right-margin-position"
#define BEDIT_SETTINGS_SMART_HOME_END "smart-home-end"
#define BEDIT_SETTINGS_RESTORE_CURSOR_POSITION "restore-cursor-position"
#define BEDIT_SETTINGS_SYNTAX_HIGHLIGHTING "syntax-highlighting"
#define BEDIT_SETTINGS_SEARCH_HIGHLIGHTING "search-highlighting"
#define BEDIT_SETTINGS_BACKGROUND_PATTERN "background-pattern"
#define BEDIT_SETTINGS_STATUSBAR_VISIBLE "statusbar-visible"
#define BEDIT_SETTINGS_PRINT_SYNTAX_HIGHLIGHTING "print-syntax-highlighting"
#define BEDIT_SETTINGS_PRINT_HEADER "print-header"
#define BEDIT_SETTINGS_PRINT_WRAP_MODE "print-wrap-mode"
#define BEDIT_SETTINGS_PRINT_LINE_NUMBERS "print-line-numbers"
#define BEDIT_SETTINGS_PRINT_FONT_BODY_PANGO "print-font-body-pango"
#define BEDIT_SETTINGS_PRINT_FONT_HEADER_PANGO "print-font-header-pango"
#define BEDIT_SETTINGS_PRINT_FONT_NUMBERS_PANGO "print-font-numbers-pango"
#define BEDIT_SETTINGS_PRINT_MARGIN_LEFT "margin-left"
#define BEDIT_SETTINGS_PRINT_MARGIN_TOP "margin-top"
#define BEDIT_SETTINGS_PRINT_MARGIN_RIGHT "margin-right"
#define BEDIT_SETTINGS_PRINT_MARGIN_BOTTOM "margin-bottom"
#define BEDIT_SETTINGS_CANDIDATE_ENCODINGS "candidate-encodings"
#define BEDIT_SETTINGS_ACTIVE_PLUGINS "active-plugins"
#define BEDIT_SETTINGS_ENSURE_TRAILING_NEWLINE "ensure-trailing-newline"

/* window state keys */
#define BEDIT_SETTINGS_WINDOW_STATE "state"
#define BEDIT_SETTINGS_WINDOW_SIZE "size"
#define BEDIT_SETTINGS_SHOW_TABS_MODE "show-tabs-mode"
#define BEDIT_SETTINGS_ACTIVE_FILE_FILTER "filter-id"

G_END_DECLS

#endif /* BEDIT_SETTINGS_H */

