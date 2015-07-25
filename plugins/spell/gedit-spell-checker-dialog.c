/*
 * gedit-spell-checker-dialog.c
 * This file is part of gedit
 *
 * Copyright (C) 2002 Paolo Maggi
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

#include "gedit-spell-checker-dialog.h"
#include <glib/gi18n.h>

struct _GeditSpellCheckerDialog
{
	GtkWindow parent_instance;

	GeditSpellChecker *spell_checker;

	gchar *misspelled_word;

	GtkHeaderBar *header_bar;
	GtkLabel *misspelled_word_label;
	GtkEntry *word_entry;
	GtkWidget *check_word_button;
	GtkWidget *ignore_button;
	GtkWidget *ignore_all_button;
	GtkWidget *change_button;
	GtkWidget *change_all_button;
	GtkWidget *add_word_button;
	GtkTreeView *suggestions_view;

	GtkTreeModel *suggestions_model;
};

enum
{
	PROP_0,
	PROP_SPELL_CHECKER,
};

enum
{
	IGNORE,
	IGNORE_ALL,
	CHANGE,
	CHANGE_ALL,
	ADD_WORD_TO_PERSONAL,
	CLOSE,
	LAST_SIGNAL
};

enum
{
	COLUMN_SUGGESTION,
	N_COLUMNS
};

static void	update_suggestions_model 			(GeditSpellCheckerDialog *dialog,
								 GSList *suggestions);

static void	word_entry_changed_handler			(GtkEntry                *word_entry,
								 GeditSpellCheckerDialog *dialog);
static void	suggestions_selection_changed_handler		(GtkTreeSelection *selection,
								 GeditSpellCheckerDialog *dialog);
static void	check_word_button_clicked_handler 		(GtkButton *button,
								 GeditSpellCheckerDialog *dialog);
static void	add_word_button_clicked_handler 		(GtkButton *button,
								 GeditSpellCheckerDialog *dialog);
static void	ignore_button_clicked_handler 			(GtkButton *button,
								 GeditSpellCheckerDialog *dialog);
static void	ignore_all_button_clicked_handler 		(GtkButton *button,
								 GeditSpellCheckerDialog *dialog);
static void	change_button_clicked_handler 			(GtkButton *button,
								 GeditSpellCheckerDialog *dialog);
static void	change_all_button_clicked_handler 		(GtkButton *button,
								 GeditSpellCheckerDialog *dialog);
static void	suggestions_row_activated_handler		(GtkTreeView *view,
								 GtkTreePath *path,
								 GtkTreeViewColumn *column,
								 GeditSpellCheckerDialog *dialog);

static guint signals [LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GeditSpellCheckerDialog, gedit_spell_checker_dialog, GTK_TYPE_WINDOW)

static void
set_spell_checker (GeditSpellCheckerDialog *dialog,
		   GeditSpellChecker       *checker)
{
	const GeditSpellCheckerLanguage *lang;

	g_return_if_fail (dialog->spell_checker == NULL);
	dialog->spell_checker = g_object_ref (checker);

	lang = gedit_spell_checker_get_language (dialog->spell_checker);

	gtk_header_bar_set_subtitle (dialog->header_bar,
	                             gedit_spell_checker_language_to_string (lang));

	g_object_notify (G_OBJECT (dialog), "spell-checker");
}

static void
gedit_spell_checker_dialog_get_property (GObject    *object,
					 guint       prop_id,
					 GValue     *value,
					 GParamSpec *pspec)
{
	GeditSpellCheckerDialog *dialog = GEDIT_SPELL_CHECKER_DIALOG (object);

	switch (prop_id)
	{
		case PROP_SPELL_CHECKER:
			g_value_set_object (value, dialog->spell_checker);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gedit_spell_checker_dialog_set_property (GObject      *object,
					 guint         prop_id,
					 const GValue *value,
					 GParamSpec   *pspec)
{
	GeditSpellCheckerDialog *dialog = GEDIT_SPELL_CHECKER_DIALOG (object);

	switch (prop_id)
	{
		case PROP_SPELL_CHECKER:
			set_spell_checker (dialog, g_value_get_object (value));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gedit_spell_checker_dialog_dispose (GObject *object)
{
	GeditSpellCheckerDialog *dialog = GEDIT_SPELL_CHECKER_DIALOG (object);

	g_clear_object (&dialog->spell_checker);

	G_OBJECT_CLASS (gedit_spell_checker_dialog_parent_class)->dispose (object);
}

static void
gedit_spell_checker_dialog_finalize (GObject *object)
{
	GeditSpellCheckerDialog *dialog = GEDIT_SPELL_CHECKER_DIALOG (object);

	g_free (dialog->misspelled_word);

	G_OBJECT_CLASS (gedit_spell_checker_dialog_parent_class)->finalize (object);
}

static void
gedit_spell_checker_dialog_close (GeditSpellCheckerDialog *dialog)
{
	gtk_window_close (GTK_WINDOW (dialog));
}

static void
gedit_spell_checker_dialog_class_init (GeditSpellCheckerDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkBindingSet *binding_set;

	object_class->get_property = gedit_spell_checker_dialog_get_property;
	object_class->set_property = gedit_spell_checker_dialog_set_property;
	object_class->dispose = gedit_spell_checker_dialog_dispose;
	object_class->finalize = gedit_spell_checker_dialog_finalize;

	klass->close = gedit_spell_checker_dialog_close;

	binding_set = gtk_binding_set_by_class (klass);
	gtk_binding_entry_add_signal (binding_set, GDK_KEY_Escape, 0, "close", 0);

	g_object_class_install_property (object_class,
					 PROP_SPELL_CHECKER,
					 g_param_spec_object ("spell-checker",
							      "Spell Checker",
							      "",
							      GEDIT_TYPE_SPELL_CHECKER,
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT_ONLY |
							      G_PARAM_STATIC_STRINGS));

	signals[IGNORE] =
		g_signal_new ("ignore",
 			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GeditSpellCheckerDialogClass, ignore),
			      NULL, NULL, NULL,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_STRING);

	signals[IGNORE_ALL] =
		g_signal_new ("ignore_all",
 			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GeditSpellCheckerDialogClass, ignore_all),
			      NULL, NULL, NULL,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_STRING);

	signals[CHANGE] =
		g_signal_new ("change",
 			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GeditSpellCheckerDialogClass, change),
			      NULL, NULL, NULL,
			      G_TYPE_NONE,
			      2,
			      G_TYPE_STRING,
			      G_TYPE_STRING);

	signals[CHANGE_ALL] =
		g_signal_new ("change_all",
 			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GeditSpellCheckerDialogClass, change_all),
			      NULL, NULL, NULL,
			      G_TYPE_NONE,
			      2,
			      G_TYPE_STRING,
			      G_TYPE_STRING);

	signals[ADD_WORD_TO_PERSONAL] =
		g_signal_new ("add_word_to_personal",
 			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GeditSpellCheckerDialogClass, add_word_to_personal),
			      NULL, NULL, NULL,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_STRING);

	signals[CLOSE] =
		g_signal_new ("close",
			      G_OBJECT_CLASS_TYPE (klass),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (GeditSpellCheckerDialogClass, close),
			      NULL, NULL, NULL,
			      G_TYPE_NONE,
			      0);
}

static void
gedit_spell_checker_dialog_init (GeditSpellCheckerDialog *dialog)
{
	GtkBuilder *builder;
	GtkWidget *content;
	GtkTreeViewColumn *column;
	GtkCellRenderer *cell;
	GtkTreeSelection *selection;
	gchar *root_objects[] = {
		"header_bar",
		"content",
		"check_word_image",
		"add_word_image",
		"ignore_image",
		"change_image",
		"ignore_all_image",
		"change_all_image",
		NULL
	};

	builder = gtk_builder_new ();
	gtk_builder_add_objects_from_resource (builder, "/org/gnome/gedit/plugins/spell/ui/spell-checker.ui",
	                                       root_objects, NULL);
	content = GTK_WIDGET (gtk_builder_get_object (builder, "content"));
	dialog->header_bar = GTK_HEADER_BAR (gtk_builder_get_object (builder, "header_bar"));
	dialog->misspelled_word_label = GTK_LABEL (gtk_builder_get_object (builder, "misspelled_word_label"));
	dialog->word_entry = GTK_ENTRY (gtk_builder_get_object (builder, "word_entry"));
	dialog->check_word_button = GTK_WIDGET (gtk_builder_get_object (builder, "check_word_button"));
	dialog->ignore_button = GTK_WIDGET (gtk_builder_get_object (builder, "ignore_button"));
	dialog->ignore_all_button = GTK_WIDGET (gtk_builder_get_object (builder, "ignore_all_button"));
	dialog->change_button = GTK_WIDGET (gtk_builder_get_object (builder, "change_button"));
	dialog->change_all_button = GTK_WIDGET (gtk_builder_get_object (builder, "change_all_button"));
	dialog->add_word_button = GTK_WIDGET (gtk_builder_get_object (builder, "add_word_button"));
	dialog->suggestions_view = GTK_TREE_VIEW (gtk_builder_get_object (builder, "suggestions_view"));

	gtk_window_set_titlebar (GTK_WINDOW (dialog),
				 GTK_WIDGET (dialog->header_bar));

	gtk_header_bar_set_subtitle (dialog->header_bar, NULL);

	gtk_container_add (GTK_CONTAINER (dialog), content);
	g_object_unref (builder);

	gtk_label_set_label (dialog->misspelled_word_label, "");
	gtk_widget_set_sensitive (GTK_WIDGET (dialog->word_entry), FALSE);
	gtk_widget_set_sensitive (dialog->check_word_button, FALSE);
	gtk_widget_set_sensitive (dialog->ignore_button, FALSE);
	gtk_widget_set_sensitive (dialog->ignore_all_button, FALSE);
	gtk_widget_set_sensitive (dialog->change_button, FALSE);
	gtk_widget_set_sensitive (dialog->change_all_button, FALSE);
	gtk_widget_set_sensitive (dialog->add_word_button, FALSE);

	gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

	/* Suggestion list */
	dialog->suggestions_model = GTK_TREE_MODEL (gtk_list_store_new (N_COLUMNS, G_TYPE_STRING));

	gtk_tree_view_set_model (dialog->suggestions_view,
				 dialog->suggestions_model);

	/* Add the suggestions column */
	cell = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Suggestions"), cell,
							   "text", COLUMN_SUGGESTION,
							   NULL);

	gtk_tree_view_append_column (dialog->suggestions_view, column);

	gtk_tree_view_set_search_column (dialog->suggestions_view, COLUMN_SUGGESTION);

	selection = gtk_tree_view_get_selection (dialog->suggestions_view);

	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

	/* Set default button */
	gtk_widget_set_can_default (dialog->change_button, TRUE);
	gtk_widget_grab_default (dialog->change_button);

	gtk_entry_set_activates_default (dialog->word_entry, TRUE);

	/* Connect signals */
	g_signal_connect (dialog->word_entry,
			  "changed",
			  G_CALLBACK (word_entry_changed_handler),
			  dialog);

	g_signal_connect (selection,
			  "changed",
			  G_CALLBACK (suggestions_selection_changed_handler),
			  dialog);

	g_signal_connect (dialog->check_word_button,
			  "clicked",
			  G_CALLBACK (check_word_button_clicked_handler),
			  dialog);

	g_signal_connect (dialog->add_word_button,
			  "clicked",
			  G_CALLBACK (add_word_button_clicked_handler),
			  dialog);

	g_signal_connect (dialog->ignore_button,
			  "clicked",
			  G_CALLBACK (ignore_button_clicked_handler),
			  dialog);

	g_signal_connect (dialog->ignore_all_button,
			  "clicked",
			  G_CALLBACK (ignore_all_button_clicked_handler),
			  dialog);

	g_signal_connect (dialog->change_button,
			  "clicked",
			  G_CALLBACK (change_button_clicked_handler),
			  dialog);

	g_signal_connect (dialog->change_all_button,
			  "clicked",
			  G_CALLBACK (change_all_button_clicked_handler),
			  dialog);

	g_signal_connect (dialog->suggestions_view,
			  "row-activated",
			  G_CALLBACK (suggestions_row_activated_handler),
			  dialog);
}

GtkWidget *
gedit_spell_checker_dialog_new (GeditSpellChecker *checker)
{
	g_return_val_if_fail (GEDIT_IS_SPELL_CHECKER (checker), NULL);

	return g_object_new (GEDIT_TYPE_SPELL_CHECKER_DIALOG,
			     "spell-checker", checker,
			     NULL);
}

void
gedit_spell_checker_dialog_set_misspelled_word (GeditSpellCheckerDialog *dialog,
						const gchar             *word)
{
	gchar *label;
	GSList *suggestions;

	g_return_if_fail (GEDIT_IS_SPELL_CHECKER_DIALOG (dialog));
	g_return_if_fail (word != NULL);

	g_return_if_fail (dialog->spell_checker != NULL);
	g_return_if_fail (!gedit_spell_checker_check_word (dialog->spell_checker, word, NULL));

	g_free (dialog->misspelled_word);
	dialog->misspelled_word = g_strdup (word);

	label = g_strdup_printf("<b>%s</b>", word);
	gtk_label_set_label (dialog->misspelled_word_label, label);
	g_free (label);

	suggestions = gedit_spell_checker_get_suggestions (dialog->spell_checker,
							   dialog->misspelled_word);

	update_suggestions_model (dialog, suggestions);

	g_slist_free_full (suggestions, g_free);

	gtk_widget_set_sensitive (dialog->ignore_button, TRUE);
	gtk_widget_set_sensitive (dialog->ignore_all_button, TRUE);
	gtk_widget_set_sensitive (dialog->add_word_button, TRUE);
}

