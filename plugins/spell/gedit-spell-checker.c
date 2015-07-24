/*
 * gedit-spell-checker.c
 * This file is part of gedit
 *
 * Copyright (C) 2002-2006 Paolo Maggi
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gedit-spell-checker.h"
#include <string.h>
#include <enchant.h>
#include "gedit-spell-utils.h"

#ifdef OS_OSX
#include "gedit-spell-osx.h"
#endif

struct _GeditSpellChecker
{
	GObject parent_instance;

	EnchantBroker *broker;
	EnchantDict *dict;
	const GeditSpellCheckerLanguage *active_lang;
};

enum
{
	PROP_0,
	PROP_LANGUAGE,
};

enum
{
	SIGNAL_ADD_WORD_TO_PERSONAL,
	SIGNAL_ADD_WORD_TO_SESSION,
	SIGNAL_SET_LANGUAGE,
	SIGNAL_CLEAR_SESSION,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GeditSpellChecker, gedit_spell_checker, G_TYPE_OBJECT)

static void
gedit_spell_checker_set_property (GObject      *object,
				  guint         prop_id,
				  const GValue *value,
				  GParamSpec   *pspec)
{
	GeditSpellChecker *spell = GEDIT_SPELL_CHECKER (object);

	switch (prop_id)
	{
		case PROP_LANGUAGE:
			gedit_spell_checker_set_language (spell, g_value_get_pointer (value));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gedit_spell_checker_get_property (GObject    *object,
				  guint       prop_id,
				  GValue     *value,
				  GParamSpec *pspec)
{
	GeditSpellChecker *spell = GEDIT_SPELL_CHECKER (object);

	switch (prop_id)
	{
		case PROP_LANGUAGE:
			g_value_set_pointer (value, (gpointer)spell->active_lang);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gedit_spell_checker_finalize (GObject *object)
{
	GeditSpellChecker *spell_checker = GEDIT_SPELL_CHECKER (object);

	if (spell_checker->dict != NULL)
	{
		enchant_broker_free_dict (spell_checker->broker, spell_checker->dict);
	}

	if (spell_checker->broker != NULL)
	{
		enchant_broker_free (spell_checker->broker);
	}

	G_OBJECT_CLASS (gedit_spell_checker_parent_class)->finalize (object);
}

static void
gedit_spell_checker_class_init (GeditSpellCheckerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->set_property = gedit_spell_checker_set_property;
	object_class->get_property = gedit_spell_checker_get_property;
	object_class->finalize = gedit_spell_checker_finalize;

	g_object_class_install_property (object_class,
					 PROP_LANGUAGE,
					 g_param_spec_pointer ("language",
							       "Language",
							       "The language used by the spell checker",
							       G_PARAM_READWRITE));

	signals[SIGNAL_ADD_WORD_TO_PERSONAL] =
	    g_signal_new ("add-word-to-personal",
			  G_OBJECT_CLASS_TYPE (object_class),
			  G_SIGNAL_RUN_LAST,
			  G_STRUCT_OFFSET (GeditSpellCheckerClass, add_word_to_personal),
			  NULL, NULL, NULL,
			  G_TYPE_NONE,
			  1,
			  G_TYPE_STRING);

	signals[SIGNAL_ADD_WORD_TO_SESSION] =
	    g_signal_new ("add-word-to-session",
			  G_OBJECT_CLASS_TYPE (object_class),
			  G_SIGNAL_RUN_LAST,
			  G_STRUCT_OFFSET (GeditSpellCheckerClass, add_word_to_session),
			  NULL, NULL, NULL,
			  G_TYPE_NONE,
			  1,
			  G_TYPE_STRING);

	signals[SIGNAL_SET_LANGUAGE] =
	    g_signal_new ("set-language",
			  G_OBJECT_CLASS_TYPE (object_class),
			  G_SIGNAL_RUN_LAST,
			  G_STRUCT_OFFSET (GeditSpellCheckerClass, set_language),
			  NULL, NULL, NULL,
			  G_TYPE_NONE,
			  1,
			  G_TYPE_POINTER);

	signals[SIGNAL_CLEAR_SESSION] =
	    g_signal_new ("clear-session",
			  G_OBJECT_CLASS_TYPE (object_class),
			  G_SIGNAL_RUN_LAST,
			  G_STRUCT_OFFSET (GeditSpellCheckerClass, clear_session),
			  NULL, NULL, NULL,
			  G_TYPE_NONE,
			  0);
}

static void
gedit_spell_checker_init (GeditSpellChecker *spell_checker)
{
	spell_checker->broker = enchant_broker_init ();
	spell_checker->dict = NULL;
	spell_checker->active_lang = NULL;
}

GeditSpellChecker *
gedit_spell_checker_new	(void)
{
	return g_object_new (GEDIT_TYPE_SPELL_CHECKER, NULL);
}

static const GeditSpellCheckerLanguage *
get_default_language (void)
{
	const GeditSpellCheckerLanguage *lang;
	const gchar * const *lang_names;
	const GSList *langs;
	gint i;

	/* Try with the current locale */
	lang_names = g_get_language_names ();

	for (i = 0; lang_names[i] != NULL; i++)
	{
		lang = gedit_spell_checker_language_from_key (lang_names[i]);

		if (lang != NULL)
		{
			return lang;
		}
	}

	/* Another try specific to Mac OS X */
