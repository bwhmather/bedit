/*
 * bedit-window.c
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * Based on gedit-window.c from Gedit.
 *
 * Copyright (C) 2000 - Chema Celorio, Jacob Leach
 * Copyright (C) 2005-2016 - Paolo Borelli
 * Copyright (C) 2006-2007 - Paolo Maggi
 * Copyright (C) 2006-2014 - Jesse van den Kieboom, Steve Frécinaux
 * Copyright (C) 2008 - Djihed Afifi
 * Copyright (C) 2008-2015 - Ignacio Casal Quinteiro
 * Copyright (C) 2009-2010 - Javier Jardón
 * Copyright (C) 2010 - Philip Withnall
 * Copyright (C) 2010-2011 - Garrett Regier
 * Copyright (C) 2011 - Cosimo Cecchi
 * Copyright (C) 2012 - Seif Lotfy
 * Copyright (C) 2013 - José Aliste, Nelson Benítez León, Pavel Vasin, Plamena
 *   Manolova
 * Copyright (C) 2013-2019 - Sébastien Wilmet
 * Copyright (C) 2014 - Daniel Mustieles, Paolo Bonzini, Robert Roth, Sagar
 *   Ghuge, Yosef Or Boczko
 * Copyright (C) 2014-2018 - Sebastien Lafargue
 * Copyright (C) 2015 - Lars Uebernickel
 * Copyright (C) 2016 - Matthias Clasen
 * Copyright (C) 2019 - Michael Catanzaro
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

#include "bedit-window.h"

#include <string.h>
#include <sys/types.h>
#include <time.h>

#include <glib/gi18n.h>
#include <libpeas/peas-extension-set.h>
#include <tepl/tepl.h>

#include "bedit-app-private.h"
#include "bedit-app.h"
#include "bedit-commands-private.h"
#include "bedit-commands.h"
#include "bedit-debug.h"
#include "bedit-dirs.h"
#include "bedit-document-private.h"
#include "bedit-document.h"
#include "bedit-enum-types.h"
#include "bedit-highlight-mode-selector.h"
#include "bedit-notebook-popup-menu.h"
#include "bedit-notebook.h"
#include "bedit-plugins-engine.h"
#include "bedit-searchbar.h"
#include "bedit-settings.h"
#include "bedit-status-menu-button.h"
#include "bedit-statusbar.h"
#include "bedit-tab-private.h"
#include "bedit-tab.h"
#include "bedit-utils.h"
#include "bedit-view-frame.h"
#include "bedit-window-activatable.h"
#include "bedit-window-private.h"

enum { PROP_0, PROP_STATE, PROP_DEFAULT_LOCATION, LAST_PROP };

static GParamSpec *properties[LAST_PROP];

enum {
    TAB_ADDED,
    TAB_REMOVED,
    TABS_REORDERED,
    ACTIVE_TAB_CHANGED,
    ACTIVE_TAB_STATE_CHANGED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

enum { TARGET_URI_LIST = 100, TARGET_XDNDDIRECTSAVE };

static const GtkTargetEntry drop_types[] = {
    {"XdndDirectSave0", 0, TARGET_XDNDDIRECTSAVE}, /* XDS Protocol Type */
    {"text/uri-list", 0, TARGET_URI_LIST}};

G_DEFINE_TYPE_WITH_PRIVATE(
    BeditWindow, bedit_window, GTK_TYPE_APPLICATION_WINDOW
)

/* Prototypes */
static void remove_actions(BeditWindow *window);

static void bedit_window_get_property(
    GObject *object, guint prop_id, GValue *value, GParamSpec *pspec
) {
    BeditWindow *window = BEDIT_WINDOW(object);

    switch (prop_id) {
    case PROP_STATE:
        g_value_set_flags(value, bedit_window_get_state(window));
        break;

    case PROP_DEFAULT_LOCATION:
        g_value_take_object(value, _bedit_window_get_default_location(window));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void bedit_window_set_property(
    GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec
) {
    BeditWindow *window = BEDIT_WINDOW(object);

    switch (prop_id) {
    case PROP_DEFAULT_LOCATION:
        _bedit_window_set_default_location(
            window, G_FILE(g_value_dup_object(value))
        );
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void save_window_state(GtkWidget *widget) {
    BeditWindow *window = BEDIT_WINDOW(widget);

    if ((
        window->priv->window_state &
        (GDK_WINDOW_STATE_MAXIMIZED | GDK_WINDOW_STATE_FULLSCREEN)
    ) == 0) {
        gtk_window_get_size(
            GTK_WINDOW(widget), &window->priv->width, &window->priv->height
        );

        g_settings_set(
            window->priv->window_settings, BEDIT_SETTINGS_WINDOW_SIZE,
            "(ii)", window->priv->width, window->priv->height
        );
    }
}

static void bedit_window_dispose(GObject *object) {
    BeditWindow *window;

    bedit_debug(DEBUG_WINDOW);

    window = BEDIT_WINDOW(object);

    /* First of all, force collection so that plugins
     * really drop some of the references.
     */
    peas_engine_garbage_collect(
        PEAS_ENGINE(bedit_plugins_engine_get_default())
    );

    /* Save window size and deactivate plugins for this window, but only once */
    if (!window->priv->dispose_has_run) {
        save_window_state(GTK_WIDGET(window));

        /* Note that unreffing the extensions will automatically remove
           all extensions which in turn will deactivate the extension */
        g_object_unref(window->priv->extensions);

        peas_engine_garbage_collect(
            PEAS_ENGINE(bedit_plugins_engine_get_default())
        );

        window->priv->dispose_has_run = TRUE;
    }

    g_clear_object(&window->priv->message_bus);
    g_clear_object(&window->priv->window_group);
    g_clear_object(&window->priv->default_location);

    /* We must free the settings after saving the panels */
    g_clear_object(&window->priv->editor_settings);
    g_clear_object(&window->priv->ui_settings);
    g_clear_object(&window->priv->window_settings);

    /* Now that there have broken some reference loops,
     * force collection again.
     */
    peas_engine_garbage_collect(
        PEAS_ENGINE(bedit_plugins_engine_get_default())
    );

    /* GTK+/GIO unref the action map in an idle. For the last BeditWindow,
     * the application quits before the idle, so the action map is not
     * unreffed, and some objects are not finalized on application shutdown
     * (BeditView for example).
     * So this is just for making the debugging of object references a bit
     * nicer.
     */
    remove_actions(window);

    G_OBJECT_CLASS(bedit_window_parent_class)->dispose(object);
}

static void bedit_window_finalize(GObject *object) {
    BeditWindow *window = BEDIT_WINDOW(object);

    g_slist_free_full(
        window->priv->closed_docs_stack,
        (GDestroyNotify)g_object_unref
    );

    G_OBJECT_CLASS(bedit_window_parent_class)->finalize(object);
}

static void update_fullscreen(BeditWindow *window, gboolean is_fullscreen) {
    GAction *fullscreen_action;

    if (is_fullscreen) {
        gtk_widget_hide(window->priv->statusbar);
    } else {
        if (g_settings_get_boolean(
            window->priv->ui_settings, "statusbar-visible"
        )) {
            gtk_widget_show(window->priv->statusbar);
        }
    }

    if (is_fullscreen) {
        gtk_widget_show_all(window->priv->leave_fullscreen_button);
    } else {
        gtk_widget_hide(window->priv->leave_fullscreen_button);
    }

    fullscreen_action = g_action_map_lookup_action(
        G_ACTION_MAP(window), "fullscreen"
    );

    g_simple_action_set_state(
        G_SIMPLE_ACTION(fullscreen_action),
        g_variant_new_boolean(is_fullscreen)
    );
}

static gboolean bedit_window_window_state_event(
    GtkWidget *widget, GdkEventWindowState *event) {
    BeditWindow *window = BEDIT_WINDOW(widget);

    window->priv->window_state = event->new_window_state;

    g_settings_set_int(
        window->priv->window_settings, BEDIT_SETTINGS_WINDOW_STATE,
        window->priv->window_state
    );

    if ((event->changed_mask & GDK_WINDOW_STATE_FULLSCREEN) != 0) {
        update_fullscreen(
            window,
            (event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN) != 0
        );
    }

    return GTK_WIDGET_CLASS(bedit_window_parent_class)
        ->window_state_event(widget, event);
}

static gboolean bedit_window_configure_event(
    GtkWidget *widget, GdkEventConfigure *event) {
    BeditWindow *window = BEDIT_WINDOW(widget);

    if (gtk_widget_get_realized(widget) && (
        window->priv->window_state &
        (GDK_WINDOW_STATE_MAXIMIZED | GDK_WINDOW_STATE_FULLSCREEN)
    ) == 0) {
        save_window_state(widget);
    }

    return GTK_WIDGET_CLASS(bedit_window_parent_class)
        ->configure_event(widget, event);
}

static gboolean bedit_window_process_change_tab_event(
    GtkWindow *window, GdkEventKey *event) {

    GdkDisplay *display;
    GdkKeymap *keymap;
    guint consumed_modifiers;

    guint keyval;
    guint modifiers;

    g_return_val_if_fail(BEDIT_IS_WINDOW(window), FALSE);

    display = gtk_widget_get_display(GTK_WIDGET(window));
    keymap = gdk_keymap_get_for_display(display);

    gdk_keymap_translate_keyboard_state(
        keymap,
        event->hardware_keycode, event->state, event->group,
        &keyval, NULL, NULL, &consumed_modifiers
    );

    /* Start with all applied modifier keys */
    modifiers = event->state;

    /* Filter out the modifiers that were applied when translating keyboard
     * state */
    modifiers &= ~consumed_modifiers;

    /* Filter out any modifiers we don't care about */
    modifiers &= GDK_CONTROL_MASK | GDK_SHIFT_MASK | GDK_MOD1_MASK;

    if (modifiers == GDK_CONTROL_MASK && keyval == GDK_KEY_Tab) {
        GtkWidget *notebook;

        notebook = _bedit_window_get_notebook(BEDIT_WINDOW(window));
        gtk_notebook_next_page(GTK_NOTEBOOK(notebook));

        return TRUE;
    }

    if (modifiers == GDK_CONTROL_MASK && keyval == GDK_KEY_ISO_Left_Tab) {
        GtkWidget *notebook;

        notebook = _bedit_window_get_notebook(BEDIT_WINDOW(window));
        gtk_notebook_prev_page(GTK_NOTEBOOK(notebook));

        return TRUE;
    }

    return FALSE;
}

/*
 * GtkWindow catches keybindings for the menu items _before_ passing them to
 * the focused widget. This is unfortunate and means that pressing ctrl+V
 * in an entry on a panel ends up pasting text in the TextView.
 * Here we override GtkWindow's handler to do the same things that it
 * does, but in the opposite order and then we chain up to the grand
 * parent handler, skipping gtk_window_key_press_event.
 */
static gboolean bedit_window_key_press_event(
    GtkWidget *widget, GdkEventKey *event
) {
    static gpointer grand_parent_class = NULL;

    GtkWindow *window = GTK_WINDOW(widget);
    gboolean handled = FALSE;

    if (grand_parent_class == NULL) {
        grand_parent_class =
            g_type_class_peek_parent(bedit_window_parent_class);
    }

    if (!handled) {
        handled = bedit_window_process_change_tab_event(window, event);
    }

    /* handle focus widget key events */
    if (!handled) {
        handled = gtk_window_propagate_key_event(window, event);
    }


    /* handle mnemonics and accelerators */
    if (!handled) {
        handled = gtk_window_activate_key(window, event);
    }

    /* Chain up, invokes binding set on window */
    if (!handled) {
        handled = GTK_WIDGET_CLASS(grand_parent_class)
            ->key_press_event(widget, event);
    }

    if (!handled) {
        return bedit_app_process_window_event(
            BEDIT_APP(g_application_get_default()), BEDIT_WINDOW(widget),
            (GdkEvent *)event
        );
    }

    return TRUE;
}

static void bedit_window_tab_removed(BeditWindow *window, BeditTab *tab) {
    peas_engine_garbage_collect(
        PEAS_ENGINE(bedit_plugins_engine_get_default())
    );
}

static void bedit_window_class_init(BeditWindowClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    klass->tab_removed = bedit_window_tab_removed;

    object_class->dispose = bedit_window_dispose;
    object_class->finalize = bedit_window_finalize;
    object_class->get_property = bedit_window_get_property;
    object_class->set_property = bedit_window_set_property;

    widget_class->window_state_event = bedit_window_window_state_event;
    widget_class->configure_event = bedit_window_configure_event;
    widget_class->key_press_event = bedit_window_key_press_event;

    properties[PROP_STATE] = g_param_spec_flags(
        "state", "State", "The window's state", BEDIT_TYPE_WINDOW_STATE,
        BEDIT_WINDOW_STATE_NORMAL, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS
    );
    properties[PROP_DEFAULT_LOCATION] = g_param_spec_object(
        "default-location", "Default Location", "The window's working directory",
        G_TYPE_FILE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS
    );
    g_object_class_install_properties(object_class, LAST_PROP, properties);

    signals[TAB_ADDED] = g_signal_new(
        "tab-added", G_OBJECT_CLASS_TYPE(object_class),
        G_SIGNAL_RUN_FIRST,
        G_STRUCT_OFFSET(BeditWindowClass, tab_added),
        NULL, NULL, NULL,
        G_TYPE_NONE, 1, BEDIT_TYPE_TAB
    );
    signals[TAB_REMOVED] = g_signal_new(
        "tab-removed", G_OBJECT_CLASS_TYPE(object_class),
        G_SIGNAL_RUN_FIRST,
        G_STRUCT_OFFSET(BeditWindowClass, tab_removed),
        NULL, NULL, NULL,
        G_TYPE_NONE, 1, BEDIT_TYPE_TAB
    );
    signals[TABS_REORDERED] = g_signal_new(
        "tabs-reordered", G_OBJECT_CLASS_TYPE(object_class),
        G_SIGNAL_RUN_FIRST,
        G_STRUCT_OFFSET(BeditWindowClass, tabs_reordered),
        NULL, NULL, NULL,
        G_TYPE_NONE, 0
    );
    signals[ACTIVE_TAB_CHANGED] = g_signal_new(
        "active-tab-changed", G_OBJECT_CLASS_TYPE(object_class),
        G_SIGNAL_RUN_FIRST,
        G_STRUCT_OFFSET(BeditWindowClass, active_tab_changed),
        NULL, NULL, NULL,
        G_TYPE_NONE, 1, BEDIT_TYPE_TAB
    );
    signals[ACTIVE_TAB_STATE_CHANGED] = g_signal_new(
        "active-tab-state-changed", G_OBJECT_CLASS_TYPE(object_class),
        G_SIGNAL_RUN_FIRST,
        G_STRUCT_OFFSET(BeditWindowClass, active_tab_state_changed),
        NULL, NULL, NULL,
        G_TYPE_NONE, 0
    );

    /* Bind class to template */
    gtk_widget_class_set_template_from_resource(
        widget_class, "/com/bwhmather/bedit/ui/bedit-window.ui"
    );
    gtk_widget_class_bind_template_child_private(
        widget_class, BeditWindow, action_area
    );
    gtk_widget_class_bind_template_child_private(
        widget_class, BeditWindow, gear_button
    );
    gtk_widget_class_bind_template_child_private(
        widget_class, BeditWindow, notebook
    );
    gtk_widget_class_bind_template_child_private(
        widget_class, BeditWindow, statusbar
    );
    gtk_widget_class_bind_template_child_private(
        widget_class, BeditWindow, searchbar
    );
    gtk_widget_class_bind_template_child_private(
        widget_class, BeditWindow, language_button
    );
    gtk_widget_class_bind_template_child_private(
        widget_class, BeditWindow, tab_width_button
    );
    gtk_widget_class_bind_template_child_private(
        widget_class, BeditWindow, line_col_button
    );
    gtk_widget_class_bind_template_child_private(
        widget_class, BeditWindow, leave_fullscreen_button
    );
}

static void received_clipboard_contents(
    GtkClipboard *clipboard, GtkSelectionData *selection_data,
    BeditWindow *window
) {
    BeditTab *tab;
    gboolean enabled;
    GAction *action;

    /* getting clipboard contents is async, so we need to
     * get the current tab and its state */

    tab = bedit_window_get_active_tab(window);

    if (tab != NULL) {
        BeditTabState state;
        gboolean state_normal;

        state = bedit_tab_get_state(tab);
        state_normal = (state == BEDIT_TAB_STATE_NORMAL);

        enabled = (
            state_normal &&
            gtk_selection_data_targets_include_text(selection_data)
        );
    } else {
        enabled = FALSE;
    }

    action = g_action_map_lookup_action(G_ACTION_MAP(window), "paste");

    /* Since this is emitted async, the disposal of the actions may have
     * already happened. Ensure that we have an action before setting the
     * state.
     */
    if (action != NULL) {
        g_simple_action_set_enabled(G_SIMPLE_ACTION(action), enabled);
    }

    g_object_unref(window);
}

static void set_paste_sensitivity_according_to_clipboard(
    BeditWindow *window, GtkClipboard *clipboard
) {
    GdkDisplay *display;

    display = gtk_clipboard_get_display(clipboard);

    if (gdk_display_supports_selection_notification(display)) {
        gtk_clipboard_request_contents(
            clipboard, gdk_atom_intern_static_string("TARGETS"),
            (GtkClipboardReceivedFunc)received_clipboard_contents,
            g_object_ref(window)
        );
    } else {
        GAction *action;

        action = g_action_map_lookup_action(G_ACTION_MAP(window), "paste");
        /* XFIXES extension not availbale, make
         * Paste always sensitive */
        g_simple_action_set_enabled(G_SIMPLE_ACTION(action), TRUE);
    }
}

static void extension_update_state(
    PeasExtensionSet *extensions, PeasPluginInfo *info, PeasExtension *exten,
    BeditWindow *window
) {
    bedit_window_activatable_update_state(BEDIT_WINDOW_ACTIVATABLE(exten));
}

static void update_actions_sensitivity(BeditWindow *window) {
    BeditNotebook *notebook;
    BeditTab *tab;
    gint num_tabs;
    BeditTabState state = BEDIT_TAB_STATE_NORMAL;
    BeditDocument *doc = NULL;
    GtkSourceFile *file = NULL;
    BeditView *view = NULL;
    gint tab_number = -1;
    GAction *action;
    gboolean editable = FALSE;
    gboolean search_active;
    GtkClipboard *clipboard;
    BeditLockdownMask lockdown;
    gboolean enable_syntax_highlighting;

    bedit_debug(DEBUG_WINDOW);

    notebook = window->priv->notebook;
    tab = bedit_notebook_get_active_tab(notebook);
    num_tabs = bedit_notebook_get_n_tabs(notebook);

    if (notebook != NULL && tab != NULL) {
        state = bedit_tab_get_state(tab);
        view = bedit_tab_get_view(tab);
        doc = BEDIT_DOCUMENT(gtk_text_view_get_buffer(GTK_TEXT_VIEW(view)));
        file = bedit_document_get_file(doc);
        tab_number =
            gtk_notebook_page_num(GTK_NOTEBOOK(notebook), GTK_WIDGET(tab));
        editable = gtk_text_view_get_editable(GTK_TEXT_VIEW(view));
    }

    search_active = bedit_searchbar_get_search_active(
        BEDIT_SEARCHBAR(window->priv->searchbar));

    lockdown = bedit_app_get_lockdown(BEDIT_APP(g_application_get_default()));

    clipboard =
        gtk_widget_get_clipboard(GTK_WIDGET(window), GDK_SELECTION_CLIPBOARD);

    action = g_action_map_lookup_action(G_ACTION_MAP(window), "save");
    g_simple_action_set_enabled(
        G_SIMPLE_ACTION(action),
        ((state == BEDIT_TAB_STATE_NORMAL) ||
         (state == BEDIT_TAB_STATE_EXTERNALLY_MODIFIED_NOTIFICATION)) &&
            (file != NULL) && !gtk_source_file_is_readonly(file) &&
            !(lockdown & BEDIT_LOCKDOWN_SAVE_TO_DISK));

    action = g_action_map_lookup_action(G_ACTION_MAP(window), "save-as");
    g_simple_action_set_enabled(
        G_SIMPLE_ACTION(action),
        ((state == BEDIT_TAB_STATE_NORMAL) ||
         (state == BEDIT_TAB_STATE_SAVING_ERROR) ||
         (state == BEDIT_TAB_STATE_EXTERNALLY_MODIFIED_NOTIFICATION)) &&
            (doc != NULL) && !(lockdown & BEDIT_LOCKDOWN_SAVE_TO_DISK));

    action = g_action_map_lookup_action(G_ACTION_MAP(window), "revert");
    g_simple_action_set_enabled(
        G_SIMPLE_ACTION(action),
        ((state == BEDIT_TAB_STATE_NORMAL) ||
         (state == BEDIT_TAB_STATE_EXTERNALLY_MODIFIED_NOTIFICATION)) &&
            (doc != NULL) && !bedit_document_is_untitled(doc));

    action =
        g_action_map_lookup_action(G_ACTION_MAP(window), "reopen-closed-tab");
    g_simple_action_set_enabled(
        G_SIMPLE_ACTION(action), (window->priv->closed_docs_stack != NULL));

    action = g_action_map_lookup_action(G_ACTION_MAP(window), "print");
    g_simple_action_set_enabled(
        G_SIMPLE_ACTION(action),
        ((state == BEDIT_TAB_STATE_NORMAL) ||
         (state == BEDIT_TAB_STATE_SHOWING_PRINT_PREVIEW)) &&
            (doc != NULL) && !(lockdown & BEDIT_LOCKDOWN_PRINTING));

    action = g_action_map_lookup_action(G_ACTION_MAP(window), "close");
    g_simple_action_set_enabled(
        G_SIMPLE_ACTION(action),
        (state != BEDIT_TAB_STATE_CLOSING) &&
            (state != BEDIT_TAB_STATE_SAVING) &&
            (state != BEDIT_TAB_STATE_SHOWING_PRINT_PREVIEW) &&
            (state != BEDIT_TAB_STATE_PRINTING) &&
            (state != BEDIT_TAB_STATE_SAVING_ERROR));

    action = g_action_map_lookup_action(G_ACTION_MAP(window), "undo");
    g_simple_action_set_enabled(
        G_SIMPLE_ACTION(action),
        (state == BEDIT_TAB_STATE_NORMAL) && (doc != NULL) &&
            gtk_source_buffer_can_undo(GTK_SOURCE_BUFFER(doc)));

    action = g_action_map_lookup_action(G_ACTION_MAP(window), "redo");
    g_simple_action_set_enabled(
        G_SIMPLE_ACTION(action),
        (state == BEDIT_TAB_STATE_NORMAL) && (doc != NULL) &&
            gtk_source_buffer_can_redo(GTK_SOURCE_BUFFER(doc)));

    action = g_action_map_lookup_action(G_ACTION_MAP(window), "cut");
    g_simple_action_set_enabled(
        G_SIMPLE_ACTION(action),
        (state == BEDIT_TAB_STATE_NORMAL) && editable && (doc != NULL) &&
            gtk_text_buffer_get_has_selection(GTK_TEXT_BUFFER(doc)));

    action = g_action_map_lookup_action(G_ACTION_MAP(window), "copy");
    g_simple_action_set_enabled(
        G_SIMPLE_ACTION(action),
        ((state == BEDIT_TAB_STATE_NORMAL) ||
         (state == BEDIT_TAB_STATE_EXTERNALLY_MODIFIED_NOTIFICATION)) &&
            (doc != NULL) &&
            gtk_text_buffer_get_has_selection(GTK_TEXT_BUFFER(doc)));

    action = g_action_map_lookup_action(G_ACTION_MAP(window), "paste");
    if (num_tabs > 0 && (state == BEDIT_TAB_STATE_NORMAL) && editable) {
        set_paste_sensitivity_according_to_clipboard(window, clipboard);
    } else {
        g_simple_action_set_enabled(G_SIMPLE_ACTION(action), FALSE);
    }

    action = g_action_map_lookup_action(G_ACTION_MAP(window), "delete");
    g_simple_action_set_enabled(
        G_SIMPLE_ACTION(action),
        (state == BEDIT_TAB_STATE_NORMAL) && editable && (doc != NULL) &&
            gtk_text_buffer_get_has_selection(GTK_TEXT_BUFFER(doc)));

    action = g_action_map_lookup_action(G_ACTION_MAP(window), "overwrite-mode");
    g_simple_action_set_enabled(G_SIMPLE_ACTION(action), doc != NULL);

    action = g_action_map_lookup_action(G_ACTION_MAP(window), "find-next");
    g_simple_action_set_enabled(
        G_SIMPLE_ACTION(action),
        ((state == BEDIT_TAB_STATE_NORMAL) ||
         (state == BEDIT_TAB_STATE_EXTERNALLY_MODIFIED_NOTIFICATION)) &&
            (doc != NULL) && search_active);

    action = g_action_map_lookup_action(G_ACTION_MAP(window), "find-prev");
    g_simple_action_set_enabled(
        G_SIMPLE_ACTION(action),
        ((state == BEDIT_TAB_STATE_NORMAL) ||
         (state == BEDIT_TAB_STATE_EXTERNALLY_MODIFIED_NOTIFICATION)) &&
            (doc != NULL) && search_active);

    action = g_action_map_lookup_action(G_ACTION_MAP(window), "goto-line");
    g_simple_action_set_enabled(
        G_SIMPLE_ACTION(action),
        ((state == BEDIT_TAB_STATE_NORMAL) ||
         (state == BEDIT_TAB_STATE_EXTERNALLY_MODIFIED_NOTIFICATION)) &&
            (doc != NULL));

    action = g_action_map_lookup_action(G_ACTION_MAP(window), "highlight-mode");
    enable_syntax_highlighting = g_settings_get_boolean(
        window->priv->editor_settings, BEDIT_SETTINGS_SYNTAX_HIGHLIGHTING);
    g_simple_action_set_enabled(
        G_SIMPLE_ACTION(action),
        (state != BEDIT_TAB_STATE_CLOSING) && (doc != NULL) &&
            enable_syntax_highlighting);

    action =
        g_action_map_lookup_action(G_ACTION_MAP(window), "move-to-new-window");
    g_simple_action_set_enabled(G_SIMPLE_ACTION(action), num_tabs > 1);

    action =
        g_action_map_lookup_action(G_ACTION_MAP(window), "previous-document");
    g_simple_action_set_enabled(G_SIMPLE_ACTION(action), tab_number > 0);

    action = g_action_map_lookup_action(G_ACTION_MAP(window), "next-document");
    g_simple_action_set_enabled(
        G_SIMPLE_ACTION(action),
        tab_number >= 0 &&
            tab_number < gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook)) - 1);

    /* We disable File->Quit/SaveAll/CloseAll while printing to avoid to have
       two operations (save and print/print preview) that uses the message area
       at the same time (may be we can remove this limitation in the future) */
    /* We disable File->Quit/CloseAll if state is saving since saving cannot be
       cancelled (may be we can remove this limitation in the future) */
    action = g_action_map_lookup_action(
        G_ACTION_MAP(g_application_get_default()), "quit");
    g_simple_action_set_enabled(
        G_SIMPLE_ACTION(action),
        !(window->priv->state & BEDIT_WINDOW_STATE_SAVING) &&
            !(window->priv->state & BEDIT_WINDOW_STATE_PRINTING));

    action = g_action_map_lookup_action(G_ACTION_MAP(window), "save-all");
    g_simple_action_set_enabled(
        G_SIMPLE_ACTION(action),
        !(window->priv->state & BEDIT_WINDOW_STATE_PRINTING) &&
            !(lockdown & BEDIT_LOCKDOWN_SAVE_TO_DISK) && num_tabs > 0);

    action = g_action_map_lookup_action(G_ACTION_MAP(window), "close-all");
    g_simple_action_set_enabled(
        G_SIMPLE_ACTION(action),
        num_tabs > 0 && !(window->priv->state & BEDIT_WINDOW_STATE_SAVING) &&
            !(window->priv->state & BEDIT_WINDOW_STATE_PRINTING) &&
            num_tabs > 0);

    peas_extension_set_foreach(
        window->priv->extensions,
        (PeasExtensionSetForeachFunc)extension_update_state, window);
}

static void on_language_selector_shown(
    BeditHighlightModeSelector *sel, BeditWindow *window
) {
    BeditDocument *doc;

    doc = bedit_window_get_active_document(window);
    if (doc) {
        bedit_highlight_mode_selector_select_language(
            sel, bedit_document_get_language(doc)
        );
    }
}

static void on_language_selected(
    BeditHighlightModeSelector *sel, GtkSourceLanguage *language,
    BeditWindow *window
) {
    BeditDocument *doc;

    doc = bedit_window_get_active_document(window);
    if (doc) {
        bedit_document_set_language(doc, language);
    }

    gtk_widget_hide(GTK_WIDGET(window->priv->language_popover));
}

static void setup_statusbar(BeditWindow *window) {
    BeditHighlightModeSelector *sel;

    bedit_debug(DEBUG_WINDOW);

    window->priv->generic_message_cid = gtk_statusbar_get_context_id(
        GTK_STATUSBAR(window->priv->statusbar), "generic_message"
    );
    window->priv->tip_message_cid = gtk_statusbar_get_context_id(
        GTK_STATUSBAR(window->priv->statusbar), "tip_message"
    );
    window->priv->bracket_match_message_cid = gtk_statusbar_get_context_id(
        GTK_STATUSBAR(window->priv->statusbar), "bracket_match_message"
    );

    g_settings_bind(
        window->priv->ui_settings, "statusbar-visible",
        window->priv->statusbar, "visible", G_SETTINGS_BIND_GET
    );

    /* Line Col button */
    gtk_menu_button_set_menu_model(
        GTK_MENU_BUTTON(window->priv->line_col_button),
        _bedit_app_get_line_col_menu(BEDIT_APP(g_application_get_default()))
    );

    /* Tab Width button */
    gtk_menu_button_set_menu_model(
        GTK_MENU_BUTTON(window->priv->tab_width_button),
        _bedit_app_get_tab_width_menu(BEDIT_APP(g_application_get_default()))
    );

    /* Language button */
    window->priv->language_popover = gtk_popover_new(
        window->priv->language_button
    );
    gtk_menu_button_set_popover(
        GTK_MENU_BUTTON(window->priv->language_button),
        window->priv->language_popover
    );

    sel = bedit_highlight_mode_selector_new();
    g_signal_connect(
        sel, "show",
        G_CALLBACK(on_language_selector_shown), window
    );
    g_signal_connect(
        sel, "language-selected",
        G_CALLBACK(on_language_selected), window
    );

    gtk_container_add(
        GTK_CONTAINER(window->priv->language_popover), GTK_WIDGET(sel)
    );
    gtk_widget_show(GTK_WIDGET(sel));
}

static BeditWindow *clone_window(BeditWindow *origin) {
    BeditWindow *window;
    GdkScreen *screen;
    BeditApp *app;

    bedit_debug(DEBUG_WINDOW);

    app = BEDIT_APP(g_application_get_default());

    screen = gtk_window_get_screen(GTK_WINDOW(origin));
    window = bedit_app_create_window(app, screen);

    gtk_window_set_default_size(
        GTK_WINDOW(window), origin->priv->width, origin->priv->height
    );

    if ((origin->priv->window_state & GDK_WINDOW_STATE_MAXIMIZED) != 0) {
        gtk_window_maximize(GTK_WINDOW(window));
    } else {
        gtk_window_unmaximize(GTK_WINDOW(window));
    }

    if ((origin->priv->window_state & GDK_WINDOW_STATE_STICKY) != 0) {
        gtk_window_stick(GTK_WINDOW(window));
    } else {
        gtk_window_unstick(GTK_WINDOW(window));
    }

    return window;
}

static void bracket_matched_cb(
    GtkSourceBuffer *buffer, GtkTextIter *iter,
    GtkSourceBracketMatchType result, BeditWindow *window
) {
    if (buffer != GTK_SOURCE_BUFFER(bedit_window_get_active_document(window)))
        return;

    switch (result) {
    case GTK_SOURCE_BRACKET_MATCH_NONE:
        gtk_statusbar_pop(
            GTK_STATUSBAR(window->priv->statusbar),
            window->priv->bracket_match_message_cid
        );
        break;
    case GTK_SOURCE_BRACKET_MATCH_OUT_OF_RANGE:
        bedit_statusbar_flash_message(
            BEDIT_STATUSBAR(window->priv->statusbar),
            window->priv->bracket_match_message_cid,
            _("Bracket match is out of range")
        );
        break;
    case GTK_SOURCE_BRACKET_MATCH_NOT_FOUND:
        bedit_statusbar_flash_message(
            BEDIT_STATUSBAR(window->priv->statusbar),
            window->priv->bracket_match_message_cid,
            _("Bracket match not found")
        );
        break;
    case GTK_SOURCE_BRACKET_MATCH_FOUND:
        bedit_statusbar_flash_message(
            BEDIT_STATUSBAR(window->priv->statusbar),
            window->priv->bracket_match_message_cid,
            _("Bracket match found on line: %d"),
            gtk_text_iter_get_line(iter) + 1
        );
        break;
    default:
        g_assert_not_reached();
    }
}

static void update_cursor_position_statusbar(
    GtkTextBuffer *buffer, BeditWindow *window
) {
    gint line, col;
    GtkTextIter iter;
    BeditView *view;
    gchar *msg = NULL;

    bedit_debug(DEBUG_WINDOW);

    if (buffer != GTK_TEXT_BUFFER(bedit_window_get_active_document(window))) {
        return;
    }

    view = bedit_window_get_active_view(window);

    gtk_text_buffer_get_iter_at_mark(
        buffer, &iter, gtk_text_buffer_get_insert(buffer)
    );

    line = 1 + gtk_text_iter_get_line(&iter);
    col = 1 + gtk_source_view_get_visual_column(GTK_SOURCE_VIEW(view), &iter);

    if ((line >= 0) || (col >= 0)) {
        /* Translators: "Ln" is an abbreviation for "Line", Col is an
        abbreviation for "Column". Please, use abbreviations if possible to
        avoid space problems. */
        msg = g_strdup_printf(_("  Ln %d, Col %d"), line, col);
    }

    bedit_status_menu_button_set_label(
        BEDIT_STATUS_MENU_BUTTON(window->priv->line_col_button), msg
    );

    g_free(msg);
}

static void set_overwrite_mode(BeditWindow *window, gboolean overwrite) {
    GAction *action;

    bedit_statusbar_set_overwrite(
        BEDIT_STATUSBAR(window->priv->statusbar), overwrite);

    action = g_action_map_lookup_action(
        G_ACTION_MAP(window), "overwrite-mode"
    );
    g_simple_action_set_state(
        G_SIMPLE_ACTION(action), g_variant_new_boolean(overwrite)
    );
}

static void overwrite_mode_changed(
    GtkTextView *view, GParamSpec *pspec, BeditWindow *window
) {
    if (view != GTK_TEXT_VIEW(bedit_window_get_active_view(window))) {
        return;
    }

    set_overwrite_mode(window, gtk_text_view_get_overwrite(view));
}

#define MAX_TITLE_LENGTH 100

static void set_title(BeditWindow *window) {
    BeditTab *tab;
    BeditDocument *doc = NULL;
    GtkSourceFile *file;
    gchar *name;
    gchar *dirname = NULL;
    gchar *main_title = NULL;
    gint len;

    tab = bedit_window_get_active_tab(window);

    if (tab == NULL) {
        bedit_app_set_window_title(
            BEDIT_APP(g_application_get_default()), window, "bedit"
        );
        return;
    }

    doc = bedit_tab_get_document(tab);
    g_return_if_fail(doc != NULL);

    file = bedit_document_get_file(doc);

    name = bedit_document_get_short_name_for_display(doc);

    len = g_utf8_strlen(name, -1);

    /* if the name is awfully long, truncate it and be done with it,
     * otherwise also show the directory (ellipsized if needed)
     */
    if (len > MAX_TITLE_LENGTH) {
        gchar *tmp;

        tmp = bedit_utils_str_middle_truncate(name, MAX_TITLE_LENGTH);
        g_free(name);
        name = tmp;
    } else {
        GFile *location = gtk_source_file_get_location(file);

        if (location != NULL) {
            gchar *str = bedit_utils_location_get_dirname_for_display(
                location
            );

            /* use the remaining space for the dir, but use a min of 20 chars
             * so that we do not end up with a dirname like "(a...b)".
             * This means that in the worst case when the filename is long 99
             * we have a title long 99 + 20, but I think it's a rare enough
             * case to be acceptable. It's justa darn title afterall :)
             */
            dirname = bedit_utils_str_middle_truncate(
                str, MAX(20, MAX_TITLE_LENGTH - len)
            );
            g_free(str);
        }
    }

    if (gtk_text_buffer_get_modified(GTK_TEXT_BUFFER(doc))) {
        gchar *tmp_name;

        tmp_name = g_strdup_printf("*%s", name);
        g_free(name);

        name = tmp_name;
    }

    if (gtk_source_file_is_readonly(file)) {
        if (dirname != NULL) {
            main_title = g_strdup_printf(
                "%s [%s] (%s) - bedit", name, _("Read-Only"), dirname
            );
        } else {
            main_title =
                g_strdup_printf("%s [%s] - bedit", name, _("Read-Only")
            );
        }
    } else {
        if (dirname != NULL) {
            main_title = g_strdup_printf("%s (%s) - bedit", name, dirname);
        } else {
            main_title = g_strdup_printf("%s - bedit", name);
        }
    }

    bedit_app_set_window_title(
        BEDIT_APP(g_application_get_default()), window, main_title
    );

    g_free(dirname);
    g_free(name);
    g_free(main_title);
}

#undef MAX_TITLE_LENGTH

static void tab_width_changed(
    GObject *object, GParamSpec *pspec, BeditWindow *window
) {
    guint new_tab_width;
    gchar *label;

    new_tab_width = gtk_source_view_get_tab_width(GTK_SOURCE_VIEW(object));

    label = g_strdup_printf(_("Tab Width: %u"), new_tab_width);
    bedit_status_menu_button_set_label(
        BEDIT_STATUS_MENU_BUTTON(window->priv->tab_width_button), label
    );
    g_free(label);
}

static void language_changed(
    GObject *object, GParamSpec *pspec, BeditWindow *window
) {
    GtkSourceLanguage *new_language;
    const gchar *label;

    new_language = gtk_source_buffer_get_language(GTK_SOURCE_BUFFER(object));

    if (new_language) {
        label = gtk_source_language_get_name(new_language);
    } else {
        label = _("Plain Text");
    }

    bedit_status_menu_button_set_label(
        BEDIT_STATUS_MENU_BUTTON(window->priv->language_button), label
    );

    peas_extension_set_foreach(
        window->priv->extensions,
        (PeasExtensionSetForeachFunc)extension_update_state, window
    );
}

static void update_statusbar_wrap_mode_checkbox_from_view(
    BeditWindow *window, BeditView *view
) {
    GtkWrapMode wrap_mode;
    GSimpleAction *simple_action;

    wrap_mode = gtk_text_view_get_wrap_mode(GTK_TEXT_VIEW(view));

    simple_action = G_SIMPLE_ACTION(
        g_action_map_lookup_action(G_ACTION_MAP(window), "wrap-mode")
    );
    g_simple_action_set_state(
        simple_action, g_variant_new_boolean(wrap_mode != GTK_WRAP_NONE)
    );
}

static void on_view_wrap_mode_changed(
    GObject *object, GParamSpec *pspec, BeditWindow *window
) {
    BeditView *view = bedit_window_get_active_view(window);

    update_statusbar_wrap_mode_checkbox_from_view(window, view);
}

static void _bedit_window_text_wrapping_change_state(
    GSimpleAction *simple, GVariant *value, gpointer window
) {
    gboolean result;
    BeditView *view;
    GtkWrapMode wrap_mode;
    GtkWrapMode current_wrap_mode;

    g_simple_action_set_state(simple, value);

    wrap_mode = g_settings_get_enum(
        BEDIT_WINDOW(window)->priv->editor_settings, BEDIT_SETTINGS_WRAP_MODE
    );

    current_wrap_mode = wrap_mode;
    result = g_variant_get_boolean(value);

    if (result && wrap_mode == GTK_WRAP_NONE) {
        current_wrap_mode = g_settings_get_enum(
            BEDIT_WINDOW(window)->priv->editor_settings,
            BEDIT_SETTINGS_WRAP_LAST_SPLIT_MODE
        );
    } else if (!result) {
        current_wrap_mode = GTK_WRAP_NONE;
    }

    view = bedit_window_get_active_view(BEDIT_WINDOW(window));

    g_signal_handler_block(
        view, BEDIT_WINDOW(window)->priv->wrap_mode_changed_id
    );
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(view), current_wrap_mode);
    g_signal_handler_unblock(
        view, BEDIT_WINDOW(window)->priv->wrap_mode_changed_id
    );
}

static GActionEntry text_wrapping_entrie[] = {
    {
        "wrap-mode", NULL, NULL, "false",
        _bedit_window_text_wrapping_change_state
    },
};

static void remove_actions(BeditWindow *window) {
    g_action_map_remove_action(G_ACTION_MAP(window), "auto-indent");
    g_action_map_remove_action(G_ACTION_MAP(window), "tab-width");
    g_action_map_remove_action(G_ACTION_MAP(window), "use-spaces");
    g_action_map_remove_action(G_ACTION_MAP(window), "show-line-numbers");
    g_action_map_remove_action(G_ACTION_MAP(window), "display-right-margin");
    g_action_map_remove_action(G_ACTION_MAP(window), "highlight-current-line");
    g_action_map_remove_action(G_ACTION_MAP(window), "wrap-mode");
}

static void sync_current_tab_actions(
    BeditWindow *window, BeditView *old_view, BeditView *new_view) {
    if (old_view != NULL) {
        remove_actions(window);

        g_signal_handler_disconnect(
            old_view, window->priv->wrap_mode_changed_id
        );
    }

    if (new_view != NULL) {
        GPropertyAction *action;

        action = g_property_action_new(
            "auto-indent", new_view, "auto-indent"
        );
        g_action_map_add_action(G_ACTION_MAP(window), G_ACTION(action));
        g_object_unref(action);

        action = g_property_action_new(
            "tab-width", new_view, "tab-width"
        );
        g_action_map_add_action(G_ACTION_MAP(window), G_ACTION(action));
        g_object_unref(action);

        action = g_property_action_new(
            "use-spaces", new_view, "insert-spaces-instead-of-tabs"
        );
        g_action_map_add_action(G_ACTION_MAP(window), G_ACTION(action));
        g_object_unref(action);

        action = g_property_action_new(
            "show-line-numbers", new_view, "show-line-numbers"
        );
        g_action_map_add_action(G_ACTION_MAP(window), G_ACTION(action));
        g_object_unref(action);

        action = g_property_action_new(
            "display-right-margin", new_view, "show-right-margin"
        );
        g_action_map_add_action(G_ACTION_MAP(window), G_ACTION(action));
        g_object_unref(action);

        action = g_property_action_new(
            "highlight-current-line", new_view, "highlight-current-line"
        );
        g_action_map_add_action(G_ACTION_MAP(window), G_ACTION(action));
        g_object_unref(action);

        g_action_map_add_action_entries(
            G_ACTION_MAP(window), text_wrapping_entrie,
            G_N_ELEMENTS(text_wrapping_entrie), window
        );

        update_statusbar_wrap_mode_checkbox_from_view(window, new_view);

        window->priv->wrap_mode_changed_id = g_signal_connect(
            new_view, "notify::wrap-mode",
            G_CALLBACK(on_view_wrap_mode_changed), window
        );
    }
}

static void update_statusbar(
    BeditWindow *window, BeditView *old_view, BeditView *new_view
) {
    if (old_view) {
        if (window->priv->tab_width_id) {
            g_signal_handler_disconnect(old_view, window->priv->tab_width_id);

            window->priv->tab_width_id = 0;
        }

        if (window->priv->language_changed_id) {
            g_signal_handler_disconnect(
                gtk_text_view_get_buffer(GTK_TEXT_VIEW(old_view)),
                window->priv->language_changed_id
            );

            window->priv->language_changed_id = 0;
        }
    }

    if (new_view) {
        BeditDocument *doc;

        doc = BEDIT_DOCUMENT(
            gtk_text_view_get_buffer(GTK_TEXT_VIEW(new_view))
        );

        /* sync the statusbar */
        update_cursor_position_statusbar(GTK_TEXT_BUFFER(doc), window);

        set_overwrite_mode(
            window, gtk_text_view_get_overwrite(GTK_TEXT_VIEW(new_view))
        );

        gtk_widget_show(window->priv->line_col_button);
        gtk_widget_show(window->priv->tab_width_button);
        gtk_widget_show(window->priv->language_button);

        window->priv->tab_width_id = g_signal_connect(
            new_view, "notify::tab-width",
            G_CALLBACK(tab_width_changed), window
        );

        window->priv->language_changed_id = g_signal_connect(
            doc, "notify::language",
            G_CALLBACK(language_changed), window
        );

        /* call it for the first time */
        tab_width_changed(G_OBJECT(new_view), NULL, window);
        language_changed(G_OBJECT(doc), NULL, window);
    }
}


static void on_switched_page(
    BeditNotebook *notebook, BeditTab *new_tab, guint page_num,
    BeditWindow *window
) {
    BeditTab *old_tab;
    BeditView *old_view, *new_view;

    bedit_debug(DEBUG_WINDOW);

    old_tab = window->priv->active_tab;
    window->priv->active_tab = new_tab;

    old_view = old_tab ? bedit_tab_get_view(old_tab) : NULL;
    new_view = new_tab ? bedit_tab_get_view(new_tab) : NULL;

    sync_current_tab_actions(window, old_view, new_view);
    update_statusbar(window, old_view, new_view);

    bedit_searchbar_set_view(
        BEDIT_SEARCHBAR(window->priv->searchbar), new_view);

    if (new_tab == NULL || window->priv->dispose_has_run) {
        return;
    }

    set_title(window);
    update_actions_sensitivity(window);

    g_signal_emit(G_OBJECT(window), signals[ACTIVE_TAB_CHANGED], 0, new_tab);
}

static void set_auto_save_enabled(BeditTab *tab, gpointer autosave) {
    bedit_tab_set_auto_save_enabled(tab, GPOINTER_TO_BOOLEAN(autosave));
}

void _bedit_window_set_lockdown(
    BeditWindow *window, BeditLockdownMask lockdown
) {
    gboolean autosave;

    /* start/stop autosave in each existing tab */
    autosave = g_settings_get_boolean(
        window->priv->editor_settings, BEDIT_SETTINGS_AUTO_SAVE
    );

    bedit_notebook_foreach_tab(
        window->priv->notebook, (GtkCallback)set_auto_save_enabled,
        &autosave
    );

    update_actions_sensitivity(window);
}

static void analyze_tab_state(BeditTab *tab, BeditWindow *window) {
    BeditTabState ts;

    ts = bedit_tab_get_state(tab);

    switch (ts) {
    case BEDIT_TAB_STATE_LOADING:
    case BEDIT_TAB_STATE_REVERTING:
        window->priv->state |= BEDIT_WINDOW_STATE_LOADING;
        break;

    case BEDIT_TAB_STATE_SAVING:
        window->priv->state |= BEDIT_WINDOW_STATE_SAVING;
        break;

    case BEDIT_TAB_STATE_PRINTING:
        window->priv->state |= BEDIT_WINDOW_STATE_PRINTING;
        break;

    case BEDIT_TAB_STATE_LOADING_ERROR:
    case BEDIT_TAB_STATE_REVERTING_ERROR:
    case BEDIT_TAB_STATE_SAVING_ERROR:
    case BEDIT_TAB_STATE_GENERIC_ERROR:
        window->priv->state |= BEDIT_WINDOW_STATE_ERROR;
        ++window->priv->num_tabs_with_error;
    default:
        /* NOP */
        break;
    }
}

static void update_window_state(BeditWindow *window) {
    BeditWindowState old_ws;
    gint old_num_of_errors;

    bedit_debug_message(DEBUG_WINDOW, "Old state: %x", window->priv->state);

    old_ws = window->priv->state;
    old_num_of_errors = window->priv->num_tabs_with_error;

    window->priv->state = 0;
    window->priv->num_tabs_with_error = 0;

    bedit_notebook_foreach_tab(
        window->priv->notebook, (GtkCallback)analyze_tab_state, window);

    bedit_debug_message(DEBUG_WINDOW, "New state: %x", window->priv->state);

    if (old_ws != window->priv->state) {
        update_actions_sensitivity(window);

        bedit_statusbar_set_window_state(
            BEDIT_STATUSBAR(window->priv->statusbar), window->priv->state,
            window->priv->num_tabs_with_error
        );

        g_object_notify_by_pspec(G_OBJECT(window), properties[PROP_STATE]);
    } else if (old_num_of_errors != window->priv->num_tabs_with_error) {
        bedit_statusbar_set_window_state(
            BEDIT_STATUSBAR(window->priv->statusbar), window->priv->state,
            window->priv->num_tabs_with_error
        );
    }
}

static void update_can_close(BeditWindow *window) {
    BeditWindowPrivate *priv = window->priv;
    GList *tabs;
    GList *l;
    gboolean can_close = TRUE;

    bedit_debug(DEBUG_WINDOW);

    tabs = bedit_notebook_get_all_tabs(priv->notebook);

    for (l = tabs; l != NULL; l = g_list_next(l)) {
        BeditTab *tab = l->data;

        if (!_bedit_tab_get_can_close(tab)) {
            can_close = FALSE;
            break;
        }
    }

    if (can_close && (priv->inhibition_cookie != 0)) {
        gtk_application_uninhibit(
            GTK_APPLICATION(g_application_get_default()),
            priv->inhibition_cookie
        );
        priv->inhibition_cookie = 0;
    } else if (!can_close && (priv->inhibition_cookie == 0)) {
        priv->inhibition_cookie = gtk_application_inhibit(
            GTK_APPLICATION(g_application_get_default()), GTK_WINDOW(window),
            GTK_APPLICATION_INHIBIT_LOGOUT, _("There are unsaved documents")
        );
    }

    g_list_free(tabs);
}

static void sync_state(BeditTab *tab, GParamSpec *pspec, BeditWindow *window) {
    bedit_debug(DEBUG_WINDOW);

    update_window_state(window);

    if (tab == bedit_window_get_active_tab(window)) {
        update_actions_sensitivity(window);

        g_signal_emit(G_OBJECT(window), signals[ACTIVE_TAB_STATE_CHANGED], 0);
    }
}

static void sync_name(BeditTab *tab, GParamSpec *pspec, BeditWindow *window) {
    if (tab == bedit_window_get_active_tab(window)) {
        set_title(window);
        update_actions_sensitivity(window);
    }
}

static void sync_can_close(
    BeditTab *tab, GParamSpec *pspec, BeditWindow *window
) {
    update_can_close(window);
}

static BeditWindow *get_drop_window(GtkWidget *widget) {
    GtkWidget *target_window;

    target_window = gtk_widget_get_toplevel(widget);
    g_return_val_if_fail(BEDIT_IS_WINDOW(target_window), NULL);

    return BEDIT_WINDOW(target_window);
}

static void load_uris_from_drop(BeditWindow *window, gchar **uri_list) {
    GSList *locations = NULL;
    gint i;
    GSList *loaded;

    if (uri_list == NULL) {
        return;
    }

    for (i = 0; uri_list[i] != NULL; ++i) {
        locations = g_slist_prepend(
            locations, g_file_new_for_uri(uri_list[i])
        );
    }

    locations = g_slist_reverse(locations);
    loaded = bedit_commands_load_locations(window, locations, NULL, 0, 0);

    g_slist_free(loaded);
    g_slist_free_full(locations, g_object_unref);
}

/* Handle drops on the BeditWindow */
static void drag_data_received_cb(
    GtkWidget *widget, GdkDragContext *context, gint x, gint y,
    GtkSelectionData *selection_data, guint info, guint timestamp,
    gpointer data
) {
    BeditWindow *window;
    gchar **uri_list;

    window = get_drop_window(widget);

    if (window == NULL) {
        return;
    }

    switch (info) {
    case TARGET_URI_LIST:
        uri_list = bedit_utils_drop_get_uris(selection_data);
        load_uris_from_drop(window, uri_list);
        g_strfreev(uri_list);

        gtk_drag_finish(context, TRUE, FALSE, timestamp);

        break;

    case TARGET_XDNDDIRECTSAVE:
        /* Indicate that we don't provide "F" fallback */
        if (
            gtk_selection_data_get_format(selection_data) == 8 &&
            gtk_selection_data_get_length(selection_data) == 1 &&
            gtk_selection_data_get_data(selection_data)[0] == 'F'
        ) {
            gdk_property_change(
                gdk_drag_context_get_source_window(context),
                gdk_atom_intern("XdndDirectSave0", FALSE),
                gdk_atom_intern("text/plain", FALSE), 8,
                GDK_PROP_MODE_REPLACE, (const guchar *)"", 0
            );
        } else if (
            gtk_selection_data_get_format(selection_data) == 8 &&
            gtk_selection_data_get_length(selection_data) == 1 &&
            gtk_selection_data_get_data(selection_data)[0] == 'S' &&
            window->priv->direct_save_uri != NULL
        ) {
            gchar **uris;

            uris = g_new(gchar *, 2);
            uris[0] = window->priv->direct_save_uri;
            uris[1] = NULL;

            load_uris_from_drop(window, uris);
            g_free(uris);
        }

        g_free(window->priv->direct_save_uri);
        window->priv->direct_save_uri = NULL;

        gtk_drag_finish(context, TRUE, FALSE, timestamp);

        break;
    }
}

static void drag_drop_cb(
    GtkWidget *widget, GdkDragContext *context,
    gint x, gint y, guint time,
    gpointer user_data
) {
    BeditWindow *window;
    GtkTargetList *target_list;
    GdkAtom target;

    window = get_drop_window(widget);

    target_list = gtk_drag_dest_get_target_list(widget);
    target = gtk_drag_dest_find_target(widget, context, target_list);

    if (target != GDK_NONE) {
        guint info;
        gboolean found;

        found = gtk_target_list_find(target_list, target, &info);
        g_assert(found);

        if (info == TARGET_XDNDDIRECTSAVE) {
            gchar *uri;
            uri = bedit_utils_set_direct_save_filename(context);

            if (uri != NULL) {
                g_free(window->priv->direct_save_uri);
                window->priv->direct_save_uri = uri;
            }
        }

        gtk_drag_get_data(GTK_WIDGET(widget), context, target, time);
    }
}

/* Handle drops on the BeditView */
static void drop_uris_cb(
    GtkWidget *widget, gchar **uri_list, BeditWindow *window
) {
    load_uris_from_drop(window, uri_list);
}

static void search_active_notify_cb(
    BeditSearchbar *doc, GParamSpec *pspec, BeditWindow *window) {
    update_actions_sensitivity(window);
}

static void can_undo(
    BeditDocument *doc, GParamSpec *pspec, BeditWindow *window
) {
    if (doc == bedit_window_get_active_document(window)) {
        update_actions_sensitivity(window);
    }
}

static void can_redo(
    BeditDocument *doc, GParamSpec *pspec, BeditWindow *window
) {
    if (doc == bedit_window_get_active_document(window)) {
        update_actions_sensitivity(window);
    }
}

static void selection_changed(
    BeditDocument *doc, GParamSpec *pspec, BeditWindow *window
) {
    if (doc == bedit_window_get_active_document(window)) {
        update_actions_sensitivity(window);
    }
}

static void readonly_changed(
    GtkSourceFile *file, GParamSpec *pspec, BeditWindow *window
) {
    update_actions_sensitivity(window);

    sync_name(bedit_window_get_active_tab(window), NULL, window);

    peas_extension_set_foreach(
        window->priv->extensions,
        (PeasExtensionSetForeachFunc)extension_update_state, window
    );
}

static void editable_changed(
    BeditView *view, GParamSpec *arg1, BeditWindow *window
) {
    peas_extension_set_foreach(
        window->priv->extensions,
        (PeasExtensionSetForeachFunc)extension_update_state, window
    );
}

static void on_page_added(
    GtkNotebook *notebook, GtkWidget *tab, guint page_num,
    BeditWindow *window
) {
    BeditView *view;
    BeditDocument *doc;
    GtkSourceFile *file;

    bedit_debug(DEBUG_WINDOW);

    g_return_if_fail(BEDIT_IS_NOTEBOOK(notebook));
    g_return_if_fail(BEDIT_IS_TAB(tab));
    g_return_if_fail(BEDIT_IS_WINDOW(window));

    update_actions_sensitivity(window);

    view = bedit_tab_get_view(BEDIT_TAB(tab));
    doc = bedit_tab_get_document(BEDIT_TAB(tab));
    file = bedit_document_get_file(doc);

    /* IMPORTANT: remember to disconnect the signal in notebook_tab_removed
     * if a new signal is connected here */

    g_signal_connect(
        tab, "notify::name",
        G_CALLBACK(sync_name), window
    );
    g_signal_connect(
        tab, "notify::state",
        G_CALLBACK(sync_state), window
    );
    g_signal_connect(
        tab, "notify::can-close",
        G_CALLBACK(sync_can_close), window
    );
    g_signal_connect(
        tab, "drop_uris",
        G_CALLBACK(drop_uris_cb), window
    );
    g_signal_connect(
        doc, "bracket-matched",
        G_CALLBACK(bracket_matched_cb), window
    );
    g_signal_connect(
        doc, "cursor-moved",
        G_CALLBACK(update_cursor_position_statusbar), window
    );
    g_signal_connect(
        doc, "notify::can-undo",
        G_CALLBACK(can_undo), window
    );
    g_signal_connect(
        doc, "notify::can-redo",
        G_CALLBACK(can_redo), window
    );
    g_signal_connect(
        doc, "notify::has-selection",
        G_CALLBACK(selection_changed), window
    );
    g_signal_connect(
        view, "notify::overwrite",
        G_CALLBACK(overwrite_mode_changed), window
    );
    g_signal_connect(
        view, "notify::editable",
        G_CALLBACK(editable_changed), window
    );
    g_signal_connect(
        file, "notify::read-only",
        G_CALLBACK(readonly_changed), window
    );

    update_window_state(window);
    update_can_close(window);

    g_signal_emit(G_OBJECT(window), signals[TAB_ADDED], 0, tab);
}

static void push_last_closed_doc(BeditWindow *window, BeditDocument *doc) {
    BeditWindowPrivate *priv = window->priv;
    GtkSourceFile *file = bedit_document_get_file(doc);
    GFile *location = gtk_source_file_get_location(file);

    if (location != NULL) {
        priv->closed_docs_stack =
            g_slist_prepend(priv->closed_docs_stack, location
        );
        g_object_ref(location);
    }
}

GFile *_bedit_window_pop_last_closed_doc(BeditWindow *window) {
    BeditWindowPrivate *priv = window->priv;
    GFile *f = NULL;

    if (window->priv->closed_docs_stack != NULL) {
        f = priv->closed_docs_stack->data;
        priv->closed_docs_stack = g_slist_remove(priv->closed_docs_stack, f);
    }

    return f;
}

static void on_page_removed(
    GtkNotebook *notebook, GtkWidget *tab, guint page_num,
    BeditWindow *window) {
    BeditView *view;
    BeditDocument *doc;
    gint num_tabs;

    bedit_debug(DEBUG_WINDOW);

    g_return_if_fail(BEDIT_IS_NOTEBOOK(notebook));
    g_return_if_fail(BEDIT_IS_TAB(tab));
    g_return_if_fail(BEDIT_IS_WINDOW(window));

    num_tabs = bedit_notebook_get_n_tabs(BEDIT_NOTEBOOK(notebook));

    view = bedit_tab_get_view(BEDIT_TAB(tab));
    doc = bedit_tab_get_document(BEDIT_TAB(tab));

    g_signal_handlers_disconnect_by_func(
        tab, G_CALLBACK(sync_name), window
    );
    g_signal_handlers_disconnect_by_func(
        tab, G_CALLBACK(sync_state), window
    );
    g_signal_handlers_disconnect_by_func(
        tab, G_CALLBACK(sync_can_close), window
    );
    g_signal_handlers_disconnect_by_func(
        tab, G_CALLBACK(drop_uris_cb), window
    );
    g_signal_handlers_disconnect_by_func(
        doc, G_CALLBACK(bracket_matched_cb), window
    );
    g_signal_handlers_disconnect_by_func(
        doc, G_CALLBACK(update_cursor_position_statusbar), window
    );
    g_signal_handlers_disconnect_by_func(
        doc, G_CALLBACK(can_undo), window
    );
    g_signal_handlers_disconnect_by_func(
        doc, G_CALLBACK(can_redo), window
    );
    g_signal_handlers_disconnect_by_func(
        doc, G_CALLBACK(selection_changed), window
    );
    g_signal_handlers_disconnect_by_func(
        doc, G_CALLBACK(readonly_changed), window
    );
    g_signal_handlers_disconnect_by_func(
        view, G_CALLBACK(overwrite_mode_changed), window
    );
    g_signal_handlers_disconnect_by_func(
        view, G_CALLBACK(editable_changed), window
    );

    if (tab == window->priv->active_tab) {
        if (window->priv->tab_width_id) {
            g_signal_handler_disconnect(view, window->priv->tab_width_id);
            window->priv->tab_width_id = 0;
        }

        if (window->priv->language_changed_id) {
            g_signal_handler_disconnect(doc, window->priv->language_changed_id);
            window->priv->language_changed_id = 0;
        }

        window->priv->active_tab = NULL;
    }

    g_return_if_fail(num_tabs >= 0);
    if (num_tabs == 0) {
        set_title(window);

        bedit_searchbar_set_view(
            BEDIT_SEARCHBAR(window->priv->searchbar), NULL);

        bedit_statusbar_clear_overwrite(
            BEDIT_STATUSBAR(window->priv->statusbar)
        );

        /* hide the combos */
        gtk_widget_hide(window->priv->line_col_button);
        gtk_widget_hide(window->priv->tab_width_button);
        gtk_widget_hide(window->priv->language_button);
    }

    if (!window->priv->dispose_has_run) {
        push_last_closed_doc(window, doc);

        if ((
            !window->priv->removing_tabs &&
            gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook)) > 0
        ) || num_tabs == 0) {
            update_actions_sensitivity(window);
        }
    }

    update_window_state(window);
    update_can_close(window);

    g_signal_emit(G_OBJECT(window), signals[TAB_REMOVED], 0, tab);
}

static void on_page_reordered(
    BeditNotebook *notebook, GtkWidget *page,
    gint page_num, BeditWindow *window
) {
    update_actions_sensitivity(window);

    g_signal_emit(G_OBJECT(window), signals[TABS_REORDERED], 0);
}

static GtkNotebook *on_notebook_create_window(
    GtkNotebook *notebook, GtkWidget *page, gint x,
    gint y, BeditWindow *window
) {
    BeditWindow *new_window;
    GtkWidget *new_notebook;

    new_window = clone_window(window);

    gtk_window_move(GTK_WINDOW(new_window), x, y);
    gtk_widget_show(GTK_WIDGET(new_window));

    new_notebook = _bedit_window_get_notebook(BEDIT_WINDOW(new_window));

    return GTK_NOTEBOOK(new_notebook);
}

static void on_tab_close_request(
    BeditNotebook *notebook, BeditTab *tab, GtkWindow *window
) {
    // TODO figure out if this is still needed without multi-notebook.
    /* Note: we are destroying the tab before the default handler
     * seems to be ok, but we need to keep an eye on this. */
    _bedit_cmd_file_close_tab(tab, BEDIT_WINDOW(window));
}

static void on_show_popup_menu(
    BeditNotebook *notebook, GdkEventButton *event, BeditTab *tab,
    BeditWindow *window
) {
    GtkWidget *menu;

    g_return_if_fail(BEDIT_IS_NOTEBOOK(notebook));
    g_return_if_fail(BEDIT_IS_TAB(tab));
    g_return_if_fail(BEDIT_IS_WINDOW(window));

    if (event == NULL) {
        return;
    }

    menu = bedit_notebook_popup_menu_new(window, tab);

    g_signal_connect(
        menu, "selection-done", G_CALLBACK(gtk_widget_destroy), NULL
    );

    gtk_widget_show(menu);
    gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent *)event);
}

static void clipboard_owner_change(
    GtkClipboard *clipboard, GdkEventOwnerChange *event, BeditWindow *window
) {
    set_paste_sensitivity_according_to_clipboard(window, clipboard);
}

static void window_realized(GtkWidget *window, gpointer *data) {
    GtkClipboard *clipboard;

    clipboard = gtk_widget_get_clipboard(window, GDK_SELECTION_CLIPBOARD);

    g_signal_connect(
        clipboard, "owner_change", G_CALLBACK(clipboard_owner_change), window
    );
}

static void window_unrealized(GtkWidget *window, gpointer *data) {
    GtkClipboard *clipboard;

    clipboard = gtk_widget_get_clipboard(window, GDK_SELECTION_CLIPBOARD);

    g_signal_handlers_disconnect_by_func(
        clipboard, G_CALLBACK(clipboard_owner_change), window
    );
}

static void extension_added(
    PeasExtensionSet *extensions, PeasPluginInfo *info, PeasExtension *exten,
    BeditWindow *window
) {
    bedit_window_activatable_activate(BEDIT_WINDOW_ACTIVATABLE(exten));
}

static void extension_removed(
    PeasExtensionSet *extensions, PeasPluginInfo *info, PeasExtension *exten,
    BeditWindow *window
) {
    bedit_window_activatable_deactivate(BEDIT_WINDOW_ACTIVATABLE(exten));
}