static void
update_suggestions_model (GeditSpellCheckerDialog *dialog,
			  GSList                  *suggestions)
{
	GtkListStore *store;
	GtkTreeIter iter;
	GtkTreeSelection *selection;
	const gchar *first_suggestion;
	GSList *l;

	store = GTK_LIST_STORE (dialog->suggestions_model);
	gtk_list_store_clear (store);

	gtk_widget_set_sensitive (GTK_WIDGET (dialog->word_entry), TRUE);

	if (suggestions == NULL)
	{
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
		                    /* Translators: Displayed in the "Check Spelling" dialog if there are no suggestions
		                     * for the current misspelled word */
				    COLUMN_SUGGESTION, _("(no suggested words)"),
				    -1);

		gtk_entry_set_text (dialog->word_entry, "");
		gtk_widget_set_sensitive (GTK_WIDGET (dialog->suggestions_view), FALSE);
		return;
	}

	gtk_widget_set_sensitive (GTK_WIDGET (dialog->suggestions_view), TRUE);

	first_suggestion = suggestions->data;
	gtk_entry_set_text (dialog->word_entry, first_suggestion);

	for (l = suggestions; l != NULL; l = l->next)
	{
		const gchar *suggestion = l->data;

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
				    COLUMN_SUGGESTION, suggestion,
				    -1);
	}

	selection = gtk_tree_view_get_selection (dialog->suggestions_view);
	gtk_tree_model_get_iter_first (dialog->suggestions_model, &iter);
	gtk_tree_selection_select_iter (selection, &iter);
}

