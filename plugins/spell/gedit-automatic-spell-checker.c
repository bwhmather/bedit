/*
 * gedit-automatic-spell-checker.c
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

/* This is a modified version of gtkspell 2.0.5  (gtkspell.sf.net) */
/* gtkspell - a spell-checking addon for GTK's TextView widget
 * Copyright (c) 2002 Evan Martin.
 */

#include "gedit-automatic-spell-checker.h"
#include <string.h>
#include <glib/gi18n.h>
#include "gedit-spell-utils.h"

struct _GeditAutomaticSpellChecker
{
	GtkTextBuffer *buffer;

	/* List of GtkTextView* */
	GSList *views;

	GtkTextMark *mark_insert_start;
	GtkTextMark *mark_insert_end;

	GtkTextTag *tag_highlight;
	GtkTextMark *mark_click;

	GeditSpellChecker *spell_checker;

	guint deferred_check : 1;
};

#define AUTOMATIC_SPELL_CHECKER_KEY	"GeditAutomaticSpellCheckerID"
#define SUGGESTION_KEY			"GeditAutoSuggestionID"

static void gedit_automatic_spell_checker_free_internal (GeditAutomaticSpellChecker *spell);

static void
view_destroy (GtkTextView                *view,
	      GeditAutomaticSpellChecker *spell)
{
	gedit_automatic_spell_checker_detach_view (spell, view);
}

static void
check_word (GeditAutomaticSpellChecker *spell,
	    const GtkTextIter          *start,
	    const GtkTextIter          *end)
{
	gchar *word;

	word = gtk_text_buffer_get_text (spell->buffer, start, end, FALSE);

	if (!gedit_spell_checker_check_word (spell->spell_checker, word, -1))
	{
		gtk_text_buffer_apply_tag (spell->buffer,
					   spell->tag_highlight,
					   start,
					   end);
	}

	g_free (word);
}

static void
adjust_iters_at_word_boundaries (GtkTextIter *start,
				 GtkTextIter *end)
{
	if (gtk_text_iter_inside_word (end))
	{
		gtk_text_iter_forward_word_end (end);
	}

	if (!gtk_text_iter_starts_word (start))
	{
		if (gtk_text_iter_inside_word (start) ||
		    gtk_text_iter_ends_word (start))
		{
			gtk_text_iter_backward_word_start (start);
		}
		else
		{
			/* If we're neither at the beginning nor inside a word,
			 * me must be in some spaces.
			 * Skip forward to the beginning of the next word.
			 */
			if (gtk_text_iter_forward_word_end (start) ||
			    gtk_text_iter_is_end (start))
			{
				gtk_text_iter_backward_word_start (start);
			}
		}
	}
}

static void
check_range (GeditAutomaticSpellChecker *spell,
	     const GtkTextIter          *start,
	     const GtkTextIter          *end,
	     gboolean                    force_all)
{
	GtkTextIter cursor;
	GtkTextIter precursor;
	GtkTextIter start_adjusted;
	GtkTextIter end_adjusted;
	GtkTextIter word_start;
  	gboolean highlight;

	gtk_text_buffer_get_iter_at_mark (spell->buffer,
					  &cursor,
					  gtk_text_buffer_get_insert (spell->buffer));

	precursor = cursor;
	gtk_text_iter_backward_char (&precursor);

	highlight = (gtk_text_iter_has_tag (&cursor, spell->tag_highlight) ||
		     gtk_text_iter_has_tag (&precursor, spell->tag_highlight));

	start_adjusted = *start;
	end_adjusted = *end;
	adjust_iters_at_word_boundaries (&start_adjusted, &end_adjusted);

	gtk_text_buffer_remove_tag (spell->buffer,
				    spell->tag_highlight,
				    &start_adjusted,
				    &end_adjusted);

	word_start = start_adjusted;

	while (gedit_spell_utils_skip_no_spell_check (&word_start, &end_adjusted) &&
	       gtk_text_iter_compare (&word_start, &end_adjusted) < 0)
	{
		GtkTextIter word_end;
		GtkTextIter next_word_start;
		gboolean in_word;

		word_end = word_start;
		gtk_text_iter_forward_word_end (&word_end);

		in_word = (gtk_text_iter_compare (&word_start, &cursor) < 0 &&
			   gtk_text_iter_compare (&cursor, &word_end) <= 0);

		if (in_word && !force_all)
		{
			/* This word is being actively edited,
			 * only check if it's already highligted,
			 * otherwise defer this check until later.
			 */
			if (highlight)
			{
				check_word (spell, &word_start, &word_end);
			}
			else
			{
				spell->deferred_check = TRUE;
			}
		}
		else
		{
			check_word (spell, &word_start, &word_end);
			spell->deferred_check = FALSE;
		}

		next_word_start = word_end;
		gtk_text_iter_forward_word_end (&next_word_start);
		gtk_text_iter_backward_word_start (&next_word_start);

		/* Make sure we've actually advanced (we don't advance if we are
		 * at the end of the buffer).
		 */
		if (gtk_text_iter_compare (&next_word_start, &word_start) <= 0)
		{
			break;
		}

		word_start = next_word_start;
	}
}