static GActionEntry win_entries[] = {
    {"new-tab", _bedit_cmd_file_new},
    {"open", _bedit_cmd_file_open},
    {"revert", _bedit_cmd_file_revert},
    {"reopen-closed-tab", _bedit_cmd_file_reopen_closed_tab},
    {"save", _bedit_cmd_file_save},
    {"save-as", _bedit_cmd_file_save_as},
    {"save-all", _bedit_cmd_file_save_all},
    {"close", _bedit_cmd_file_close},
    {"close-all", _bedit_cmd_file_close_all},
    {"print", _bedit_cmd_file_print},
    {"focus-active-view", NULL, NULL, "false", _bedit_cmd_view_focus_active},
    {"fullscreen", NULL, NULL, "false", _bedit_cmd_view_toggle_fullscreen_mode},
    {"leave-fullscreen", _bedit_cmd_view_leave_fullscreen_mode},
    {"show-find", _bedit_cmd_search_show_find},
    {"find-next", _bedit_cmd_search_find_next},
    {"find-prev", _bedit_cmd_search_find_prev},
    {"show-replace", _bedit_cmd_search_show_replace},
    {"goto-line", _bedit_cmd_search_goto_line},
    {"previous-document", _bedit_cmd_documents_previous_document},
    {"next-document", _bedit_cmd_documents_next_document},
    {"move-to-new-window", _bedit_cmd_documents_move_to_new_window},
    {"undo", _bedit_cmd_edit_undo},
    {"redo", _bedit_cmd_edit_redo},
    {"cut", _bedit_cmd_edit_cut},
    {"copy", _bedit_cmd_edit_copy},
    {"paste", _bedit_cmd_edit_paste},
    {"delete", _bedit_cmd_edit_delete},
    {"select-all", _bedit_cmd_edit_select_all},
    {"highlight-mode", _bedit_cmd_view_highlight_mode},
    {"overwrite-mode", NULL, NULL, "false", _bedit_cmd_edit_overwrite_mode}};

