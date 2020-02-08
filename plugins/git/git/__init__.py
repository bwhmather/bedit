# __init__.py
# This file is part of Bedit.
#
# Copyright (C) 2020 - Ben Mather
#
# Based on plugins/git/git/__init__.py from Gedit Plugins.
#
#  Copyright (C) 2013 - Ignacio Casal Quinteiro
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, see <http://www.gnu.org/licenses/>.

import gi

gi.require_version("Bedit", "3.0")
gi.require_version("Gtk", "3.0")
gi.require_version("Ggit", "1.0")

from .appactivatable import GitAppActivatable
from .viewactivatable import GitViewActivatable
from .windowactivatable import GitWindowActivatable

# ex:ts=4:et:
