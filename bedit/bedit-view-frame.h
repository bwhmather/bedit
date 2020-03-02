/*
 * bedit-view-frame.h
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * Based on gedit-view-frame.h from Gedit.
 *
 * Copyright (C) 2010-2015 - Ignacio Casal Quinteiro
 * Copyright (C) 2011 - Garrett Regier
 * Copyright (C) 2013-2019 - Sébastien Wilmet
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

#ifndef BEDIT_VIEW_FRAME_H
#define BEDIT_VIEW_FRAME_H

#include <gtk/gtk.h>
#include "bedit-document.h"
#include "bedit-view.h"

G_BEGIN_DECLS

#define BEDIT_TYPE_VIEW_FRAME (bedit_view_frame_get_type())
G_DECLARE_FINAL_TYPE(
    BeditViewFrame, bedit_view_frame,
    BEDIT, VIEW_FRAME,
    GtkOverlay
)

BeditViewFrame *bedit_view_frame_new(void);

BeditView *bedit_view_frame_get_view(BeditViewFrame *frame);

void bedit_view_frame_popup_goto_line(BeditViewFrame *frame);

G_END_DECLS

#endif /* BEDIT_VIEW_FRAME_H */
