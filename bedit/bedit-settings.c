/*
 * bedit-settings.c
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * Based on gedit-settings.c from Gedit.
 * 
 * Copyright (C) 2002-2005 - Paolo Maggi
 * Copyright (C) 2009-2013 - Ignacio Casal Quinteiro
 * Copyright (C) 2010 - Florian Müllner, Jesse van den Kieboom, Steve Frécinaux,
 *   Tomas Bzatek
 * Copyright (C) 2010-2013 - Garrett Regier
 * Copyright (C) 2011-2015 - Paolo Borelli
 * Copyright (C) 2013-2019 - Sébastien Wilmet
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

#include "config.h"

#include "bedit-settings.h"

#include <gtksourceview/gtksource.h>
#include <string.h>

#include "bedit-app-private.h"
#include "bedit-app.h"
#include "bedit-view.h"
#include "bedit-window.h"

#define BEDIT_SETTINGS_LOCKDOWN_COMMAND_LINE "disable-command-line"
#define BEDIT_SETTINGS_LOCKDOWN_PRINTING "disable-printing"
#define BEDIT_SETTINGS_LOCKDOWN_PRINT_SETUP "disable-print-setup"
#define BEDIT_SETTINGS_LOCKDOWN_SAVE_TO_DISK "disable-save-to-disk"

#define BEDIT_SETTINGS_SYSTEM_FONT "monospace-font-name"

struct _BeditSettings {
    GObject parent_instance;

    GSettings *lockdown;
    GSettings *interface;
    GSettings *editor;
    GSettings *ui;

    gchar *old_scheme;
};

G_DEFINE_TYPE(BeditSettings, bedit_settings, G_TYPE_OBJECT)

static void bedit_settings_finalize(GObject *object) {
    BeditSettings *gs = BEDIT_SETTINGS(object);

    g_free(gs->old_scheme);

    G_OBJECT_CLASS(bedit_settings_parent_class)->finalize(object);
}

static void bedit_settings_dispose(GObject *object) {
    BeditSettings *gs = BEDIT_SETTINGS(object);

    g_clear_object(&gs->lockdown);
    g_clear_object(&gs->interface);
    g_clear_object(&gs->editor);
    g_clear_object(&gs->ui);

    G_OBJECT_CLASS(bedit_settings_parent_class)->dispose(object);
}

static void on_lockdown_changed(
    GSettings *settings, const gchar *key, gpointer useless) {
    gboolean locked;
    BeditApp *app;

    locked = g_settings_get_boolean(settings, key);
    app = BEDIT_APP(g_application_get_default());

    if (strcmp(key, BEDIT_SETTINGS_LOCKDOWN_COMMAND_LINE) == 0) {
        _bedit_app_set_lockdown_bit(app, BEDIT_LOCKDOWN_COMMAND_LINE, locked);
    } else if (strcmp(key, BEDIT_SETTINGS_LOCKDOWN_PRINTING) == 0) {
        _bedit_app_set_lockdown_bit(app, BEDIT_LOCKDOWN_PRINTING, locked);
    } else if (strcmp(key, BEDIT_SETTINGS_LOCKDOWN_PRINT_SETUP) == 0) {
        _bedit_app_set_lockdown_bit(app, BEDIT_LOCKDOWN_PRINT_SETUP, locked);
    } else if (strcmp(key, BEDIT_SETTINGS_LOCKDOWN_SAVE_TO_DISK) == 0) {
        _bedit_app_set_lockdown_bit(app, BEDIT_LOCKDOWN_SAVE_TO_DISK, locked);
    }
}

static void set_font(BeditSettings *gs, const gchar *font) {
    GList *views, *l;
    guint ts;

    g_settings_get(gs->editor, BEDIT_SETTINGS_TABS_SIZE, "u", &ts);

    views = bedit_app_get_views(BEDIT_APP(g_application_get_default()));

    for (l = views; l != NULL; l = g_list_next(l)) {
        /* Note: we use def=FALSE to avoid BeditView to query dconf */
        bedit_view_set_font(BEDIT_VIEW(l->data), FALSE, font);

        gtk_source_view_set_tab_width(GTK_SOURCE_VIEW(l->data), ts);
    }

    g_list_free(views);
}

static void on_system_font_changed(
    GSettings *settings, const gchar *key, BeditSettings *gs) {
    gboolean use_default_font;

    use_default_font =
        g_settings_get_boolean(gs->editor, BEDIT_SETTINGS_USE_DEFAULT_FONT);

    if (use_default_font) {
        gchar *font;

        font = g_settings_get_string(settings, key);
        set_font(gs, font);
        g_free(font);
    }
}

static void on_use_default_font_changed(
    GSettings *settings, const gchar *key, BeditSettings *gs) {
    gboolean def;
    gchar *font;

    def = g_settings_get_boolean(settings, key);

    if (def) {
        font = g_settings_get_string(gs->interface, BEDIT_SETTINGS_SYSTEM_FONT);
    } else {
        font = g_settings_get_string(gs->editor, BEDIT_SETTINGS_EDITOR_FONT);
    }

    set_font(gs, font);

    g_free(font);
}

static void on_editor_font_changed(
    GSettings *settings, const gchar *key, BeditSettings *gs) {
    gboolean use_default_font;

    use_default_font =
        g_settings_get_boolean(gs->editor, BEDIT_SETTINGS_USE_DEFAULT_FONT);

    if (!use_default_font) {
        gchar *font;

        font = g_settings_get_string(settings, key);
        set_font(gs, font);
        g_free(font);
    }
}

static void on_scheme_changed(
    GSettings *settings, const gchar *key, BeditSettings *gs) {
    GtkSourceStyleSchemeManager *manager;
    GtkSourceStyleScheme *style;
    gchar *scheme;
    GList *docs;
    GList *l;

    scheme = g_settings_get_string(settings, key);

    if (gs->old_scheme != NULL && (strcmp(scheme, gs->old_scheme) == 0)) {
        g_free(scheme);
        return;
    }

    g_free(gs->old_scheme);
    gs->old_scheme = scheme;

    manager = gtk_source_style_scheme_manager_get_default();
    style = gtk_source_style_scheme_manager_get_scheme(manager, scheme);
    if (style == NULL) {
        g_warning(
            "Default style scheme '%s' not found, falling back to 'classic'",
            scheme);

        style = gtk_source_style_scheme_manager_get_scheme(manager, "classic");
        if (style == NULL) {
            g_warning(
                "Style scheme 'classic' cannot be found, check your "
                "GtkSourceView installation.");
            return;
        }
    }

    docs = bedit_app_get_documents(BEDIT_APP(g_application_get_default()));

    for (l = docs; l != NULL; l = g_list_next(l)) {
        g_return_if_fail(GTK_SOURCE_IS_BUFFER(l->data));

        gtk_source_buffer_set_style_scheme(GTK_SOURCE_BUFFER(l->data), style);
    }

    g_list_free(docs);
}

static void on_auto_save_changed(
    GSettings *settings, const gchar *key, BeditSettings *gs) {
    GList *docs, *l;
    gboolean auto_save;

    auto_save = g_settings_get_boolean(settings, key);

    docs = bedit_app_get_documents(BEDIT_APP(g_application_get_default()));

    for (l = docs; l != NULL; l = g_list_next(l)) {
        BeditTab *tab = bedit_tab_get_from_document(BEDIT_DOCUMENT(l->data));

        bedit_tab_set_auto_save_enabled(tab, auto_save);
    }

    g_list_free(docs);
}

static void on_auto_save_interval_changed(
    GSettings *settings, const gchar *key, BeditSettings *gs) {
    GList *docs, *l;
    gint auto_save_interval;

    g_settings_get(settings, key, "u", &auto_save_interval);

    docs = bedit_app_get_documents(BEDIT_APP(g_application_get_default()));

    for (l = docs; l != NULL; l = g_list_next(l)) {
        BeditTab *tab = bedit_tab_get_from_document(BEDIT_DOCUMENT(l->data));

        bedit_tab_set_auto_save_interval(tab, auto_save_interval);
    }

    g_list_free(docs);
}

static void on_syntax_highlighting_changed(
    GSettings *settings, const gchar *key, BeditSettings *gs) {
    GList *docs, *windows, *l;
    gboolean enable;

    enable = g_settings_get_boolean(settings, key);

    docs = bedit_app_get_documents(BEDIT_APP(g_application_get_default()));

    for (l = docs; l != NULL; l = g_list_next(l)) {
        gtk_source_buffer_set_highlight_syntax(
            GTK_SOURCE_BUFFER(l->data), enable);
    }

    g_list_free(docs);

    /* update the sensitivity of the Higlight Mode menu item */
    windows =
        bedit_app_get_main_windows(BEDIT_APP(g_application_get_default()));

    for (l = windows; l != NULL; l = g_list_next(l)) {
        GAction *action;

        action =
            g_action_map_lookup_action(G_ACTION_MAP(l->data), "highlight-mode");
        g_simple_action_set_enabled(G_SIMPLE_ACTION(action), enable);
    }

    g_list_free(windows);
}

static void bedit_settings_init(BeditSettings *gs) {
    gs->old_scheme = NULL;
    gs->editor = g_settings_new("com.bwhmather.bedit.preferences.editor");
    gs->ui = g_settings_new("com.bwhmather.bedit.preferences.ui");

    /* Load settings */
    gs->lockdown = g_settings_new("org.gnome.desktop.lockdown");

    g_signal_connect(
        gs->lockdown, "changed", G_CALLBACK(on_lockdown_changed), NULL);

    gs->interface = g_settings_new("org.gnome.desktop.interface");

    g_signal_connect(
        gs->interface, "changed::monospace-font-name",
        G_CALLBACK(on_system_font_changed), gs);

    /* editor changes */
    g_signal_connect(
        gs->editor, "changed::use-default-font",
        G_CALLBACK(on_use_default_font_changed), gs);
    g_signal_connect(
        gs->editor, "changed::editor-font", G_CALLBACK(on_editor_font_changed),
        gs);
    g_signal_connect(
        gs->editor, "changed::scheme", G_CALLBACK(on_scheme_changed), gs);
    g_signal_connect(
        gs->editor, "changed::auto-save", G_CALLBACK(on_auto_save_changed), gs);
    g_signal_connect(
        gs->editor, "changed::auto-save-interval",
        G_CALLBACK(on_auto_save_interval_changed), gs);
    g_signal_connect(
        gs->editor, "changed::syntax-highlighting",
        G_CALLBACK(on_syntax_highlighting_changed), gs);
}

static void bedit_settings_class_init(BeditSettingsClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = bedit_settings_finalize;
    object_class->dispose = bedit_settings_dispose;
}

BeditSettings *bedit_settings_new(void) {
    return g_object_new(BEDIT_TYPE_SETTINGS, NULL);
}

BeditLockdownMask bedit_settings_get_lockdown(BeditSettings *gs) {
    guint lockdown = 0;
    gboolean command_line, printing, print_setup, save_to_disk;

    command_line = g_settings_get_boolean(
        gs->lockdown, BEDIT_SETTINGS_LOCKDOWN_COMMAND_LINE);
    printing =
        g_settings_get_boolean(gs->lockdown, BEDIT_SETTINGS_LOCKDOWN_PRINTING);
    print_setup = g_settings_get_boolean(
        gs->lockdown, BEDIT_SETTINGS_LOCKDOWN_PRINT_SETUP);
    save_to_disk = g_settings_get_boolean(
        gs->lockdown, BEDIT_SETTINGS_LOCKDOWN_SAVE_TO_DISK);

    if (command_line)
        lockdown |= BEDIT_LOCKDOWN_COMMAND_LINE;

    if (printing)
        lockdown |= BEDIT_LOCKDOWN_PRINTING;

    if (print_setup)
        lockdown |= BEDIT_LOCKDOWN_PRINT_SETUP;

    if (save_to_disk)
        lockdown |= BEDIT_LOCKDOWN_SAVE_TO_DISK;

    return lockdown;
}

gchar *bedit_settings_get_system_font(BeditSettings *gs) {
    gchar *system_font;

    g_return_val_if_fail(BEDIT_IS_SETTINGS(gs), NULL);

    system_font = g_settings_get_string(gs->interface, "monospace-font-name");

    return system_font;
}

GSList *bedit_settings_get_list(GSettings *settings, const gchar *key) {
    GSList *list = NULL;
    gchar **values;
    gsize i;

    g_return_val_if_fail(G_IS_SETTINGS(settings), NULL);
    g_return_val_if_fail(key != NULL, NULL);

    values = g_settings_get_strv(settings, key);
    i = 0;

    while (values[i] != NULL) {
        list = g_slist_prepend(list, values[i]);
        i++;
    }

    g_free(values);

    return g_slist_reverse(list);
}

void bedit_settings_set_list(
    GSettings *settings, const gchar *key, const GSList *list) {
    gchar **values = NULL;
    const GSList *l;

    g_return_if_fail(G_IS_SETTINGS(settings));
    g_return_if_fail(key != NULL);

    if (list != NULL) {
        gint i, len;

        len = g_slist_length((GSList *)list);
        values = g_new(gchar *, len + 1);

        for (l = list, i = 0; l != NULL; l = g_slist_next(l), i++) {
            values[i] = l->data;
        }
        values[i] = NULL;
    }

    g_settings_set_strv(settings, key, (const gchar *const *)values);
    g_free(values);
}

static gboolean strv_is_empty(gchar **strv) {
    if (strv == NULL || strv[0] == NULL) {
        return TRUE;
    }

    /* Contains one empty string. */
    if (strv[1] == NULL && strv[0][0] == '\0') {
        return TRUE;
    }

    return FALSE;
}

static GSList *encoding_strv_to_list(const gchar *const *encoding_strv) {
    GSList *list = NULL;
    gchar **p;

    for (p = (gchar **)encoding_strv; p != NULL && *p != NULL; p++) {
        const gchar *charset = *p;
        const GtkSourceEncoding *encoding;

        encoding = gtk_source_encoding_get_from_charset(charset);

        if (encoding != NULL && g_slist_find(list, encoding) == NULL) {
            list = g_slist_prepend(list, (gpointer)encoding);
        }
    }

    return g_slist_reverse(list);
}

/* Take in priority the candidate encodings from GSettings. If the gsetting is
 * empty, take the default candidates of GtkSourceEncoding.
 * Also, ensure that UTF-8 and the current locale encoding are present.
 * Returns: a list of GtkSourceEncodings. Free with g_slist_free().
 */
GSList *bedit_settings_get_candidate_encodings(gboolean *default_candidates) {
    const GtkSourceEncoding *utf8_encoding;
    const GtkSourceEncoding *current_encoding;
    GSettings *settings;
    gchar **settings_strv;
    GSList *candidates;

    utf8_encoding = gtk_source_encoding_get_utf8();
    current_encoding = gtk_source_encoding_get_current();

    settings = g_settings_new("com.bwhmather.bedit.preferences.encodings");

    settings_strv =
        g_settings_get_strv(settings, BEDIT_SETTINGS_CANDIDATE_ENCODINGS);

    if (strv_is_empty(settings_strv)) {
        if (default_candidates != NULL) {
            *default_candidates = TRUE;
        }

        candidates = gtk_source_encoding_get_default_candidates();
    } else {
        if (default_candidates != NULL) {
            *default_candidates = FALSE;
        }

        candidates = encoding_strv_to_list((const gchar *const *)settings_strv);

        /* Ensure that UTF-8 is present. */
        if (utf8_encoding != current_encoding &&
            g_slist_find(candidates, utf8_encoding) == NULL) {
            candidates = g_slist_prepend(candidates, (gpointer)utf8_encoding);
        }

        /* Ensure that the current locale encoding is present (if not
         * present, it must be the first encoding).
         */
        if (g_slist_find(candidates, current_encoding) == NULL) {
            candidates =
                g_slist_prepend(candidates, (gpointer)current_encoding);
        }
    }

    g_object_unref(settings);
    g_strfreev(settings_strv);
    return candidates;
}

