# viewactivatable.py
# This file is part of Bedit.
#
# Copyright (C) 2020 - Ben Mather
#
# Based on plugins/git/git/viewactivatable.py from Gedit Plugins.
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

from gi.repository import GLib, GObject, Gtk, Bedit, Ggit

from .appactivatable import GitAppActivatable
from .diffrenderer import DiffType, DiffRenderer
from .windowactivatable import GitWindowActivatable

import sys
import os.path
import difflib


class LineContext:
    __slots__ = ("removed_lines", "line_type")

    def __init__(self):
        self.removed_lines = []
        self.line_type = DiffType.NONE


class GitViewActivatable(GObject.Object, Bedit.ViewActivatable):
    view = GObject.Property(type=Bedit.View)

    status = GObject.Property(
        type=Ggit.StatusFlags, default=Ggit.StatusFlags.CURRENT
    )

    def __init__(self):
        super().__init__()

        self.diff_timeout = 0
        self.file_contents_list = None
        self.file_context = None

    def do_activate(self):
        GitWindowActivatable.register_view_activatable(self)

        self.app_activatable = GitAppActivatable.get_instance()

        self.diff_renderer = DiffRenderer()
        self.gutter = self.view.get_gutter(Gtk.TextWindowType.LEFT)

        # Note: GitWindowActivatable will call
        #       update_location() for us when needed
        self.view_signals = [
            self.view.connect("notify::buffer", self.on_notify_buffer)
        ]

        self.buffer = None
        self.on_notify_buffer(self.view)

    def do_deactivate(self):
        if self.diff_timeout != 0:
            GLib.source_remove(self.diff_timeout)

        self.disconnect_buffer()
        self.buffer = None

        self.disconnect_view()
        self.gutter.remove(self.diff_renderer)

    def disconnect(self, obj, signals):
        for sid in signals:
            obj.disconnect(sid)

        signals[:] = []

    def disconnect_buffer(self):
        self.disconnect(self.buffer, self.buffer_signals)

    def disconnect_view(self):
        self.disconnect(self.view, self.view_signals)

    def on_notify_buffer(self, view, gspec=None):
        if self.diff_timeout != 0:
            GLib.source_remove(self.diff_timeout)

        if self.buffer:
            self.disconnect_buffer()

        self.buffer = view.get_buffer()

        # The changed signal is connected to in update_location().
        # The saved signal is pointless as the window activatable
        # will see the change and call update_location().
        self.buffer_signals = [
            self.buffer.connect("loaded", self.update_location)
        ]

        # We wait and let the loaded signal call
        # update_location() as the buffer is currently empty

    # TODO: This can be called many times and by idles,
    #       should instead do the work in another thread
    def update_location(self, *args):
        self.location = self.buffer.get_file().get_location()

        if self.location is not None:
            repo = self.app_activatable.get_repository(self.location, False)

        if self.location is None or repo is None:
            if self.file_contents_list is not None:
                self.file_contents_list = None
                self.gutter.remove(self.diff_renderer)
                self.diff_renderer.set_file_context({})
                self.buffer.disconnect(self.buffer_signals.pop())

            return

        if self.file_contents_list is None:
            self.gutter.insert(self.diff_renderer, 40)
            self.buffer_signals.append(
                self.buffer.connect("changed", self.update)
            )

        try:
            head = repo.get_head()
            commit = repo.lookup(head.get_target(), Ggit.Commit)
            tree = commit.get_tree()
            relative_path = os.path.relpath(
                os.path.realpath(self.location.get_path()),
                repo.get_workdir().get_path(),
            )

            entry = tree.get_by_path(relative_path)
            file_blob = repo.lookup(entry.get_id(), Ggit.Blob)
            try:
                gitconfig = repo.get_config()
                encoding = gitconfig.get_string("gui.encoding")
            except GLib.Error:
                encoding = "utf8"
            file_contents = file_blob.get_raw_content().decode(encoding)
            self.file_contents_list = file_contents.splitlines()

            # Remove the last empty line added by bedit automatically
            if self.file_contents_list:
                last_item = self.file_contents_list[-1]
                if last_item[-1:] == "\n":
                    self.file_contents_list[-1] = last_item[:-1]

        except GLib.Error:
            # New file in a git repository
            self.file_contents_list = []

        self.update()

    def update(self, *unused):
        # We don't let the delay accumulate
        if self.diff_timeout != 0:
            return

        # Do the initial diff without a delay
        if self.file_context is None:
            self.on_diff_timeout()

        else:
            n_lines = self.buffer.get_line_count()
            delay = min(10000, 200 * (n_lines // 2000 + 1))

            self.diff_timeout = GLib.timeout_add(delay, self.on_diff_timeout)

    def on_diff_timeout(self):
        self.diff_timeout = 0

        # Must be a new file
        if not self.file_contents_list:
            self.status = Ggit.StatusFlags.WORKING_TREE_NEW

            n_lines = self.buffer.get_line_count()
            if len(self.diff_renderer.file_context) == n_lines:
                return False

            line_context = LineContext()
            line_context.line_type = DiffType.ADDED
            file_context = dict(
                zip(range(1, n_lines + 1), [line_context] * n_lines)
            )

            self.diff_renderer.set_file_context(file_context)
            return False

        start_iter, end_iter = self.buffer.get_bounds()
        src_contents = start_iter.get_visible_text(end_iter)
        src_contents_list = src_contents.splitlines()

        # GtkTextBuffer does not consider a trailing "\n" to be text
        if len(src_contents_list) != self.buffer.get_line_count():
            src_contents_list.append("")

        diff = difflib.unified_diff(
            self.file_contents_list, src_contents_list, n=0
        )

        # Skip the first 2 lines: ---, +++
        try:
            next(diff)
            next(diff)

        except StopIteration:
            # Nothing has changed
            self.status = Ggit.StatusFlags.CURRENT

        else:
            self.status = Ggit.StatusFlags.WORKING_TREE_MODIFIED

        file_context = {}
        for line_data in diff:
            if line_data[0] == "@":
                for token in line_data.split():
                    if token[0] == "+":
                        hunk_point = int(token.split(",", 1)[0])
                        line_context = LineContext()
                        break

            elif line_data[0] == "-":
                if line_context.line_type == DiffType.NONE:
                    line_context.line_type = DiffType.REMOVED

                line_context.removed_lines.append(line_data[1:])

                # No hunk point increase
                file_context[hunk_point] = line_context

            elif line_data[0] == "+":
                if line_context.line_type == DiffType.NONE:
                    line_context.line_type = DiffType.ADDED
                    file_context[hunk_point] = line_context

                elif line_context.line_type == DiffType.REMOVED:
                    # Why is this the only one that does
                    # not add it to file_context?

                    line_context.line_type = DiffType.MODIFIED

                else:
                    file_context[hunk_point] = line_context

                hunk_point += 1

        # Occurs when all of the original content is deleted
        if 0 in file_context:
            for i in reversed(list(file_context.keys())):
                file_context[i + 1] = file_context[i]
                del file_context[i]

        self.file_context = file_context
        self.diff_renderer.set_file_context(file_context)
        return False


# ex:ts=4:et:
