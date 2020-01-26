/*
 * gedit-view-frame.h
 * This file is part of gedit
 *
 * Copyright (C) 2010 - Ignacio Casal Quinteiro
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

#ifndef GEDIT_VIEW_FRAME_H
#define GEDIT_VIEW_FRAME_H

#include <gtk/gtk.h>
#include "gedit-document.h"
#include "gedit-view.h"

G_BEGIN_DECLS

#define GEDIT_TYPE_VIEW_FRAME (gedit_view_frame_get_type ())
G_DECLARE_FINAL_TYPE (BeditViewFrame, gedit_view_frame, GEDIT, VIEW_FRAME, GtkOverlay)

BeditViewFrame	*gedit_view_frame_new			(void);

BeditView	*gedit_view_frame_get_view		(BeditViewFrame *frame);

void		 gedit_view_frame_popup_search		(BeditViewFrame *frame);

void		 gedit_view_frame_popup_goto_line	(BeditViewFrame *frame);

void		 gedit_view_frame_clear_search		(BeditViewFrame *frame);

G_END_DECLS

#endif /* GEDIT_VIEW_FRAME_H */