static void sync_fullscreen_actions(BeditWindow *window, gboolean fullscreen) {
    /* TODO this no longer changes anything and should be moved to init */
    GtkWidget *button;
    GPropertyAction *action;

    /* button = fullscreen ? window->priv->fullscreen_gear_button
                        : window->priv->gear_button; */
    button = window->priv->gear_button;
    g_action_map_remove_action(G_ACTION_MAP(window), "hamburger-menu");
    action = g_property_action_new("hamburger-menu", button, "active");
    g_action_map_add_action(G_ACTION_MAP(window), G_ACTION(action));
    g_object_unref(action);
}

static void init_amtk_application_window(BeditWindow *bedit_window) {
    AmtkApplicationWindow *amtk_window;

    amtk_window = amtk_application_window_get_from_gtk_application_window(
        GTK_APPLICATION_WINDOW(bedit_window)
    );
    amtk_application_window_set_statusbar(
        amtk_window, GTK_STATUSBAR(bedit_window->priv->statusbar)
    );
}

static void bedit_window_init(BeditWindow *window) {
    GtkTargetList *tl;
    GMenuModel *hamburger_menu;

    bedit_debug(DEBUG_WINDOW);

    window->priv = bedit_window_get_instance_private(window);

    window->priv->removing_tabs = FALSE;
    window->priv->state = BEDIT_WINDOW_STATE_NORMAL;
    window->priv->default_location = g_file_new_for_path(g_get_home_dir());
    window->priv->inhibition_cookie = 0;
    window->priv->dispose_has_run = FALSE;
    window->priv->direct_save_uri = NULL;
    window->priv->closed_docs_stack = NULL;
    window->priv->editor_settings =
        g_settings_new("com.bwhmather.bedit.preferences.editor"
    );
    window->priv->ui_settings =
        g_settings_new("com.bwhmather.bedit.preferences.ui"
    );

    /* window settings are applied only once the window is closed. We do not
       want to keep writing to disk when the window is dragged around */
    window->priv->window_settings = g_settings_new(
        "com.bwhmather.bedit.state.window"
    );
    g_settings_delay(window->priv->window_settings);


    window->priv->message_bus = bedit_message_bus_new();

    gtk_widget_init_template(GTK_WIDGET(window));
    init_amtk_application_window(window);

    g_action_map_add_action_entries(
        G_ACTION_MAP(window), win_entries, G_N_ELEMENTS(win_entries), window
    );

    window->priv->window_group = gtk_window_group_new();
    gtk_window_group_add_window(
        window->priv->window_group, GTK_WINDOW(window)
    );

    /* TODO inline */
    sync_fullscreen_actions(window, FALSE);

    hamburger_menu = _bedit_app_get_hamburger_menu(
        BEDIT_APP(g_application_get_default())
    );
    if (hamburger_menu) {
        gtk_menu_button_set_menu_model(
            GTK_MENU_BUTTON(window->priv->gear_button), hamburger_menu
        );
    } else {
        gtk_widget_hide(GTK_WIDGET(window->priv->gear_button));
        gtk_widget_set_no_show_all(GTK_WIDGET(window->priv->gear_button), TRUE);
    }

    /* Setup status bar */
    setup_statusbar(window);

    /* Setup search bar */
    g_signal_connect(
        window->priv->searchbar, "notify::search-active",
        G_CALLBACK(search_active_notify_cb), window);

    /* Setup main area */
    g_signal_connect(
        window->priv->notebook, "page-added",
        G_CALLBACK(on_page_added), window
    );

    g_signal_connect(
        window->priv->notebook, "page-removed",
        G_CALLBACK(on_page_removed), window
    );

    g_signal_connect(
        window->priv->notebook, "switch-page",
        G_CALLBACK(on_switched_page), window
    );

    g_signal_connect(
        window->priv->notebook, "tab-close-request",
        G_CALLBACK(on_tab_close_request), window
    );

    g_signal_connect(
        window->priv->notebook, "page-reordered",
        G_CALLBACK(on_page_reordered), window
    );

    g_signal_connect(
        window->priv->notebook, "create-window",
        G_CALLBACK(on_notebook_create_window), window
    );

    g_signal_connect(
        window->priv->notebook, "show-popup-menu",
        G_CALLBACK(on_show_popup_menu), window
    );

    /* Drag and drop support */
    gtk_drag_dest_set(
        GTK_WIDGET(window), (
            GTK_DEST_DEFAULT_MOTION |
            GTK_DEST_DEFAULT_HIGHLIGHT |
            GTK_DEST_DEFAULT_DROP
        ), drop_types, G_N_ELEMENTS(drop_types), GDK_ACTION_COPY
    );

    /* Add uri targets */
    tl = gtk_drag_dest_get_target_list(GTK_WIDGET(window));

    if (tl == NULL) {
        tl = gtk_target_list_new(drop_types, G_N_ELEMENTS(drop_types));
        gtk_drag_dest_set_target_list(GTK_WIDGET(window), tl);
        gtk_target_list_unref(tl);
    }

    gtk_target_list_add_uri_targets(tl, TARGET_URI_LIST);

    /* connect instead of override, so that we can
     * share the cb code with the view */
    g_signal_connect(
        window, "drag_data_received",
        G_CALLBACK(drag_data_received_cb), NULL
    );
    g_signal_connect(
        window, "drag_drop",
        G_CALLBACK(drag_drop_cb), NULL
    );

    /* we can get the clipboard only after the widget
     * is realized */
    g_signal_connect(
        window, "realize",
        G_CALLBACK(window_realized), NULL
    );
    g_signal_connect(
        window, "unrealize",
        G_CALLBACK(window_unrealized), NULL
    );

    bedit_debug_message(DEBUG_WINDOW, "Update plugins ui");

    window->priv->extensions = peas_extension_set_new(
        PEAS_ENGINE(bedit_plugins_engine_get_default()),
        BEDIT_TYPE_WINDOW_ACTIVATABLE, "window", window, NULL
    );
    g_signal_connect(
        window->priv->extensions, "extension-added",
        G_CALLBACK(extension_added), window
    );
    g_signal_connect(
        window->priv->extensions, "extension-removed",
        G_CALLBACK(extension_removed), window
    );
    peas_extension_set_foreach(
        window->priv->extensions,
        (PeasExtensionSetForeachFunc)extension_added,
        window
    );

    update_actions_sensitivity(window);

    bedit_debug_message(DEBUG_WINDOW, "END");
}