#ifdef OS_OSX
	{
		gchar *key = gedit_spell_osx_get_preferred_spell_language ();

		if (key != NULL)
		{
			lang = gedit_spell_checker_language_from_key (key);
			g_free (key);
			return lang;
		}
	}
#endif

	/* Try English */
	lang = gedit_spell_checker_language_from_key ("en_US");
	if (lang != NULL)
	{
		return lang;
	}

	/* Take the first available language */
	langs = gedit_spell_checker_get_available_languages ();
	if (langs != NULL)
	{
		return langs->data;
	}

	return NULL;
}

static gboolean
lazy_init (GeditSpellChecker               *spell,
	   const GeditSpellCheckerLanguage *language)
{
	if (spell->dict != NULL)
	{
		return TRUE;
	}

	g_return_val_if_fail (spell->broker != NULL, FALSE);

	spell->active_lang = language != NULL ? language : get_default_language ();

	if (spell->active_lang != NULL)
	{
		const gchar *key;

		key = gedit_spell_checker_language_to_key (spell->active_lang);

		spell->dict = enchant_broker_request_dict (spell->broker, key);
	}

	if (spell->dict == NULL)
	{
		spell->active_lang = NULL;

		if (language != NULL)
		{
			g_warning ("Spell checker plugin: cannot select a default language.");
		}

		return FALSE;
	}

	return TRUE;
}

gboolean
gedit_spell_checker_set_language (GeditSpellChecker               *spell,
				  const GeditSpellCheckerLanguage *language)
{
	gboolean ret;

	g_return_val_if_fail (GEDIT_IS_SPELL_CHECKER (spell), FALSE);

	if (spell->dict != NULL)
	{
		enchant_broker_free_dict (spell->broker, spell->dict);
		spell->dict = NULL;
	}

	ret = lazy_init (spell, language);

	if (ret)
	{
		g_signal_emit (G_OBJECT (spell), signals[SIGNAL_SET_LANGUAGE], 0, language);
	}
	else
	{
		g_warning ("Spell checker plugin: cannot use language %s.",
		           gedit_spell_checker_language_to_string (language));
	}

	g_object_notify (G_OBJECT (spell), "language");

	return ret;
}

const GeditSpellCheckerLanguage *
gedit_spell_checker_get_language (GeditSpellChecker *spell)
{
	g_return_val_if_fail (GEDIT_IS_SPELL_CHECKER (spell), NULL);

	if (!lazy_init (spell, spell->active_lang))
	{
		return NULL;
	}

	return spell->active_lang;
}

gboolean
gedit_spell_checker_check_word (GeditSpellChecker *spell,
				const gchar       *word)
{
	gint enchant_result;
	gboolean res = FALSE;

	g_return_val_if_fail (GEDIT_IS_SPELL_CHECKER (spell), FALSE);
	g_return_val_if_fail (word != NULL, FALSE);

	if (!lazy_init (spell, spell->active_lang))
	{
		return FALSE;
	}

	if (strcmp (word, "gedit") == 0)
	{
		return TRUE;
	}

	if (gedit_spell_utils_is_digit (word))
	{
		return TRUE;
	}

	g_return_val_if_fail (spell->dict != NULL, FALSE);
	enchant_result = enchant_dict_check (spell->dict, word, -1);

	switch (enchant_result)
	{
		case -1:
			/* error */
			res = FALSE;

			g_warning ("Spell checker plugin: error checking word '%s' (%s).",
				   word, enchant_dict_get_error (spell->dict));

			break;

		case 1:
			/* it is not in the directory */
			res = FALSE;
			break;

		case 0:
			/* is is in the directory */
			res = TRUE;
			break;

		default:
			g_return_val_if_reached (FALSE);
	}

	return res;
}