static void
word_entry_changed_handler (GtkEntry                *word_entry,
			    GeditSpellCheckerDialog *dialog)
{
	gboolean sensitive;

	sensitive = gtk_entry_get_text_length (word_entry) > 0;

	gtk_widget_set_sensitive (dialog->check_word_button, sensitive);
	gtk_widget_set_sensitive (dialog->change_button, sensitive);
	gtk_widget_set_sensitive (dialog->change_all_button, sensitive);
}

static void
suggestions_selection_changed_handler (GtkTreeSelection        *selection,
				       GeditSpellCheckerDialog *dialog)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *text;

	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
	{
		return;
	}

	gtk_tree_model_get (model, &iter,
			    COLUMN_SUGGESTION, &text,
			    -1);

	gtk_entry_set_text (dialog->word_entry, text);

	g_free (text);
}

static void
check_word_button_clicked_handler (GtkButton               *button,
				   GeditSpellCheckerDialog *dialog)
{
	const gchar *word;
	gboolean correctly_spelled;
	GError *error = NULL;

	g_return_if_fail (gtk_entry_get_text_length (dialog->word_entry) > 0);

	word = gtk_entry_get_text (dialog->word_entry);

	correctly_spelled = gedit_spell_checker_check_word (dialog->spell_checker, word, &error);

	if (error != NULL)
	{
		g_warning ("Spell checker dialog: %s", error->message);
		g_error_free (error);
		return;
	}

	if (correctly_spelled)
	{
		GtkListStore *store;
		GtkTreeIter iter;

		store = GTK_LIST_STORE (dialog->suggestions_model);
		gtk_list_store_clear (store);

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
		                    /* Translators: Displayed in the "Check
				     * Spelling" dialog if the current word
				     * isn't misspelled.
				     */
				    COLUMN_SUGGESTION, _("(correct spelling)"),
				    -1);

		gtk_widget_set_sensitive (GTK_WIDGET (dialog->suggestions_view), FALSE);
	}
	else
	{
		GSList *suggestions;

		suggestions = gedit_spell_checker_get_suggestions (dialog->spell_checker, word);

		update_suggestions_model (dialog, suggestions);

		g_slist_free_full (suggestions, g_free);
	}
}

