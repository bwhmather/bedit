/*
 * bedit-view-frame.h
 * This file is part of bedit
 *
 * Copyright (C) 2010 - Ignacio Casal Quinteiro
 *
 * bedit is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * bedit is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with bedit. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GEDIT_VIEW_FRAME_H
#define GEDIT_VIEW_FRAME_H

#include <gtk/gtk.h>
#include "bedit-document.h"
#include "bedit-view.h"

G_BEGIN_DECLS

#define GEDIT_TYPE_VIEW_FRAME (bedit_view_frame_get_type ())
G_DECLARE_FINAL_TYPE (BeditViewFrame, bedit_view_frame, GEDIT, VIEW_FRAME, GtkOverlay)

BeditViewFrame	*bedit_view_frame_new			(void);

BeditView	*bedit_view_frame_get_view		(BeditViewFrame *frame);

void		 bedit_view_frame_popup_search		(BeditViewFrame *frame);

void		 bedit_view_frame_popup_goto_line	(BeditViewFrame *frame);

void		 bedit_view_frame_clear_search		(BeditViewFrame *frame);

G_END_DECLS

#endif /* GEDIT_VIEW_FRAME_H */