/* return NULL on error or if no suggestions are found */
GSList *
gedit_spell_checker_get_suggestions (GeditSpellChecker *spell,
				     const gchar       *word)
{
	gchar **suggestions;
	GSList *suggestions_list = NULL;
	gint i;

	g_return_val_if_fail (GEDIT_IS_SPELL_CHECKER (spell), NULL);
	g_return_val_if_fail (word != NULL, NULL);

	if (!lazy_init (spell, spell->active_lang))
	{
		return NULL;
	}

	g_return_val_if_fail (spell->dict != NULL, NULL);

	suggestions = enchant_dict_suggest (spell->dict, word, -1, NULL);

	if (suggestions == NULL)
	{
		return NULL;
	}

	for (i = 0; suggestions[i] != NULL; i++)
	{
		suggestions_list = g_slist_prepend (suggestions_list, suggestions[i]);
	}

	/* The single suggestions will be freed by the caller */
	g_free (suggestions);

	return g_slist_reverse (suggestions_list);
}

gboolean
gedit_spell_checker_add_word_to_personal (GeditSpellChecker *spell,
					  const gchar       *word)
{
	g_return_val_if_fail (GEDIT_IS_SPELL_CHECKER (spell), FALSE);
	g_return_val_if_fail (word != NULL, FALSE);

	if (!lazy_init (spell, spell->active_lang))
	{
		return FALSE;
	}

	g_return_val_if_fail (spell->dict != NULL, FALSE);

	enchant_dict_add (spell->dict, word, -1);

	g_signal_emit (G_OBJECT (spell), signals[SIGNAL_ADD_WORD_TO_PERSONAL], 0, word);

	return TRUE;
}

gboolean
gedit_spell_checker_add_word_to_session (GeditSpellChecker *spell,
					 const gchar       *word)
{
	g_return_val_if_fail (GEDIT_IS_SPELL_CHECKER (spell), FALSE);
	g_return_val_if_fail (word != NULL, FALSE);

	if (!lazy_init (spell, spell->active_lang))
	{
		return FALSE;
	}

	g_return_val_if_fail (spell->dict != NULL, FALSE);

	enchant_dict_add_to_session (spell->dict, word, -1);

	g_signal_emit (G_OBJECT (spell), signals[SIGNAL_ADD_WORD_TO_SESSION], 0, word);

	return TRUE;
}

gboolean
gedit_spell_checker_clear_session (GeditSpellChecker *spell)
{
	g_return_val_if_fail (GEDIT_IS_SPELL_CHECKER (spell), FALSE);

	/* free and re-request dictionary */
	if (spell->dict != NULL)
	{
		enchant_broker_free_dict (spell->broker, spell->dict);
		spell->dict = NULL;
	}

	if (!lazy_init (spell, spell->active_lang))
	{
		return FALSE;
	}

	g_signal_emit (G_OBJECT (spell), signals[SIGNAL_CLEAR_SESSION], 0);

	return TRUE;
}

/* Informs dictionary, that @word will be replaced/corrected by @replacement. */
gboolean
gedit_spell_checker_set_correction (GeditSpellChecker *spell,
				    const gchar       *word,
				    const gchar       *replacement)
{
	g_return_val_if_fail (GEDIT_IS_SPELL_CHECKER (spell), FALSE);
	g_return_val_if_fail (word != NULL, FALSE);
	g_return_val_if_fail (replacement != NULL, FALSE);

	if (!lazy_init (spell, spell->active_lang))
	{
		return FALSE;
	}

	g_return_val_if_fail (spell->dict != NULL, FALSE);

	enchant_dict_store_replacement (spell->dict,
					word, -1,
					replacement, -1);

	return TRUE;
}

/* ex:set ts=8 noet: */
