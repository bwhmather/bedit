/*
 * bedit-io-error-info-bar.h
 * This file is part of Bedit.
 *
 * Copyright (C) 2020 - Ben Mather
 *
 * Based on gedit-io-error-info-bar.h from Gedit.
 *
 * Copyright (C) 2005 - Paolo Maggi
 * Copyright (C) 2010 - Garrett Regier, Ignacio Casal Quinteiro, Steve Frécinaux
 * Copyright (C) 2013-2016 - Sébastien Wilmet
 * Copyright (C) 2014 - Paolo Borelli, Sagar Ghuge
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

#ifndef BEDIT_IO_ERROR_INFO_BAR_H
#define BEDIT_IO_ERROR_INFO_BAR_H

#include <gtksourceview/gtksource.h>

G_BEGIN_DECLS

GtkWidget *bedit_io_loading_error_info_bar_new(
    GFile *location, const GtkSourceEncoding *encoding, const GError *error
);

GtkWidget *bedit_unrecoverable_reverting_error_info_bar_new(
    GFile *location, const GError *error
);

GtkWidget *bedit_conversion_error_while_saving_info_bar_new(
    GFile *location, const GtkSourceEncoding *encoding, const GError *error
);

const GtkSourceEncoding *bedit_conversion_error_info_bar_get_encoding(
    GtkWidget *info_bar
);

GtkWidget *bedit_file_already_open_warning_info_bar_new(GFile *location);

GtkWidget *bedit_externally_modified_saving_error_info_bar_new(
    GFile *location, const GError *error
);

GtkWidget *bedit_no_backup_saving_error_info_bar_new(
    GFile *location, const GError *error
);

GtkWidget *bedit_unrecoverable_saving_error_info_bar_new(
    GFile *location, const GError *error
);

GtkWidget *bedit_externally_modified_info_bar_new(
    GFile *location, gboolean document_modified
);

GtkWidget *bedit_invalid_character_info_bar_new(GFile *location);

GtkWidget *bedit_network_unavailable_info_bar_new(GFile *location);

G_END_DECLS

#endif /* BEDIT_IO_ERROR_INFO_BAR_H  */

