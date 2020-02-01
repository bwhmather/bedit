/*
 * bedit-debug.h
 * This file is part of bedit
 *
 * Copyright (C) 1998, 1999 Alex Roberts, Evan Lawrence
 * Copyright (C) 2000, 2001 Chema Celorio, Paolo Maggi
 * Copyright (C) 2002 - 2005 Paolo Maggi
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

#ifndef BEDIT_DEBUG_H
#define BEDIT_DEBUG_H

#include <glib.h>

/**
 * BeditDebugSection:
 *
 * Enumeration of debug sections.
 *
 * Debugging output for a section is enabled by setting an environment variable
 * of the same name. For example, setting the <code>BEDIT_DEBUG_PLUGINS</code>
 * environment variable enables all debugging output for the
 * %BEDIT_DEBUG_PLUGINS section. Setting the special environment variable
 * <code>BEDIT_DEBUG</code> enables output for all sections.
 */
typedef enum {
    BEDIT_NO_DEBUG = 0,
    BEDIT_DEBUG_VIEW = 1 << 0,
    BEDIT_DEBUG_PREFS = 1 << 1,
    BEDIT_DEBUG_WINDOW = 1 << 2,
    BEDIT_DEBUG_PLUGINS = 1 << 3,
    BEDIT_DEBUG_TAB = 1 << 4,
    BEDIT_DEBUG_DOCUMENT = 1 << 5,
    BEDIT_DEBUG_COMMANDS = 1 << 6,
    BEDIT_DEBUG_APP = 1 << 7,
    BEDIT_DEBUG_UTILS = 1 << 8,
    BEDIT_DEBUG_METADATA = 1 << 9,
} BeditDebugSection;

#define DEBUG_VIEW BEDIT_DEBUG_VIEW, __FILE__, __LINE__, G_STRFUNC
#define DEBUG_PREFS BEDIT_DEBUG_PREFS, __FILE__, __LINE__, G_STRFUNC
#define DEBUG_WINDOW BEDIT_DEBUG_WINDOW, __FILE__, __LINE__, G_STRFUNC
#define DEBUG_PLUGINS BEDIT_DEBUG_PLUGINS, __FILE__, __LINE__, G_STRFUNC
#define DEBUG_TAB BEDIT_DEBUG_TAB, __FILE__, __LINE__, G_STRFUNC
#define DEBUG_DOCUMENT BEDIT_DEBUG_DOCUMENT, __FILE__, __LINE__, G_STRFUNC
#define DEBUG_COMMANDS BEDIT_DEBUG_COMMANDS, __FILE__, __LINE__, G_STRFUNC
#define DEBUG_APP BEDIT_DEBUG_APP, __FILE__, __LINE__, G_STRFUNC
#define DEBUG_UTILS BEDIT_DEBUG_UTILS, __FILE__, __LINE__, G_STRFUNC
#define DEBUG_METADATA BEDIT_DEBUG_METADATA, __FILE__, __LINE__, G_STRFUNC

void bedit_debug_init(void);

void bedit_debug(
    BeditDebugSection section, const gchar *file, gint line,
    const gchar *function);

void bedit_debug_message(
    BeditDebugSection section, const gchar *file, gint line,
    const gchar *function, const gchar *format, ...) G_GNUC_PRINTF(5, 6);

void bedit_debug_plugin_message(
    const gchar *file, gint line, const gchar *function, const gchar *message);

#endif /* BEDIT_DEBUG_H */