static void
add_word_button_clicked_handler (GtkButton               *button,
				 GeditSpellCheckerDialog *dialog)
{
	gchar *word;

	g_return_if_fail (dialog->misspelled_word != NULL);

	gedit_spell_checker_add_word_to_personal (dialog->spell_checker,
						  dialog->misspelled_word);

	word = g_strdup (dialog->misspelled_word);
	g_signal_emit (G_OBJECT (dialog), signals [ADD_WORD_TO_PERSONAL], 0, word);
	g_free (word);
}

static void
ignore_button_clicked_handler (GtkButton               *button,
			       GeditSpellCheckerDialog *dialog)
{
	gchar *word;

	g_return_if_fail (dialog->misspelled_word != NULL);

	word = g_strdup (dialog->misspelled_word);
	g_signal_emit (G_OBJECT (dialog), signals [IGNORE], 0, word);
	g_free (word);
}

static void
ignore_all_button_clicked_handler (GtkButton               *button,
				   GeditSpellCheckerDialog *dialog)
{
	gchar *word;

	g_return_if_fail (dialog->misspelled_word != NULL);

	gedit_spell_checker_add_word_to_session (dialog->spell_checker,
						 dialog->misspelled_word);

	word = g_strdup (dialog->misspelled_word);
	g_signal_emit (G_OBJECT (dialog), signals [IGNORE_ALL], 0, word);
	g_free (word);
}

static void
change_button_clicked_handler (GtkButton               *button,
			       GeditSpellCheckerDialog *dialog)
{
	gchar *word;
	gchar *change;

	g_return_if_fail (dialog->misspelled_word != NULL);

	change = g_strdup (gtk_entry_get_text (dialog->word_entry));
	g_return_if_fail (change != NULL);
	g_return_if_fail (*change != '\0');

	gedit_spell_checker_set_correction (dialog->spell_checker,
					    dialog->misspelled_word,
					    change);

	word = g_strdup (dialog->misspelled_word);
	g_signal_emit (G_OBJECT (dialog), signals [CHANGE], 0, word, change);

	g_free (word);
	g_free (change);
}

/* double click on one of the suggestions is like clicking on "change" */
static void
suggestions_row_activated_handler (GtkTreeView             *view,
				   GtkTreePath             *path,
				   GtkTreeViewColumn       *column,
				   GeditSpellCheckerDialog *dialog)
{
	change_button_clicked_handler (GTK_BUTTON (dialog->change_button), dialog);
}

static void
change_all_button_clicked_handler (GtkButton               *button,
				   GeditSpellCheckerDialog *dialog)
{
	gchar *word;
	gchar *change;

	g_return_if_fail (dialog->misspelled_word != NULL);

	change = g_strdup (gtk_entry_get_text (dialog->word_entry));
	g_return_if_fail (change != NULL);
	g_return_if_fail (*change != '\0');

	gedit_spell_checker_set_correction (dialog->spell_checker,
					    dialog->misspelled_word,
					    change);

	word = g_strdup (dialog->misspelled_word);
	g_signal_emit (G_OBJECT (dialog), signals [CHANGE_ALL], 0, word, change);

	g_free (word);
	g_free (change);
}

void
gedit_spell_checker_dialog_set_completed (GeditSpellCheckerDialog *dialog)
{
	gchar *label;

	g_return_if_fail (GEDIT_IS_SPELL_CHECKER_DIALOG (dialog));

	label = g_strdup_printf ("<b>%s</b>", _("Completed spell checking"));
	gtk_label_set_label (dialog->misspelled_word_label, label);
	g_free (label);

	gtk_list_store_clear (GTK_LIST_STORE (dialog->suggestions_model));
	gtk_entry_set_text (dialog->word_entry, "");

	gtk_widget_set_sensitive (GTK_WIDGET (dialog->word_entry), FALSE);
	gtk_widget_set_sensitive (dialog->check_word_button, FALSE);
	gtk_widget_set_sensitive (dialog->ignore_button, FALSE);
	gtk_widget_set_sensitive (dialog->ignore_all_button, FALSE);
	gtk_widget_set_sensitive (dialog->change_button, FALSE);
	gtk_widget_set_sensitive (dialog->change_all_button, FALSE);
	gtk_widget_set_sensitive (dialog->add_word_button, FALSE);
	gtk_widget_set_sensitive (GTK_WIDGET (dialog->suggestions_view), FALSE);
}

/* ex:set ts=8 noet: */
