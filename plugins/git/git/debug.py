# debug.py
# This file is part of Bedit.
#
# Copyright (C) 2020 - Ben Mather
#
# Based on plugins/git/git/debug.py from Gedit Plugins.
#
#  Copyright (C) 2014 - Garrett Regier
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

import inspect
import io
import os
import sys
import traceback
 
 
_DEBUG = os.getenv('BEDIT_DEBUG_GIT_PLUGIN') is not None
 
 
def debug(msg, *, frames=1, print_stack=False, limit=None):
    """Mimicks Bedit's bedit_debug_message() output, but only prints
       when the BEDIT_DEBUG_GIT_PLUGIN enviroment variable exists.
    """
    if not _DEBUG:
        return
 
    current_frame = inspect.currentframe()
    calling_frame = current_frame
 
    try:
        for i in range(frames):
            calling_frame = calling_frame.f_back
 
        info = inspect.getframeinfo(calling_frame)
 
        path = min(info.filename.replace(x, '') for x in sys.path)
        if path[0] == os.path.sep:
            path = path[1:]
 
        full_message = io.StringIO()
        full_message.writelines((path, ':', str(info.lineno),
                                 ' (', info.function, ') ', msg, '\n'))
 
        if print_stack:
            full_message.write('Stack (most recent call last):\n')
            traceback.print_stack(calling_frame,
                                  file=full_message, limit=limit)
 
        if full_message.getvalue()[-1] != '\n':
            full_message.write('\n')
 
        # Always write the message in a single call to prevent
        # the message from being split when using multiple threads
        sys.stderr.write(full_message.getvalue())
 
        full_message.close()
 
    finally:
        # Avoid leaking
        del calling_frame
        del current_frame
 
# ex:ts=4:et:
