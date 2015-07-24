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
#include <enchant.h>
#include "gedit-spell-utils.h"

#ifdef OS_OSX
#include "gedit-spell-osx.h"
#endif

typedef struct _GeditSpellCheckerPrivate GeditSpellCheckerPrivate;

struct _GeditSpellCheckerPrivate
{
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
	SIGNAL_CLEAR_SESSION,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (GeditSpellChecker, gedit_spell_checker, G_TYPE_OBJECT)

static void
gedit_spell_checker_set_property (GObject      *object,
				  guint         prop_id,
				  const GValue *value,
				  GParamSpec   *pspec)
{
	GeditSpellChecker *checker = GEDIT_SPELL_CHECKER (object);

	switch (prop_id)
	{
		case PROP_LANGUAGE:
			gedit_spell_checker_set_language (checker, g_value_get_pointer (value));
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
	GeditSpellCheckerPrivate *priv;

	priv = gedit_spell_checker_get_instance_private (GEDIT_SPELL_CHECKER (object));

	switch (prop_id)
	{
		case PROP_LANGUAGE:
			g_value_set_pointer (value, (gpointer)priv->active_lang);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gedit_spell_checker_finalize (GObject *object)
{
	GeditSpellCheckerPrivate *priv;

	priv = gedit_spell_checker_get_instance_private (GEDIT_SPELL_CHECKER (object));

	if (priv->dict != NULL)
	{
		enchant_broker_free_dict (priv->broker, priv->dict);
	}

	if (priv->broker != NULL)
	{
		enchant_broker_free (priv->broker);
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
							       G_PARAM_READWRITE |
							       G_PARAM_CONSTRUCT |
							       G_PARAM_STATIC_STRINGS));

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
gedit_spell_checker_init (GeditSpellChecker *checker)
{
	GeditSpellCheckerPrivate *priv;

	priv = gedit_spell_checker_get_instance_private (checker);

	priv->broker = enchant_broker_init ();
	priv->dict = NULL;
	priv->active_lang = NULL;
}

GeditSpellChecker *
gedit_spell_checker_new	(const GeditSpellCheckerLanguage *language)
{
	return g_object_new (GEDIT_TYPE_SPELL_CHECKER,
			     "language", language,
			     NULL);
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
init_dictionary (GeditSpellChecker *checker)
{
	GeditSpellCheckerPrivate *priv;
	const gchar *app_name;

	priv = gedit_spell_checker_get_instance_private (checker);

	g_return_val_if_fail (priv->broker != NULL, FALSE);

	if (priv->dict != NULL)
	{
		return TRUE;
	}

	if (priv->active_lang == NULL)
	{
		priv->active_lang = get_default_language ();
	}

	if (priv->active_lang != NULL)
	{
		const gchar *key;

		key = gedit_spell_checker_language_to_key (priv->active_lang);

		priv->dict = enchant_broker_request_dict (priv->broker, key);
	}

	if (priv->dict == NULL)
	{
		priv->active_lang = NULL;
		return FALSE;
	}

	app_name = g_get_application_name ();
	gedit_spell_checker_add_word_to_session (checker, app_name);

	return TRUE;
}

/* If @language is %NULL, find the best available language based on the current
 * locale.
 */
gboolean
gedit_spell_checker_set_language (GeditSpellChecker               *checker,
				  const GeditSpellCheckerLanguage *language)
{
	GeditSpellCheckerPrivate *priv;
	gboolean ret;

	g_return_val_if_fail (GEDIT_IS_SPELL_CHECKER (checker), FALSE);

	priv = gedit_spell_checker_get_instance_private (checker);

	if (language != NULL && priv->active_lang == language)
	{
		return TRUE;
	}

	if (priv->dict != NULL)
	{
		enchant_broker_free_dict (priv->broker, priv->dict);
		priv->dict = NULL;
	}

	priv->active_lang = language;
	ret = init_dictionary (checker);

	if (!ret)
	{
		g_warning ("Spell checker plugin: cannot use language %s.",
		           gedit_spell_checker_language_to_string (language));
	}

	g_object_notify (G_OBJECT (checker), "language");

	return ret;
}

const GeditSpellCheckerLanguage *
gedit_spell_checker_get_language (GeditSpellChecker *checker)
{
	GeditSpellCheckerPrivate *priv;

	g_return_val_if_fail (GEDIT_IS_SPELL_CHECKER (checker), NULL);

	priv = gedit_spell_checker_get_instance_private (checker);

	return priv->active_lang;
}

gboolean
gedit_spell_checker_check_word (GeditSpellChecker *checker,
				const gchar       *word)
{
	GeditSpellCheckerPrivate *priv;
	gint enchant_result;
	gboolean res = FALSE;

	g_return_val_if_fail (GEDIT_IS_SPELL_CHECKER (checker), FALSE);
	g_return_val_if_fail (word != NULL, FALSE);

	priv = gedit_spell_checker_get_instance_private (checker);

	if (priv->dict == NULL)
	{
		return FALSE;
	}

	if (gedit_spell_utils_is_digit (word))
	{
		return TRUE;
	}

	enchant_result = enchant_dict_check (priv->dict, word, -1);

	switch (enchant_result)
	{
		case -1:
			/* error */
			res = FALSE;

			g_warning ("Spell checker plugin: error checking word '%s' (%s).",
				   word, enchant_dict_get_error (priv->dict));

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
gedit_spell_checker_get_suggestions (GeditSpellChecker *checker,
				     const gchar       *word)
{
	GeditSpellCheckerPrivate *priv;
	gchar **suggestions;
	GSList *suggestions_list = NULL;
	gint i;

	g_return_val_if_fail (GEDIT_IS_SPELL_CHECKER (checker), NULL);
	g_return_val_if_fail (word != NULL, NULL);

	priv = gedit_spell_checker_get_instance_private (checker);

	if (priv->dict == NULL)
	{
		return NULL;
	}

	suggestions = enchant_dict_suggest (priv->dict, word, -1, NULL);

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
gedit_spell_checker_add_word_to_personal (GeditSpellChecker *checker,
					  const gchar       *word)
{
	GeditSpellCheckerPrivate *priv;

	g_return_val_if_fail (GEDIT_IS_SPELL_CHECKER (checker), FALSE);
	g_return_val_if_fail (word != NULL, FALSE);

	priv = gedit_spell_checker_get_instance_private (checker);

	if (priv->dict == NULL)
	{
		return FALSE;
	}

	enchant_dict_add (priv->dict, word, -1);

	g_signal_emit (G_OBJECT (checker), signals[SIGNAL_ADD_WORD_TO_PERSONAL], 0, word);

	return TRUE;
}

gboolean
gedit_spell_checker_add_word_to_session (GeditSpellChecker *checker,
					 const gchar       *word)
{
	GeditSpellCheckerPrivate *priv;

	g_return_val_if_fail (GEDIT_IS_SPELL_CHECKER (checker), FALSE);
	g_return_val_if_fail (word != NULL, FALSE);

	priv = gedit_spell_checker_get_instance_private (checker);

	if (priv->dict == NULL)
	{
		return FALSE;
	}

	enchant_dict_add_to_session (priv->dict, word, -1);

	g_signal_emit (G_OBJECT (checker), signals[SIGNAL_ADD_WORD_TO_SESSION], 0, word);

	return TRUE;
}

gboolean
gedit_spell_checker_clear_session (GeditSpellChecker *checker)
{
	GeditSpellCheckerPrivate *priv;

	g_return_val_if_fail (GEDIT_IS_SPELL_CHECKER (checker), FALSE);

	priv = gedit_spell_checker_get_instance_private (checker);

	/* free and re-request dictionary */
	if (priv->dict != NULL)
	{
		enchant_broker_free_dict (priv->broker, priv->dict);
		priv->dict = NULL;
	}

	if (!init_dictionary (checker))
	{
		return FALSE;
	}

	g_signal_emit (G_OBJECT (checker), signals[SIGNAL_CLEAR_SESSION], 0);

	return TRUE;
}

/* Informs dictionary, that @word will be replaced/corrected by @replacement. */
gboolean
gedit_spell_checker_set_correction (GeditSpellChecker *checker,
				    const gchar       *word,
				    const gchar       *replacement)
{
	GeditSpellCheckerPrivate *priv;

	g_return_val_if_fail (GEDIT_IS_SPELL_CHECKER (checker), FALSE);
	g_return_val_if_fail (word != NULL, FALSE);
	g_return_val_if_fail (replacement != NULL, FALSE);

	priv = gedit_spell_checker_get_instance_private (checker);

	if (priv->dict == NULL)
	{
		return FALSE;
	}

	enchant_dict_store_replacement (priv->dict,
					word, -1,
					replacement, -1);

	return TRUE;
}

/* ex:set ts=8 noet: */
