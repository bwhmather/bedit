/*
 * bedit-debug.c
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

#include "bedit-debug.h"
#include <stdio.h>

#define ENABLE_PROFILING

#ifdef ENABLE_PROFILING
static GTimer *timer = NULL;
static gdouble last_time = 0.0;
#endif

static BeditDebugSection enabled_sections = BEDIT_NO_DEBUG;

#define DEBUG_IS_ENABLED(section) (enabled_sections & (section))

/**
 * bedit_debug_init:
 *
 * Initializes the debugging subsystem of bedit.
 *
 * The function checks for the existence of certain environment variables to
 * determine whether to enable output for a debug section. To enable output
 * for a specific debug section, set an environment variable of the same name;
 * e.g. to enable output for the %BEDIT_DEBUG_PLUGINS section, set a
 * <code>BEDIT_DEBUG_PLUGINS</code> environment variable. To enable output
 * for all debug sections, set the <code>BEDIT_DEBUG</code> environment
 * variable.
 *
 * This function must be called before any of the other debug functions are
 * called. It must only be called once.
 */
void bedit_debug_init(void) {
    if (g_getenv("BEDIT_DEBUG") != NULL) {
        /* enable all debugging */
        enabled_sections = ~BEDIT_NO_DEBUG;
        goto out;
    }

    if (g_getenv("BEDIT_DEBUG_VIEW") != NULL) {
        enabled_sections |= BEDIT_DEBUG_VIEW;
    }
    if (g_getenv("BEDIT_DEBUG_PREFS") != NULL) {
        enabled_sections |= BEDIT_DEBUG_PREFS;
    }
    if (g_getenv("BEDIT_DEBUG_WINDOW") != NULL) {
        enabled_sections |= BEDIT_DEBUG_WINDOW;
    }
    if (g_getenv("BEDIT_DEBUG_PLUGINS") != NULL) {
        enabled_sections |= BEDIT_DEBUG_PLUGINS;
    }
    if (g_getenv("BEDIT_DEBUG_TAB") != NULL) {
        enabled_sections |= BEDIT_DEBUG_TAB;
    }
    if (g_getenv("BEDIT_DEBUG_DOCUMENT") != NULL) {
        enabled_sections |= BEDIT_DEBUG_DOCUMENT;
    }
    if (g_getenv("BEDIT_DEBUG_COMMANDS") != NULL) {
        enabled_sections |= BEDIT_DEBUG_COMMANDS;
    }
    if (g_getenv("BEDIT_DEBUG_APP") != NULL) {
        enabled_sections |= BEDIT_DEBUG_APP;
    }
    if (g_getenv("BEDIT_DEBUG_UTILS") != NULL) {
        enabled_sections |= BEDIT_DEBUG_UTILS;
    }
    if (g_getenv("BEDIT_DEBUG_METADATA") != NULL) {
        enabled_sections |= BEDIT_DEBUG_METADATA;
    }

out:

#ifdef ENABLE_PROFILING
    if (enabled_sections != BEDIT_NO_DEBUG) {
        timer = g_timer_new();
    }
#endif
}

/**
 * bedit_debug:
 * @section: debug section.
 * @file: file name.
 * @line: line number.
 * @function: name of the function that is calling bedit_debug().
 *
 * If @section is enabled, then logs the trace information @file, @line, and
 * @function.
 */
void bedit_debug(
    BeditDebugSection section, const gchar *file, gint line,
    const gchar *function) {
    bedit_debug_message(section, file, line, function, "%s", "");
}

/**
 * bedit_debug_message:
 * @section: debug section.
 * @file: file name.
 * @line: line number.
 * @function: name of the function that is calling bedit_debug_message().
 * @format: A g_vprintf() format string.
 * @...: The format string arguments.
 *
 * If @section is enabled, then logs the trace information @file, @line, and
 * @function along with the message obtained by formatting @format with the
 * given format string arguments.
 */
void bedit_debug_message(
    BeditDebugSection section, const gchar *file, gint line,
    const gchar *function, const gchar *format, ...) {
    if (G_UNLIKELY(DEBUG_IS_ENABLED(section))) {
        va_list args;
        gchar *msg;

#ifdef ENABLE_PROFILING
        gdouble seconds;

        g_return_if_fail(timer != NULL);

        seconds = g_timer_elapsed(timer, NULL);
#endif

        g_return_if_fail(format != NULL);

        va_start(args, format);
        msg = g_strdup_vprintf(format, args);
        va_end(args);

#ifdef ENABLE_PROFILING
        g_print(
            "[%f (%f)] %s:%d (%s) %s\n", seconds, seconds - last_time, file,
            line, function, msg);

        last_time = seconds;
#else
        g_print("%s:%d (%s) %s\n", file, line, function, msg);
#endif

        fflush(stdout);

        g_free(msg);
    }
}

/**
 * bedit_debug_plugin_message:
 * @file: file name.
 * @line: line number.
 * @function: name of the function that is calling bedit_debug_plugin_message().
 * @message: a message.
 *
 * If the section %BEDIT_DEBUG_PLUGINS is enabled, then logs the trace
 * information @file, @line, and @function along with @message.
 *
 * This function may be overridden by GObject Introspection language bindings
 * to be more language-specific.
 *
 * <emphasis>Python</emphasis>
 *
 * A PyGObject override is provided that has the following signature:
 * <informalexample>
 *   <programlisting>
 *     def debug_plugin_message(format_str, *format_args):
 *         #...
 *   </programlisting>
 * </informalexample>
 *
 * It automatically supplies parameters @file, @line, and @function, and it
 * formats <code>format_str</code> with the given format arguments. The syntax
 * of the format string is the usual Python string formatting syntax described
 * by <ulink
 * url="http://docs.python.org/library/stdtypes.html#string-formatting">5.6.2.
 * String Formatting Operations</ulink>.
 *
 * Since: 3.4
 */
void bedit_debug_plugin_message(
    const gchar *file, gint line, const gchar *function, const gchar *message) {
    bedit_debug_message(
        BEDIT_DEBUG_PLUGINS, file, line, function, "%s", message);
}

