/*
 * bedit-file-browser-filter-match.c
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

#include "config.h"

#include "bedit-file-browser-filter-match.h"

#include <gmodule.h>

static void bedit_file_browser_filter_append_escaped(
    GString *str, gunichar chr
) {
    // Stolen from `g_markup_escape_text`, with some minor reformatting.
    switch (chr) {
    case '&':
        g_string_append(str, "&amp;");
        break;

    case '<':
        g_string_append(str, "&lt;");
        break;

    case '>':
        g_string_append(str, "&gt;");
        break;

    case '\'':
        g_string_append(str, "&apos;");
        break;

    case '"':
        g_string_append(str, "&quot;");
        break;

    default:
        if (
            (0x1 <= chr && chr <= 0x8) ||
            (0xb <= chr && chr <= 0xc) ||
            (0xe <= chr && chr <= 0x1f) ||
            (0x7f <= chr && chr <= 0x84) ||
            (0x86 <= chr && chr <= 0x9f)
        ) {
            g_string_append_printf(str, "&#x%x;", chr);
        } else {
            g_string_append_unichar(str, chr);
        }
        break;
    }
}

gboolean bedit_file_browser_filter_match_segment(
    gchar const *segment, gchar const *name, GString *markup, guint64 *quality
) {
    gboolean bold = FALSE;
    gchar const *query_cursor;
    gchar const *name_cursor;
    int quality_cursor = 0;
    gchar const *chunk_start;

    g_string_truncate(markup, 0);
    *quality = 0l;

    query_cursor = segment;
    name_cursor = name;
    chunk_start = name;

    while (*chunk_start != '\0') {
        gchar const *chunk_end;
        gchar const *candidate_start;
        gchar const *best_start;
        size_t best_length;

        // Find end of chunk.
        chunk_end = g_utf8_find_next_char(chunk_start, NULL);
        while (TRUE) {
            gunichar start_char = g_utf8_get_char(chunk_start);
            gunichar end_char = g_utf8_get_char(chunk_end);

            if (end_char == '\0') {
                break;
            }

            if (g_unichar_isalpha(start_char)) {
                // Chunk is made up of an upper or lower case letter followed by
                // any number of lower case letters.
                if (!g_unichar_islower(end_char)) {
                    break;
                }
            } else if (g_unichar_isdigit(start_char)) {
                // Chunk is a sequence of digits
                if (!g_unichar_isdigit(end_char)) {
                    break;
                }
            } else {
                // Chunk is a single, non-alphanumeric character.
                break;
            }

            chunk_end = g_utf8_find_next_char(chunk_end, NULL);
        }

        // Find longest match.
        best_start = chunk_start;
        best_length = 0;

        candidate_start = chunk_start;
        while (candidate_start < chunk_end) {
            gchar const *candidate_name_cursor;
            gchar const *candidate_query_cursor;
            gint candidate_length;

            candidate_name_cursor = candidate_start;
            candidate_query_cursor = query_cursor;
            candidate_length = 0;

            while (candidate_start + candidate_length <= chunk_end) {
                gunichar name_char = g_utf8_get_char(candidate_name_cursor);
                gunichar query_char = g_utf8_get_char(candidate_query_cursor);

                if (query_char == '\0') {
                    break;
                }

                if (
                    g_unichar_tolower(name_char) !=
                    g_unichar_tolower(query_char)
                ) {
                    break;
                }

                candidate_name_cursor = g_utf8_find_next_char(
                    candidate_name_cursor, NULL
                );
                candidate_query_cursor = g_utf8_find_next_char(
                    candidate_query_cursor, NULL
                );
                candidate_length++;
            }

            if (candidate_length > best_length) {
                best_start = candidate_start;
                best_length = candidate_length;
            }

            candidate_start = g_utf8_find_next_char(candidate_start, NULL);
        }

        while (name_cursor < chunk_end) {
            if (
                name_cursor >= best_start &&
                name_cursor < (best_start + best_length)
            ) {
                if (!bold) {
                    g_string_append(markup, "<b>");
                    bold = TRUE;
                }
                *quality = (*quality) | (1l << MAX(63 - quality_cursor, 0));
            } else {
                if (bold) {
                    g_string_append(markup, "</b>");
                    bold = FALSE;
                }
            }

            bedit_file_browser_filter_append_escaped(
                markup, g_utf8_get_char(name_cursor)
            );

            name_cursor = g_utf8_find_next_char(name_cursor, NULL);
            quality_cursor ++;
        }

        query_cursor += best_length;
        chunk_start = chunk_end;
    }

    if (bold) {
        g_string_append(markup, "</b>");
    }

    if (g_utf8_get_char(query_cursor) == '\0') {
        return TRUE;
    } else {
        *quality = 0;
        return FALSE;
    }
}

