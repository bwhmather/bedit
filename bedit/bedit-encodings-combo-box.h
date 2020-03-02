/*
 * bedit-encodings-combo-box.h
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * Based on gedit-encodings-combo-box.h from Gedit.
 *
 * Copyright (C) 2003-2005 - Paolo Maggi
 * Copyright (C) 2010 - Garrett Regier, Ignacio Casal Quinteiro, Jesse van den
 *   Kieboom, Steve Frécinaux
 * Copyright (C) 2013-2016 - Sébastien Wilmet
 * Copyright (C) 2014-2015 - Paolo Borelli
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

#ifndef BEDIT_ENCODINGS_COMBO_BOX_H
#define BEDIT_ENCODINGS_COMBO_BOX_H

#include <gtksourceview/gtksource.h>

G_BEGIN_DECLS

#define BEDIT_TYPE_ENCODINGS_COMBO_BOX (bedit_encodings_combo_box_get_type())

G_DECLARE_FINAL_TYPE(
    BeditEncodingsComboBox, bedit_encodings_combo_box, BEDIT,
    ENCODINGS_COMBO_BOX, GtkComboBox
)

GtkWidget *bedit_encodings_combo_box_new(gboolean save_mode);

const GtkSourceEncoding *bedit_encodings_combo_box_get_selected_encoding(
    BeditEncodingsComboBox *menu
);

void bedit_encodings_combo_box_set_selected_encoding(
    BeditEncodingsComboBox *menu, const GtkSourceEncoding *encoding
);

G_END_DECLS

#endif /* BEDIT_ENCODINGS_COMBO_BOX_H */

