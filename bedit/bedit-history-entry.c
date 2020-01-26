/*
 * bedit-history-entry.c
 * This file is part of bedit
 *
 * Copyright (C) 2006 - Paolo Borelli
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

#include "bedit-history-entry.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>

#define MIN_ITEM_LEN 3

#define GEDIT_HISTORY_ENTRY_HISTORY_LENGTH_DEFAULT 10

struct _BeditHistoryEntry {
    GtkComboBoxText parent_instance;

    gchar *history_id;
    guint history_length;

    GtkEntryCompletion *completion;

    GSettings *settings;
};

enum {
    PROP_0,
    PROP_HISTORY_ID,
    PROP_HISTORY_LENGTH,
    PROP_ENABLE_COMPLETION,
    LAST_PROP
};

static GParamSpec *properties[LAST_PROP];

G_DEFINE_TYPE(BeditHistoryEntry, bedit_history_entry, GTK_TYPE_COMBO_BOX_TEXT)

static void bedit_history_entry_set_property(
    GObject *object, guint prop_id, const GValue *value, GParamSpec *spec) {
    BeditHistoryEntry *entry;

    g_return_if_fail(GEDIT_IS_HISTORY_ENTRY(object));

    entry = GEDIT_HISTORY_ENTRY(object);

    switch (prop_id) {
    case PROP_HISTORY_ID:
        entry->history_id = g_value_dup_string(value);
        break;
    case PROP_HISTORY_LENGTH:
        bedit_history_entry_set_history_length(entry, g_value_get_uint(value));
        break;
    case PROP_ENABLE_COMPLETION:
        bedit_history_entry_set_enable_completion(
            entry, g_value_get_boolean(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, spec);
    }
}

static void bedit_history_entry_get_property(
    GObject *object, guint prop_id, GValue *value, GParamSpec *spec) {
    BeditHistoryEntry *entry;

    g_return_if_fail(GEDIT_IS_HISTORY_ENTRY(object));

    entry = GEDIT_HISTORY_ENTRY(object);

    switch (prop_id) {
    case PROP_HISTORY_ID:
        g_value_set_string(value, entry->history_id);
        break;
    case PROP_HISTORY_LENGTH:
        g_value_set_uint(value, entry->history_length);
        break;
    case PROP_ENABLE_COMPLETION:
        g_value_set_boolean(
            value,
            bedit_history_entry_get_enable_completion(
                GEDIT_HISTORY_ENTRY(object)));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, spec);
    }
}

static void bedit_history_entry_dispose(GObject *object) {
    BeditHistoryEntry *entry = GEDIT_HISTORY_ENTRY(object);

    bedit_history_entry_set_enable_completion(entry, FALSE);

    g_clear_object(&entry->settings);

    G_OBJECT_CLASS(bedit_history_entry_parent_class)->dispose(object);
}

static void bedit_history_entry_finalize(GObject *object) {
    BeditHistoryEntry *entry = GEDIT_HISTORY_ENTRY(object);

    g_free(entry->history_id);

    G_OBJECT_CLASS(bedit_history_entry_parent_class)->finalize(object);
}

static void bedit_history_entry_load_history(BeditHistoryEntry *entry) {
    gchar **items;
    gsize i;

    items = g_settings_get_strv(entry->settings, entry->history_id);
    i = 0;

    gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(entry));

    /* Now the default value is an empty string so we have to take care
       of it to not add the empty string in the search list */
    while (items[i] != NULL && *items[i] != '\0' && i < entry->history_length) {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(entry), items[i]);
        i++;
    }

    g_strfreev(items);
}

static void bedit_history_entry_class_init(BeditHistoryEntryClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->set_property = bedit_history_entry_set_property;
    object_class->get_property = bedit_history_entry_get_property;
    object_class->dispose = bedit_history_entry_dispose;
    object_class->finalize = bedit_history_entry_finalize;

    properties[PROP_HISTORY_ID] = g_param_spec_string(
        "history-id", "History ID", "History ID", NULL,
        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

    properties[PROP_HISTORY_LENGTH] = g_param_spec_uint(
        "history-length", "Max History Length", "Max History Length", 0,
        G_MAXUINT, GEDIT_HISTORY_ENTRY_HISTORY_LENGTH_DEFAULT,
        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_ENABLE_COMPLETION] = g_param_spec_boolean(
        "enable-completion", "Enable Completion",
        "Wether the completion is enabled", TRUE,
        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties(object_class, LAST_PROP, properties);
}

static GtkListStore *get_history_store(BeditHistoryEntry *entry) {
    GtkTreeModel *store;

    store = gtk_combo_box_get_model(GTK_COMBO_BOX(entry));
    g_return_val_if_fail(GTK_IS_LIST_STORE(store), NULL);

    return (GtkListStore *)store;
}

static gchar **get_history_items(BeditHistoryEntry *entry) {
    GtkListStore *store;
    GtkTreeIter iter;
    GPtrArray *array;
    gboolean valid;
    gint n_children;
    gint text_column;

    store = get_history_store(entry);
    text_column = gtk_combo_box_get_entry_text_column(GTK_COMBO_BOX(entry));

    valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter);
    n_children = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(store), NULL);

    array = g_ptr_array_sized_new(n_children + 1);

    while (valid) {
        gchar *str;

        gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, text_column, &str, -1);

        g_ptr_array_add(array, str);

        valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter);
    }

    g_ptr_array_add(array, NULL);

    return (gchar **)g_ptr_array_free(array, FALSE);
}

static void bedit_history_entry_save_history(BeditHistoryEntry *entry) {
    gchar **items;

    g_return_if_fail(GEDIT_IS_HISTORY_ENTRY(entry));

    items = get_history_items(entry);

    g_settings_set_strv(
        entry->settings, entry->history_id, (const gchar *const *)items);

    g_strfreev(items);
}

static gboolean remove_item(BeditHistoryEntry *entry, const gchar *text) {
    GtkListStore *store;
    GtkTreeIter iter;
    gint text_column;

    g_return_val_if_fail(text != NULL, FALSE);

    store = get_history_store(entry);
    text_column = gtk_combo_box_get_entry_text_column(GTK_COMBO_BOX(entry));

    if (!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter))
        return FALSE;

    do {
        gchar *item_text;

        gtk_tree_model_get(
            GTK_TREE_MODEL(store), &iter, text_column, &item_text, -1);

        if (item_text != NULL && strcmp(item_text, text) == 0) {
            gtk_list_store_remove(store, &iter);
            g_free(item_text);
            return TRUE;
        }

        g_free(item_text);

    } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter));

    return FALSE;
}

static void clamp_list_store(GtkListStore *store, guint max) {
    GtkTreePath *path;
    GtkTreeIter iter;

    /* -1 because TreePath counts from 0 */
    path = gtk_tree_path_new_from_indices(max - 1, -1);

    if (gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &iter, path)) {
        while (TRUE) {
            if (!gtk_list_store_remove(store, &iter))
                break;
        }
    }

    gtk_tree_path_free(path);
}

static void insert_history_item(
    BeditHistoryEntry *entry, const gchar *text, gboolean prepend) {
    GtkListStore *store;

    if (g_utf8_strlen(text, -1) <= MIN_ITEM_LEN)
        return;

    store = get_history_store(entry);

    /* remove the text from the store if it was already
     * present. If it wasn't, clamp to max history - 1
     * before inserting the new row, otherwise appending
     * would not work */

    if (!remove_item(entry, text)) {
        clamp_list_store(store, entry->history_length - 1);
    }

    if (prepend) {
        gtk_combo_box_text_prepend_text(GTK_COMBO_BOX_TEXT(entry), text);
    } else {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(entry), text);
    }

    bedit_history_entry_save_history(entry);
}

void bedit_history_entry_prepend_text(
    BeditHistoryEntry *entry, const gchar *text) {
    g_return_if_fail(GEDIT_IS_HISTORY_ENTRY(entry));
    g_return_if_fail(text != NULL);

    insert_history_item(entry, text, TRUE);
}

