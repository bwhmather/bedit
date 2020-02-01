/*
 * bedit-history-entry.h
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * Based on gedit-history-entry.h from Gedit.
 *
 * Copyright (C) 2006 - Paolo Maggi
 * Copyright (C) 2006-2015 - Paolo Borelli
 * Copyright (C) 2010 - Garrett Regier, Jesse van den Kieboom, Steve Frécinaux
 * Copyright (C) 2011 - Ignacio Casal Quinteiro
 * Copyright (C) 2013-2016 - Sébastien Wilmet
 * Copyright (C) 2014 - Robert Roth
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

#ifndef BEDIT_HISTORY_ENTRY_H
#define BEDIT_HISTORY_ENTRY_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define BEDIT_TYPE_HISTORY_ENTRY (bedit_history_entry_get_type())

G_DECLARE_FINAL_TYPE(
    BeditHistoryEntry, bedit_history_entry, BEDIT, HISTORY_ENTRY,
    GtkComboBoxText)

GtkWidget *bedit_history_entry_new(
    const gchar *history_id, gboolean enable_completion);

void bedit_history_entry_prepend_text(
    BeditHistoryEntry *entry, const gchar *text);

void bedit_history_entry_append_text(
    BeditHistoryEntry *entry, const gchar *text);

void bedit_history_entry_clear(BeditHistoryEntry *entry);

void bedit_history_entry_set_history_length(
    BeditHistoryEntry *entry, guint max_saved);

guint bedit_history_entry_get_history_length(BeditHistoryEntry *gentry);

void bedit_history_entry_set_enable_completion(
    BeditHistoryEntry *entry, gboolean enable);

gboolean bedit_history_entry_get_enable_completion(BeditHistoryEntry *entry);

GtkWidget *bedit_history_entry_get_entry(BeditHistoryEntry *entry);

G_END_DECLS

#endif /* BEDIT_HISTORY_ENTRY_H */