static void
check_deferred_range (GeditAutomaticSpellChecker *spell,
		      gboolean                    force_all)
{
	GtkTextIter start, end;

	gtk_text_buffer_get_iter_at_mark (spell->buffer,
					  &start,
					  spell->mark_insert_start);

	gtk_text_buffer_get_iter_at_mark (spell->buffer,
					  &end,
					  spell->mark_insert_end);

	check_range (spell, &start, &end, force_all);
}

/* insertion works like this:
 *  - before the text is inserted, we mark the position in the buffer.
 *  - after the text is inserted, we see where our mark is and use that and
 *    the current position to check the entire range of inserted text.
 *
 * this may be overkill for the common case (inserting one character). */

static void
insert_text_before (GtkTextBuffer              *buffer,
		    GtkTextIter                *iter,
		    gchar                      *text,
		    gint                        len,
		    GeditAutomaticSpellChecker *spell)
{
	gtk_text_buffer_move_mark (buffer, spell->mark_insert_start, iter);
}

static void
insert_text_after (GtkTextBuffer              *buffer,
		   GtkTextIter                *iter,
		   gchar                      *text,
		   gint                        len,
		   GeditAutomaticSpellChecker *spell)
{
	GtkTextIter start;

	/* we need to check a range of text. */
	gtk_text_buffer_get_iter_at_mark (buffer, &start, spell->mark_insert_start);

	check_range (spell, &start, iter, FALSE);

	gtk_text_buffer_move_mark (buffer, spell->mark_insert_end, iter);
}

/* deleting is more simple:  we're given the range of deleted text.
 * after deletion, the start and end iters should be at the same position
 * (because all of the text between them was deleted!).
 * this means we only really check the words immediately bounding the
 * deletion.
 */

static void
delete_range_after (GtkTextBuffer              *buffer,
		    GtkTextIter                *start,
		    GtkTextIter                *end,
		    GeditAutomaticSpellChecker *spell)
{
	check_range (spell, start, end, FALSE);
}

static void
mark_set (GtkTextBuffer              *buffer,
	  GtkTextIter                *iter,
	  GtkTextMark                *mark,
	  GeditAutomaticSpellChecker *spell)
{
	/* if the cursor has moved and there is a deferred check so handle it now */
	if ((mark == gtk_text_buffer_get_insert (buffer)) && spell->deferred_check)
	{
		check_deferred_range (spell, FALSE);
	}
}

static void
get_word_extents_from_mark (GtkTextBuffer *buffer,
			    GtkTextIter   *start,
			    GtkTextIter   *end,
			    GtkTextMark   *mark)
{
	gtk_text_buffer_get_iter_at_mark (buffer, start, mark);

	if (!gtk_text_iter_starts_word (start))
	{
		gtk_text_iter_backward_word_start (start);
	}

	*end = *start;

	if (gtk_text_iter_inside_word (end))
	{
		gtk_text_iter_forward_word_end (end);
	}
}

