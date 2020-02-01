/*
 * bedit-pango.h
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * Based on gedit-pango.h from Gedit which is, in turn, a copy of dzl-pango.h
 * from libdazzle.
 *
 * Copyright (C) 2014 - Christian Hergert <christian@hergert.me>
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

#ifndef BEDIT_PANGO_H
#define BEDIT_PANGO_H

#include <pango/pango.h>

G_BEGIN_DECLS

gchar *bedit_pango_font_description_to_css(
    const PangoFontDescription *font_desc);

G_END_DECLS

#endif /* BEDIT_PANGO_H */

