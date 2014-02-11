/*
 * gedit-highlight-mode-selector.h
 * This file is part of gedit
 *
 * Copyright (C) 2013 - Ignacio Casal Quinteiro
 *
 * gedit is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * gedit is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with gedit. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GEDIT_HIGHLIGHT_MODE_SELECTOR_H__
#define __GEDIT_HIGHLIGHT_MODE_SELECTOR_H__

#include <glib-object.h>
#include <gtksourceview/gtksource.h>
#include "gedit-window.h"

G_BEGIN_DECLS

#define GEDIT_TYPE_HIGHLIGHT_MODE_SELECTOR		(gedit_highlight_mode_selector_get_type ())
#define GEDIT_HIGHLIGHT_MODE_SELECTOR(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GEDIT_TYPE_HIGHLIGHT_MODE_SELECTOR, GeditHighlightModeSelector))
#define GEDIT_HIGHLIGHT_MODE_SELECTOR_CONST(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GEDIT_TYPE_HIGHLIGHT_MODE_SELECTOR, GeditHighlightModeSelector const))
#define GEDIT_HIGHLIGHT_MODE_SELECTOR_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), GEDIT_TYPE_HIGHLIGHT_MODE_SELECTOR, GeditHighlightModeSelectorClass))
#define GEDIT_IS_HIGHLIGHT_MODE_SELECTOR(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GEDIT_TYPE_HIGHLIGHT_MODE_SELECTOR))
#define GEDIT_IS_HIGHLIGHT_MODE_SELECTOR_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GEDIT_TYPE_HIGHLIGHT_MODE_SELECTOR))
#define GEDIT_HIGHLIGHT_MODE_SELECTOR_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), GEDIT_TYPE_HIGHLIGHT_MODE_SELECTOR, GeditHighlightModeSelectorClass))

typedef struct _GeditHighlightModeSelector	GeditHighlightModeSelector;
typedef struct _GeditHighlightModeSelectorClass	GeditHighlightModeSelectorClass;
typedef struct _GeditHighlightModeSelectorPrivate	GeditHighlightModeSelectorPrivate;

struct _GeditHighlightModeSelector
{
	GtkGrid parent;

	GeditHighlightModeSelectorPrivate *priv;
};

struct _GeditHighlightModeSelectorClass
{
	GtkGridClass parent_class;

	void (* language_selected) (GeditHighlightModeSelector *widget,
	                            GtkSourceLanguage          *language);
};

GType                       gedit_highlight_mode_selector_get_type        (void) G_GNUC_CONST;

GeditHighlightModeSelector *gedit_highlight_mode_selector_new             (void);

void                        gedit_highlight_mode_selector_select_language (GeditHighlightModeSelector *selector,
                                                                           GtkSourceLanguage          *language);

void                        gedit_highlight_mode_selector_activate_selected_language
                                                                          (GeditHighlightModeSelector *selector);

G_END_DECLS

#endif /* __GEDIT_HIGHLIGHT_MODE_SELECTOR_H__ */

/* ex:set ts=8 noet: */