static void
remove_tag_to_word (GeditAutomaticSpellChecker *spell,
		    const gchar                *word)
{
	GtkTextIter iter;

	gtk_text_buffer_get_start_iter (spell->buffer, &iter);

	while (TRUE)
	{
		gboolean found;
		GtkTextIter match_start;
		GtkTextIter match_end;

		found = gtk_text_iter_forward_search (&iter,
						      word,
						      GTK_TEXT_SEARCH_VISIBLE_ONLY |
						      GTK_TEXT_SEARCH_TEXT_ONLY,
						      &match_start,
						      &match_end,
						      NULL);

		if (!found)
		{
			break;
		}

		if (gtk_text_iter_starts_word (&match_start) &&
		    gtk_text_iter_ends_word (&match_end))
		{
			gtk_text_buffer_remove_tag (spell->buffer,
						    spell->tag_highlight,
						    &match_start,
						    &match_end);
		}

		iter = match_end;
	}
}

static void
add_to_dictionary (GtkWidget                  *menu_item,
		   GeditAutomaticSpellChecker *spell)
{
	GtkTextIter start;
	GtkTextIter end;
	gchar *word;

	get_word_extents_from_mark (spell->buffer, &start, &end, spell->mark_click);

	word = gtk_text_buffer_get_text (spell->buffer, &start, &end, FALSE);

	gedit_spell_checker_add_word_to_personal (spell->spell_checker, word, -1);

	g_free (word);
}

static void
ignore_all (GtkWidget                  *menu_item,
	    GeditAutomaticSpellChecker *spell)
{
	GtkTextIter start;
	GtkTextIter end;
	gchar *word;

	get_word_extents_from_mark (spell->buffer, &start, &end, spell->mark_click);

	word = gtk_text_buffer_get_text (spell->buffer, &start, &end, FALSE);

	gedit_spell_checker_add_word_to_session (spell->spell_checker, word, -1);

	g_free (word);
}

static void
replace_word (GtkWidget                  *menu_item,
	      GeditAutomaticSpellChecker *spell)
{
	GtkTextIter start;
	GtkTextIter end;
	gchar *old_word;
	const gchar *new_word;

	get_word_extents_from_mark (spell->buffer, &start, &end, spell->mark_click);

	old_word = gtk_text_buffer_get_text (spell->buffer, &start, &end, FALSE);

	new_word = g_object_get_data (G_OBJECT (menu_item), SUGGESTION_KEY);
	g_return_if_fail (new_word != NULL);

	gtk_text_buffer_begin_user_action (spell->buffer);

	gtk_text_buffer_delete (spell->buffer, &start, &end);
	gtk_text_buffer_insert (spell->buffer, &start, new_word, -1);

	gtk_text_buffer_end_user_action (spell->buffer);

	gedit_spell_checker_set_correction (spell->spell_checker,
					    old_word, strlen (old_word),
					    new_word, strlen (new_word));

	g_free (old_word);
}