/**
 * bedit_window_get_active_view:
 * @window: a #BeditWindow
 *
 * Gets the active #BeditView.
 *
 * Returns: (transfer none): the active #BeditView
 */
BeditView *bedit_window_get_active_view(BeditWindow *window) {
    BeditTab *tab;
    BeditView *view;

    g_return_val_if_fail(BEDIT_IS_WINDOW(window), NULL);

    tab = bedit_window_get_active_tab(window);

    if (tab == NULL) {
        return NULL;
    }

    view = bedit_tab_get_view(tab);

    return view;
}

/**
 * bedit_window_get_active_document:
 * @window: a #BeditWindow
 *
 * Gets the active #BeditDocument.
 *
 * Returns: (transfer none): the active #BeditDocument
 */
BeditDocument *bedit_window_get_active_document(BeditWindow *window) {
    BeditView *view;

    g_return_val_if_fail(BEDIT_IS_WINDOW(window), NULL);

    view = bedit_window_get_active_view(window);
    if (view == NULL) {
        return NULL;
    }

    return BEDIT_DOCUMENT(gtk_text_view_get_buffer(GTK_TEXT_VIEW(view)));
}

GtkWidget *_bedit_window_get_notebook(BeditWindow *window) {
    g_return_val_if_fail(BEDIT_IS_WINDOW(window), NULL);

    return GTK_WIDGET(window->priv->notebook);
}

static BeditTab *process_create_tab(
    BeditWindow *window, GtkWidget *notebook, BeditTab *tab, gboolean jump_to) {
    if (tab == NULL) {
        return NULL;
    }

    bedit_debug(DEBUG_WINDOW);

    gtk_widget_show(GTK_WIDGET(tab));
    bedit_notebook_add_tab(BEDIT_NOTEBOOK(notebook), tab, -1, jump_to);

    if (!gtk_widget_get_visible(GTK_WIDGET(window))) {
        gtk_window_present(GTK_WINDOW(window));
    }

    return tab;
}

/**
 * bedit_window_create_tab:
 * @window: a #BeditWindow
 * @jump_to: %TRUE to set the new #BeditTab as active
 *
 * Creates a new #BeditTab and adds the new tab to the #GtkNotebook.
 * In case @jump_to is %TRUE the #GtkNotebook switches to that new #BeditTab.
 *
 * Returns: (transfer none): a new #BeditTab
 */
BeditTab *bedit_window_create_tab(BeditWindow *window, gboolean jump_to) {
    GtkWidget *notebook;
    BeditTab *tab;

    g_return_val_if_fail(BEDIT_IS_WINDOW(window), NULL);

    bedit_debug(DEBUG_WINDOW);

    notebook = _bedit_window_get_notebook(window);
    tab = _bedit_tab_new();
    gtk_widget_show(GTK_WIDGET(tab));

    return process_create_tab(window, notebook, tab, jump_to);
}

/**
 * bedit_window_create_tab_from_location:
 * @window: a #BeditWindow
 * @location: the location of the document
 * @encoding: (allow-none): a #GtkSourceEncoding, or %NULL
 * @line_pos: the line position to visualize
 * @column_pos: the column position to visualize
 * @create: %TRUE to create a new document in case @uri does exist
 * @jump_to: %TRUE to set the new #BeditTab as active
 *
 * Creates a new #BeditTab loading the document specified by @uri.
 * In case @jump_to is %TRUE the #GtkNotebook swithes to that new #BeditTab.
 * Whether @create is %TRUE, creates a new empty document if location does
 * not refer to an existing file
 *
 * Returns: (transfer none): a new #BeditTab
 */
BeditTab *bedit_window_create_tab_from_location(
    BeditWindow *window, GFile *location, const GtkSourceEncoding *encoding,
    gint line_pos, gint column_pos, gboolean create, gboolean jump_to
) {
    GtkWidget *notebook;
    BeditTab *tab;

    g_return_val_if_fail(BEDIT_IS_WINDOW(window), NULL);
    g_return_val_if_fail(G_IS_FILE(location), NULL);

    bedit_debug(DEBUG_WINDOW);

    tab = _bedit_tab_new();

    _bedit_tab_load(tab, location, encoding, line_pos, column_pos, create);

    notebook = _bedit_window_get_notebook(window);

    return process_create_tab(window, notebook, tab, jump_to);
}

/**
 * bedit_window_create_tab_from_stream:
 * @window: a #BeditWindow
 * @stream: a #GInputStream
 * @encoding: (allow-none): a #GtkSourceEncoding, or %NULL
 * @line_pos: the line position to visualize
 * @column_pos: the column position to visualize
 * @jump_to: %TRUE to set the new #BeditTab as active
 *
 * Returns: (transfer none): a new #BeditTab
 */
BeditTab *bedit_window_create_tab_from_stream(
    BeditWindow *window, GInputStream *stream,
    const GtkSourceEncoding *encoding, gint line_pos, gint column_pos,
    gboolean jump_to
) {
    GtkWidget *notebook;
    BeditTab *tab;

    bedit_debug(DEBUG_WINDOW);

    g_return_val_if_fail(BEDIT_IS_WINDOW(window), NULL);
    g_return_val_if_fail(G_IS_INPUT_STREAM(stream), NULL);

    tab = _bedit_tab_new();

    _bedit_tab_load_stream(tab, stream, encoding, line_pos, column_pos);

    notebook = _bedit_window_get_notebook(window);

    return process_create_tab(window, notebook, tab, jump_to);
}

/**
 * bedit_window_get_active_tab:
 * @window: a BeditWindow
 *
 * Gets the active #BeditTab in the @window.
 *
 * Returns: (transfer none): the active #BeditTab in the @window.
 */
BeditTab *bedit_window_get_active_tab(BeditWindow *window) {
    g_return_val_if_fail(BEDIT_IS_WINDOW(window), NULL);
    g_return_val_if_fail(BEDIT_IS_NOTEBOOK(window->priv->notebook), NULL);

    return bedit_notebook_get_active_tab(window->priv->notebook);
}

static void add_document(BeditTab *tab, GList **res) {
    BeditDocument *doc;

    doc = bedit_tab_get_document(tab);

    *res = g_list_prepend(*res, doc);
}

/**
 * bedit_window_get_documents:
 * @window: a #BeditWindow
 *
 * Gets a newly allocated list with all the documents in the window.
 * This list must be freed.
 *
 * Returns: (element-type Bedit.Document) (transfer container): a newly
 * allocated list with all the documents in the window
 */
GList *bedit_window_get_documents(BeditWindow *window) {
    GList *res = NULL;

    g_return_val_if_fail(BEDIT_IS_WINDOW(window), NULL);

    bedit_notebook_foreach_tab(
        window->priv->notebook, (GtkCallback)add_document, &res
    );

    res = g_list_reverse(res);

    return res;
}

static void add_view(BeditTab *tab, GList **res) {
    BeditView *view;

    view = bedit_tab_get_view(tab);

    *res = g_list_prepend(*res, view);
}

/**
 * bedit_window_get_views:
 * @window: a #BeditWindow
 *
 * Gets a list with all the views in the window. This list must be freed.
 *
 * Returns: (element-type Bedit.View) (transfer container): a newly allocated
 * list with all the views in the window
 */
GList *bedit_window_get_views(BeditWindow *window) {
    GList *res = NULL;

    g_return_val_if_fail(BEDIT_IS_WINDOW(window), NULL);

    bedit_notebook_foreach_tab(
        window->priv->notebook, (GtkCallback)add_view, &res
    );

    res = g_list_reverse(res);

    return res;
}

/**
 * bedit_window_close_tab:
 * @window: a #BeditWindow
 * @tab: the #BeditTab to close
 *
 * Closes the @tab.
 */
void bedit_window_close_tab(BeditWindow *window, BeditTab *tab) {
    GList *tabs = NULL;

    g_return_if_fail(BEDIT_IS_WINDOW(window));
    g_return_if_fail(BEDIT_IS_TAB(tab));
    g_return_if_fail(
        (bedit_tab_get_state(tab) != BEDIT_TAB_STATE_SAVING) &&
        (bedit_tab_get_state(tab) != BEDIT_TAB_STATE_SHOWING_PRINT_PREVIEW)
    );

    tabs = g_list_append(tabs, tab);
    bedit_notebook_close_tabs(window->priv->notebook, tabs);
    g_list_free(tabs);
}

/**
 * bedit_window_close_all_tabs:
 * @window: a #BeditWindow
 *
 * Closes all opened tabs.
 */
void bedit_window_close_all_tabs(BeditWindow *window) {
    g_return_if_fail(BEDIT_IS_WINDOW(window));
    g_return_if_fail(!(window->priv->state & BEDIT_WINDOW_STATE_SAVING));

    window->priv->removing_tabs = TRUE;

    bedit_notebook_close_all_tabs(window->priv->notebook);

    window->priv->removing_tabs = FALSE;
}

/**
 * bedit_window_close_tabs:
 * @window: a #BeditWindow
 * @tabs: (element-type Bedit.Tab): a list of #BeditTab
 *
 * Closes all tabs specified by @tabs.
 */
void bedit_window_close_tabs(BeditWindow *window, const GList *tabs) {
    g_return_if_fail(BEDIT_IS_WINDOW(window));
    g_return_if_fail(!(window->priv->state & BEDIT_WINDOW_STATE_SAVING));

    window->priv->removing_tabs = TRUE;

    bedit_notebook_close_tabs(window->priv->notebook, tabs);

    window->priv->removing_tabs = FALSE;
}

BeditWindow *_bedit_window_move_tab_to_new_window(
    BeditWindow *window, BeditTab *tab
) {
    BeditWindow *new_window;
    BeditNotebook *old_notebook;
    BeditNotebook *new_notebook;

    g_return_val_if_fail(BEDIT_IS_WINDOW(window), NULL);
    g_return_val_if_fail(BEDIT_IS_TAB(tab), NULL);

    new_window = clone_window(window);

    old_notebook = BEDIT_NOTEBOOK(gtk_widget_get_parent(GTK_WIDGET(tab)));
    new_notebook = new_window->priv->notebook;

    bedit_notebook_move_tab(old_notebook, new_notebook, tab, -1);

    gtk_widget_show(GTK_WIDGET(new_window));

    return new_window;
}

/**
 * bedit_window_set_active_tab:
 * @window: a #BeditWindow
 * @tab: a #BeditTab
 *
 * Switches to the tab that matches with @tab.
 */
void bedit_window_set_active_tab(BeditWindow *window, BeditTab *tab) {
    g_return_if_fail(BEDIT_IS_WINDOW(window));

    bedit_notebook_set_active_tab(window->priv->notebook, tab);
}

/**
 * bedit_window_get_group:
 * @window: a #BeditWindow
 *
 * Gets the #GtkWindowGroup in which @window resides.
 *
 * Returns: (transfer none): the #GtkWindowGroup
 */
GtkWindowGroup *bedit_window_get_group(BeditWindow *window) {
    g_return_val_if_fail(BEDIT_IS_WINDOW(window), NULL);

    return window->priv->window_group;
}

gboolean _bedit_window_is_removing_tabs(BeditWindow *window) {
    g_return_val_if_fail(BEDIT_IS_WINDOW(window), FALSE);

    return window->priv->removing_tabs;
}

/**
 * bedit_window_get_action_area:
 * @window: a #BeditWindow
 *
 * The actions area is the list of buttons to the right of the tab bar.
 *
 * Returns: (transfer none): #GtkContainer
 */
GtkWidget *bedit_window_get_action_area(BeditWindow *window) {
    g_return_val_if_fail(BEDIT_IS_WINDOW(window), NULL);

    return window->priv->action_area;
}

/**
 * bedit_window_get_statusbar:
 * @window: a #BeditWindow
 *
 * Gets the #BeditStatusbar of the @window.
 *
 * Returns: (transfer none): the #BeditStatusbar of the @window.
 */
GtkWidget *bedit_window_get_statusbar(BeditWindow *window) {
    g_return_val_if_fail(BEDIT_IS_WINDOW(window), 0);

    return window->priv->statusbar;
}

/**
 * bedit_window_get_state:
 * @window: a #BeditWindow
 *
 * Retrieves the state of the @window.
 *
 * Returns: the current #BeditWindowState of the @window.
 */
BeditWindowState bedit_window_get_state(BeditWindow *window) {
    g_return_val_if_fail(BEDIT_IS_WINDOW(window), BEDIT_WINDOW_STATE_NORMAL);

    return window->priv->state;
}

GFile *_bedit_window_get_default_location(BeditWindow *window) {
    g_return_val_if_fail(BEDIT_IS_WINDOW(window), NULL);

    return window->priv->default_location != NULL
        ? g_object_ref(window->priv->default_location)
        : NULL;
}

void _bedit_window_set_default_location(BeditWindow *window, GFile *location) {
    gboolean updated = FALSE;

    g_return_if_fail(BEDIT_IS_WINDOW(window));
    g_return_if_fail(G_IS_FILE(location));

    if (window->priv->default_location != NULL) {
        updated = !g_file_equal(location, window->priv->default_location);
        g_object_unref(window->priv->default_location);
    } else {
        updated = location != NULL;
    }

    window->priv->default_location = location;

    if (updated) {
        g_object_notify(G_OBJECT(window), "default-location");
    }

}

static void add_unsaved_doc(BeditTab *tab, GList **res) {
    if (!_bedit_tab_get_can_close(tab)) {
        BeditDocument *doc;

        doc = bedit_tab_get_document(tab);
        *res = g_list_prepend(*res, doc);
    }
}

/**
 * bedit_window_get_unsaved_documents:
 * @window: a #BeditWindow
 *
 * Gets the list of documents that need to be saved before closing the window.
 *
 * Returns: (element-type Bedit.Document) (transfer container): a list of
 * #BeditDocument that need to be saved before closing the window
 */
GList *bedit_window_get_unsaved_documents(BeditWindow *window) {
    GList *res = NULL;

    g_return_val_if_fail(BEDIT_IS_WINDOW(window), NULL);

    bedit_notebook_foreach_tab(
        window->priv->notebook, (GtkCallback)add_unsaved_doc, &res
    );

    return g_list_reverse(res);
}

GList *_bedit_window_get_all_tabs(BeditWindow *window) {
    g_return_val_if_fail(BEDIT_IS_WINDOW(window), NULL);

    return bedit_notebook_get_all_tabs(window->priv->notebook);
}

void _bedit_window_fullscreen(BeditWindow *window) {
    g_return_if_fail(BEDIT_IS_WINDOW(window));

    if (_bedit_window_is_fullscreen(window)) {
        return;
    }

    sync_fullscreen_actions(window, TRUE);

    /* Go to fullscreen mode and hide bars */
    gtk_window_fullscreen(GTK_WINDOW(&window->window));
}

void _bedit_window_unfullscreen(BeditWindow *window) {
    g_return_if_fail(BEDIT_IS_WINDOW(window));

    if (!_bedit_window_is_fullscreen(window)) {
        return;
    }

    sync_fullscreen_actions(window, FALSE);

    /* Unfullscreen and show bars */
    gtk_window_unfullscreen(GTK_WINDOW(&window->window));
}

gboolean _bedit_window_is_fullscreen(BeditWindow *window) {
    g_return_val_if_fail(BEDIT_IS_WINDOW(window), FALSE);

    return window->priv->window_state & GDK_WINDOW_STATE_FULLSCREEN;
}

/**
 * bedit_window_get_tab_from_location:
 * @window: a #BeditWindow
 * @location: a #GFile
 *
 * Gets the #BeditTab that matches with the given @location.
 *
 * Returns: (transfer none): the #BeditTab that matches with the given
 * @location.
 */
BeditTab *bedit_window_get_tab_from_location(
    BeditWindow *window, GFile *location
) {
    GList *tabs;
    GList *l;
    BeditTab *ret = NULL;

    g_return_val_if_fail(BEDIT_IS_WINDOW(window), NULL);
    g_return_val_if_fail(G_IS_FILE(location), NULL);

    tabs = bedit_notebook_get_all_tabs(window->priv->notebook);

    for (l = tabs; l != NULL; l = g_list_next(l)) {
        BeditDocument *doc;
        GtkSourceFile *file;
        BeditTab *tab;
        GFile *cur_location;

        tab = BEDIT_TAB(l->data);
        doc = bedit_tab_get_document(tab);
        file = bedit_document_get_file(doc);
        cur_location = gtk_source_file_get_location(file);

        if (cur_location != NULL) {
            gboolean found = g_file_equal(location, cur_location);

            if (found) {
                ret = tab;
                break;
            }
        }
    }

    g_list_free(tabs);

    return ret;
}

/**
 * bedit_window_get_message_bus:
 * @window: a #BeditWindow
 *
 * Gets the #BeditMessageBus associated with @window. The returned reference
 * is owned by the window and should not be unreffed.
 *
 * Return value: (transfer none): the #BeditMessageBus associated with @window
 */
BeditMessageBus *bedit_window_get_message_bus(BeditWindow *window) {
    g_return_val_if_fail(BEDIT_IS_WINDOW(window), NULL);

    return window->priv->message_bus;
}