void bedit_history_entry_append_text(
    BeditHistoryEntry *entry, const gchar *text) {
    g_return_if_fail(GEDIT_IS_HISTORY_ENTRY(entry));
    g_return_if_fail(text != NULL);

    insert_history_item(entry, text, FALSE);
}

void bedit_history_entry_clear(BeditHistoryEntry *entry) {
    g_return_if_fail(GEDIT_IS_HISTORY_ENTRY(entry));

    gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(entry));

    bedit_history_entry_save_history(entry);
}

static void bedit_history_entry_init(BeditHistoryEntry *entry) {
    entry->history_id = NULL;
    entry->history_length = GEDIT_HISTORY_ENTRY_HISTORY_LENGTH_DEFAULT;

    entry->completion = NULL;

    entry->settings = g_settings_new("com.bwhmather.bedit.state.history-entry");
}

void bedit_history_entry_set_history_length(
    BeditHistoryEntry *entry, guint history_length) {
    g_return_if_fail(GEDIT_IS_HISTORY_ENTRY(entry));
    g_return_if_fail(history_length > 0);

    entry->history_length = history_length;

    /* TODO: update if we currently have more items than max */
}

guint bedit_history_entry_get_history_length(BeditHistoryEntry *entry) {
    g_return_val_if_fail(GEDIT_IS_HISTORY_ENTRY(entry), 0);

    return entry->history_length;
}

void bedit_history_entry_set_enable_completion(
    BeditHistoryEntry *entry, gboolean enable) {
    g_return_if_fail(GEDIT_IS_HISTORY_ENTRY(entry));

    if (enable) {
        if (entry->completion != NULL) {
            return;
        }

        entry->completion = gtk_entry_completion_new();
        gtk_entry_completion_set_model(
            entry->completion, GTK_TREE_MODEL(get_history_store(entry)));

        /* Use model column 0 as the text column */
        gtk_entry_completion_set_text_column(entry->completion, 0);

        gtk_entry_completion_set_minimum_key_length(
            entry->completion, MIN_ITEM_LEN);

        gtk_entry_completion_set_popup_completion(entry->completion, FALSE);
        gtk_entry_completion_set_inline_completion(entry->completion, TRUE);

        /* Assign the completion to the entry */
        gtk_entry_set_completion(
            GTK_ENTRY(bedit_history_entry_get_entry(entry)), entry->completion);
    } else {
        if (entry->completion == NULL) {
            return;
        }

        gtk_entry_set_completion(
            GTK_ENTRY(bedit_history_entry_get_entry(entry)), NULL);
        g_clear_object(&entry->completion);
    }
}

gboolean bedit_history_entry_get_enable_completion(BeditHistoryEntry *entry) {
    g_return_val_if_fail(GEDIT_IS_HISTORY_ENTRY(entry), FALSE);

    return entry->completion != NULL;
}

GtkWidget *bedit_history_entry_new(
    const gchar *history_id, gboolean enable_completion) {
    BeditHistoryEntry *entry;

    g_return_val_if_fail(history_id != NULL, NULL);

    enable_completion = (enable_completion != FALSE);

    entry = g_object_new(
        GEDIT_TYPE_HISTORY_ENTRY, "has-entry", TRUE, "entry-text-column", 0,
        "id-column", 1, "history-id", history_id, "enable-completion",
        enable_completion, NULL);

    /* We must load the history after the object has been constructed,
     * to ensure that the model is set properly.
     */
    bedit_history_entry_load_history(entry);

    return GTK_WIDGET(entry);
}

/*
 * Utility function to get the editable text entry internal widget.
 * I would prefer to not expose this implementation detail and
 * simply make the BeditHistoryEntry widget implement the
 * GtkEditable interface. Unfortunately both GtkEditable and
 * GtkComboBox have a "changed" signal and I am not sure how to
 * handle the conflict.
 */
GtkWidget *bedit_history_entry_get_entry(BeditHistoryEntry *entry) {
    g_return_val_if_fail(GEDIT_IS_HISTORY_ENTRY(entry), NULL);

    return gtk_bin_get_child(GTK_BIN(entry));
}