static GtkWidget *
build_suggestion_menu (GeditAutomaticSpellChecker *spell,
		       const gchar                *word)
{
	GtkWidget *top_menu;
	GtkWidget *menu_item;
	GSList *suggestions;

	top_menu = gtk_menu_new ();

	suggestions = gedit_spell_checker_get_suggestions (spell->spell_checker, word, -1);

	if (suggestions == NULL)
	{
		/* No suggestions. Put something in the menu anyway... */
		menu_item = gtk_menu_item_new_with_label (_("(no suggested words)"));
		gtk_widget_set_sensitive (menu_item, FALSE);
		gtk_menu_shell_prepend (GTK_MENU_SHELL (top_menu), menu_item);
	}
	else
	{
		GtkWidget *menu = top_menu;
		gint count = 0;
		GSList *l;

		/* Build a set of menus with suggestions. */
		for (l = suggestions; l != NULL; l = l->next)
		{
			gchar *suggested_word = l->data;
			GtkWidget *label;
			gchar *label_text;

			if (count == 10)
			{
				/* Separator */
				menu_item = gtk_separator_menu_item_new ();
				gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

				menu_item = gtk_menu_item_new_with_mnemonic (_("_More..."));
				gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

				menu = gtk_menu_new ();
				gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu_item), menu);
				count = 0;
			}

			label_text = g_strdup_printf ("<b>%s</b>", suggested_word);

			label = gtk_label_new (label_text);
			gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
			gtk_widget_set_halign (label, GTK_ALIGN_START);

			menu_item = gtk_menu_item_new ();
			gtk_container_add (GTK_CONTAINER (menu_item), label);
			gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

			g_object_set_data_full (G_OBJECT (menu_item),
						SUGGESTION_KEY,
						g_strdup (suggested_word),
						g_free);

			g_signal_connect (menu_item,
					  "activate",
					  G_CALLBACK (replace_word),
					  spell);

			g_free (label_text);
			count++;
		}
	}

	g_slist_free_full (suggestions, g_free);

	/* Separator */
	menu_item = gtk_separator_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (top_menu), menu_item);

	/* Ignore all */
	menu_item = gtk_menu_item_new_with_mnemonic (_("_Ignore All"));
	gtk_menu_shell_append (GTK_MENU_SHELL (top_menu), menu_item);

	g_signal_connect (menu_item,
			  "activate",
			  G_CALLBACK (ignore_all),
			  spell);

	/* Add to Dictionary */
	menu_item = gtk_menu_item_new_with_mnemonic (_("_Add"));
	gtk_menu_shell_append (GTK_MENU_SHELL (top_menu), menu_item);

	g_signal_connect (menu_item,
			  "activate",
			  G_CALLBACK (add_to_dictionary),
			  spell);

	return top_menu;
}

static void
populate_popup (GtkTextView                *view,
		GtkMenu                    *menu,
		GeditAutomaticSpellChecker *spell)
{
	GtkWidget *menu_item;
	GtkTextIter start;
	GtkTextIter end;
	gchar *word;

	get_word_extents_from_mark (spell->buffer, &start, &end, spell->mark_click);

	if (!gtk_text_iter_has_tag (&start, spell->tag_highlight))
	{
		return;
	}

	/* Prepend separator */
	menu_item = gtk_separator_menu_item_new ();
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), menu_item);
	gtk_widget_show (menu_item);

	/* Prepend suggestions */
	menu_item = gtk_menu_item_new_with_mnemonic (_("_Spelling Suggestions..."));
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), menu_item);

	word = gtk_text_buffer_get_text (spell->buffer, &start, &end, FALSE);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu_item),
				   build_suggestion_menu (spell, word));
	g_free (word);

	gtk_widget_show_all (menu_item);
}

void
gedit_automatic_spell_checker_recheck_all (GeditAutomaticSpellChecker *spell)
{
	GtkTextIter start;
	GtkTextIter end;

	g_return_if_fail (spell != NULL);

	gtk_text_buffer_get_bounds (spell->buffer, &start, &end);

	check_range (spell, &start, &end, TRUE);
}

static void
add_word_signal_cb (GeditSpellChecker          *checker,
		    const gchar                *word,
		    gint                        len,
		    GeditAutomaticSpellChecker *spell)
{
	gchar *w;

	w = len < 0 ? g_strdup (word) : g_strndup (word, len);

	remove_tag_to_word (spell, w);

	g_free (w);
}

static void
set_language_cb (GeditSpellChecker               *checker,
		 const GeditSpellCheckerLanguage *lang,
		 GeditAutomaticSpellChecker      *spell)
{
	gedit_automatic_spell_checker_recheck_all (spell);
}

static void
clear_session_cb (GeditSpellChecker          *checker,
		  GeditAutomaticSpellChecker *spell)
{
	gedit_automatic_spell_checker_recheck_all (spell);
}

/* When the user right-clicks on a word, they want to check that word.
 * Here, we do NOT move the cursor to the location of the clicked-upon word
 * since that prevents the use of edit functions on the context menu.
 */
static gboolean
button_press_event (GtkTextView                *view,
		    GdkEventButton             *event,
		    GeditAutomaticSpellChecker *spell)
{
	if (event->button == GDK_BUTTON_SECONDARY)
	{
		GtkTextBuffer *buffer = gtk_text_view_get_buffer (view);
		GtkTextIter iter;
		gint x;
		gint y;

		if (spell->deferred_check)
		{
			check_deferred_range (spell, TRUE);
		}

		gtk_text_view_window_to_buffer_coords (view,
						       GTK_TEXT_WINDOW_TEXT,
						       event->x, event->y,
						       &x, &y);

		gtk_text_view_get_iter_at_location (view, &iter, x, y);

		gtk_text_buffer_move_mark (buffer, spell->mark_click, &iter);
	}

	return GDK_EVENT_PROPAGATE;
}

/* Move the insert mark before popping up the menu, otherwise it
 * will contain the wrong set of suggestions.
 */
static gboolean
popup_menu_event (GtkTextView                *view,
		  GeditAutomaticSpellChecker *spell)
{
	GtkTextIter iter;
	GtkTextBuffer *buffer;

	buffer = gtk_text_view_get_buffer (view);

	if (spell->deferred_check)
	{
		check_deferred_range (spell, TRUE);
	}

	gtk_text_buffer_get_iter_at_mark (buffer, &iter,
					  gtk_text_buffer_get_insert (buffer));
	gtk_text_buffer_move_mark (buffer, spell->mark_click, &iter);

	return FALSE;
}

static void
tag_table_changed (GtkTextTagTable            *table,
		   GeditAutomaticSpellChecker *spell)
{
	g_return_if_fail (spell->tag_highlight != NULL);

	gtk_text_tag_set_priority (spell->tag_highlight,
				   gtk_text_tag_table_get_size (table) - 1);
}

static void
tag_added_or_removed (GtkTextTagTable            *table,
		      GtkTextTag                 *tag,
		      GeditAutomaticSpellChecker *spell)
{
	tag_table_changed (table, spell);
}

static void
tag_changed (GtkTextTagTable            *table,
	     GtkTextTag                 *tag,
	     gboolean                    size_changed,
	     GeditAutomaticSpellChecker *spell)
{
	tag_table_changed (table, spell);
}

static void
highlight_updated (GtkSourceBuffer            *buffer,
		   GtkTextIter                *start,
		   GtkTextIter                *end,
		   GeditAutomaticSpellChecker *spell)
{
	check_range (spell, start, end, FALSE);
}

static void
spell_tag_destroyed (GeditAutomaticSpellChecker *spell,
		     GObject                    *where_the_object_was)
{
	spell->tag_highlight = NULL;
}

GeditAutomaticSpellChecker *
gedit_automatic_spell_checker_new (GtkSourceBuffer   *buffer,
				   GeditSpellChecker *checker)
{
	GeditAutomaticSpellChecker *spell;
	GtkTextTagTable *tag_table;
	GtkTextIter start;
	GtkTextIter end;

	g_return_val_if_fail (GTK_SOURCE_IS_BUFFER (buffer), NULL);
	g_return_val_if_fail (GEDIT_IS_SPELL_CHECKER (checker), NULL);

	spell = gedit_automatic_spell_checker_get_from_buffer (buffer);
	g_return_val_if_fail (spell == NULL, spell);

	/* attach to the widget */
	spell = g_new0 (GeditAutomaticSpellChecker, 1);

	spell->buffer = GTK_TEXT_BUFFER (buffer);
	spell->spell_checker = g_object_ref (checker);

	g_object_set_data_full (G_OBJECT (buffer),
				AUTOMATIC_SPELL_CHECKER_KEY,
				spell,
				(GDestroyNotify) gedit_automatic_spell_checker_free_internal);

	g_signal_connect (buffer,
			  "insert-text",
			  G_CALLBACK (insert_text_before),
			  spell);

	g_signal_connect_after (buffer,
				"insert-text",
				G_CALLBACK (insert_text_after),
				spell);

	g_signal_connect_after (buffer,
				"delete-range",
				G_CALLBACK (delete_range_after),
				spell);

	g_signal_connect (buffer,
			  "mark-set",
			  G_CALLBACK (mark_set),
			  spell);

	g_signal_connect (buffer,
	                  "highlight-updated", /* GtkSourceBuffer signal */
	                  G_CALLBACK (highlight_updated),
	                  spell);

	g_signal_connect (spell->spell_checker,
			  "add_word_to_session",
			  G_CALLBACK (add_word_signal_cb),
			  spell);

	g_signal_connect (spell->spell_checker,
			  "add_word_to_personal",
			  G_CALLBACK (add_word_signal_cb),
			  spell);

	g_signal_connect (spell->spell_checker,
			  "clear_session",
			  G_CALLBACK (clear_session_cb),
			  spell);

	g_signal_connect (spell->spell_checker,
			  "set_language",
			  G_CALLBACK (set_language_cb),
			  spell);

	spell->tag_highlight = gtk_text_buffer_create_tag (spell->buffer,
							   "gedit-spell-misspelled",
							   "underline", PANGO_UNDERLINE_ERROR,
							   NULL);

	g_object_weak_ref (G_OBJECT (spell->tag_highlight),
	                   (GWeakNotify)spell_tag_destroyed,
	                   spell);

	tag_table = gtk_text_buffer_get_tag_table (spell->buffer);

	gtk_text_tag_set_priority (spell->tag_highlight,
				   gtk_text_tag_table_get_size (tag_table) - 1);

	g_signal_connect (tag_table,
			  "tag-added",
			  G_CALLBACK (tag_added_or_removed),
			  spell);

	g_signal_connect (tag_table,
			  "tag-removed",
			  G_CALLBACK (tag_added_or_removed),
			  spell);

	g_signal_connect (tag_table,
			  "tag-changed",
			  G_CALLBACK (tag_changed),
			  spell);

	/* We create the mark here, but we don't use it until text is
	 * inserted, so we don't really care where iter points.
	 */
	gtk_text_buffer_get_bounds (spell->buffer, &start, &end);

	spell->mark_insert_start = gtk_text_buffer_get_mark (spell->buffer,
							     "gedit-automatic-spell-checker-insert-start");

	if (spell->mark_insert_start == NULL)
	{
		spell->mark_insert_start =
			gtk_text_buffer_create_mark (spell->buffer,
						     "gedit-automatic-spell-checker-insert-start",
						     &start,
						     TRUE);
	}
	else
	{
		gtk_text_buffer_move_mark (spell->buffer,
					   spell->mark_insert_start,
					   &start);
	}

	spell->mark_insert_end = gtk_text_buffer_get_mark (spell->buffer,
							   "gedit-automatic-spell-checker-insert-end");

	if (spell->mark_insert_end == NULL)
	{
		spell->mark_insert_end = gtk_text_buffer_create_mark (spell->buffer,
								      "gedit-automatic-spell-checker-insert-end",
								      &start,
								      TRUE);
	}
	else
	{
		gtk_text_buffer_move_mark (spell->buffer,
					   spell->mark_insert_end,
					   &start);
	}

	spell->mark_click = gtk_text_buffer_get_mark (spell->buffer,
						      "gedit-automatic-spell-checker-click");

	if (spell->mark_click == NULL)
	{
		spell->mark_click = gtk_text_buffer_create_mark (spell->buffer,
								 "gedit-automatic-spell-checker-click",
								 &start,
								 TRUE);
	}
	else
	{
		gtk_text_buffer_move_mark (spell->buffer,
					   spell->mark_click,
					   &start);
	}

	spell->deferred_check = FALSE;

	return spell;
}

GeditAutomaticSpellChecker *
gedit_automatic_spell_checker_get_from_buffer (GtkSourceBuffer *buffer)
{
	g_return_val_if_fail (GTK_SOURCE_IS_BUFFER (buffer), NULL);

	return g_object_get_data (G_OBJECT (buffer), AUTOMATIC_SPELL_CHECKER_KEY);
}

void
gedit_automatic_spell_checker_free (GeditAutomaticSpellChecker *spell)
{
	g_return_if_fail (spell != NULL);
	g_return_if_fail (gedit_automatic_spell_checker_get_from_buffer (GTK_SOURCE_BUFFER (spell->buffer)) == spell);

	g_object_set_data (G_OBJECT (spell->buffer), AUTOMATIC_SPELL_CHECKER_KEY, NULL);
}

static void
gedit_automatic_spell_checker_free_internal (GeditAutomaticSpellChecker *spell)
{
	GtkTextTagTable *table;
	GSList *l;

	g_return_if_fail (spell != NULL);

	table = gtk_text_buffer_get_tag_table (spell->buffer);

	if (table != NULL && spell->tag_highlight != NULL)
	{
		GtkTextIter start;
		GtkTextIter end;

		gtk_text_buffer_get_bounds (spell->buffer, &start, &end);

		gtk_text_buffer_remove_tag (spell->buffer,
					    spell->tag_highlight,
					    &start,
					    &end);

		g_signal_handlers_disconnect_matched (table,
						      G_SIGNAL_MATCH_DATA,
						      0, 0, NULL, NULL,
						      spell);

		gtk_text_tag_table_remove (table, spell->tag_highlight);
	}

	g_signal_handlers_disconnect_matched (spell->buffer,
					      G_SIGNAL_MATCH_DATA,
					      0, 0, NULL, NULL,
					      spell);

	g_signal_handlers_disconnect_matched (spell->spell_checker,
					      G_SIGNAL_MATCH_DATA,
					      0, 0, NULL, NULL,
					      spell);

	g_object_unref (spell->spell_checker);

	for (l = spell->views; l != NULL; l = l->next)
	{
		GtkTextView *view = GTK_TEXT_VIEW (l->data);

		g_signal_handlers_disconnect_matched (view,
						      G_SIGNAL_MATCH_DATA,
						      0, 0, NULL, NULL,
						      spell);
	}

	g_slist_free (spell->views);
	g_free (spell);
}

void
gedit_automatic_spell_checker_attach_view (GeditAutomaticSpellChecker *spell,
					   GtkTextView                *view)
{
	g_return_if_fail (spell != NULL);
	g_return_if_fail (GTK_IS_TEXT_VIEW (view));
	g_return_if_fail (gtk_text_view_get_buffer (view) == spell->buffer);

	g_signal_connect (view,
			  "button-press-event",
			  G_CALLBACK (button_press_event),
			  spell);

	g_signal_connect (view,
			  "popup-menu",
			  G_CALLBACK (popup_menu_event),
			  spell);

	g_signal_connect (view,
			  "populate-popup",
			  G_CALLBACK (populate_popup),
			  spell);

	g_signal_connect (view,
			  "destroy",
			  G_CALLBACK (view_destroy),
			  spell);

	spell->views = g_slist_prepend (spell->views, view);
}

void
gedit_automatic_spell_checker_detach_view (GeditAutomaticSpellChecker *spell,
					   GtkTextView                *view)
{
	g_return_if_fail (spell != NULL);
	g_return_if_fail (GTK_IS_TEXT_VIEW (view));
	g_return_if_fail (gtk_text_view_get_buffer (view) == spell->buffer);
	g_return_if_fail (spell->views != NULL);

	g_signal_handlers_disconnect_matched (view,
					      G_SIGNAL_MATCH_DATA,
					      0, 0, NULL, NULL,
					      spell);

	spell->views = g_slist_remove (spell->views, view);
}

/* ex:set ts=8 noet: */
