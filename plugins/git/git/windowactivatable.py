# windowactivatable.py
# This file is part of Bedit.
#
# Copyright (C) 2020 - Ben Mather
#
# Based on plugins/git/git/windowactivatable.py from Gedit.
#
#  Copyright (C) 2013-2014 - Garrett Regier
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

from gi.repository import GLib, GObject, Gio, Bedit, Ggit

import collections.abc
import weakref

from .appactivatable import GitAppActivatable
from .debug import debug
from .workerthread import WorkerThread


class FileNode(object):
    __slots__ = ('id', 'name')

    def __init__(self, msg):
        self.id = msg.id
        self.name = msg.name


class FileNodes(collections.abc.MutableMapping):
    __slots__ = ('__data')

    def __init__(self):
        self.__data = {}

    def __type_name(self, type):
        if type.__module__ == 'builtins':
            return type.__name__

        return '.'.join((type.__module__, type.__name__))

    def __typecheck(self, value, desired_type):
        if not isinstance(value, desired_type):
            raise TypeError('%s argument expected, got %s.' %
                            (self.__type_name(desired_type),
                             self.__type_name(type(value))))

    def __delitem__(self, location):
        self.__typecheck(location, Gio.File)

        del self.__data[location.get_uri()]

    def __getitem__(self, location):
        self.__typecheck(location, Gio.File)

        return self.__data[location.get_uri()]

    def __setitem__(self, location, node):
        self.__typecheck(location, Gio.File)
        self.__typecheck(node, FileNode)

        if location in self:
            debug('Location is already inserted: %s' % (location.get_uri()))

        self.__data[location.get_uri()] = node

    def __iter__(self):
        return (Gio.File.new_for_uri(x) for x in self.__data)

    def __len__(self):
        return len(self.__data)


class GitStatusThread(WorkerThread):
    def push(self, repo, location):
        if repo is None:
            debug('Invalid repository', print_stack=True)
            return
 
        git_dir = repo.get_location().get_uri()
        if location.get_uri().startswith(git_dir):
            debug('Invalid location: "%s" is in git dir "%s"' %
                  (location.get_uri(), git_dir), print_stack=True)
            return

        workdir = repo.get_workdir()
        if workdir.get_relative_path(location) is None:
            debug('Invalid location "%s" for workdir "%s"' %
                  (location.get_uri(), workdir.get_uri()), print_stack=True)
            return

        super().push(repo, location)

    def handle_task(self, repo, location):
        return location, repo.file_status(location)


class GitWindowActivatable(GObject.Object, Bedit.WindowActivatable):
    window = GObject.Property(type=Bedit.Window)

    windows = weakref.WeakValueDictionary()

    def __init__(self):
        super().__init__()

        self.view_activatables = weakref.WeakSet()

    @classmethod
    def register_view_activatable(cls, view_activatable):
        window = view_activatable.view.get_toplevel()

        if window not in cls.windows:
            return None

        window_activatable = cls.windows[window]

        window_activatable.view_activatables.add(view_activatable)
        view_activatable.connect('notify::status',
                                 window_activatable.notify_status)

        return window_activatable

    def do_activate(self):
        # self.window is not set until now
        self.windows[self.window] = self

        self.app_activatable = GitAppActivatable.get_instance()

        self.bus = self.window.get_message_bus()

        self.git_status_thread = GitStatusThread(self.update_location)
        self.git_status_thread.start()

        self.file_nodes = FileNodes()
        self.monitors = {}
        self.has_focus = True

        self.gobject_signals = {
            self.window: [
                self.window.connect('tab-removed', self.tab_removed),
                self.window.connect('focus-in-event', self.focus_in_event),
                self.window.connect('focus-out-event', self.focus_out_event)
            ],

            # BeditMessageBus.connect() shadows GObject.connect()
            self.bus: [
                GObject.Object.connect(self.bus, 'unregistered',
                                       self.unregistered)
            ]
        }

        # It is safe to connect to these even
        # if the file browser is not enabled yet
        self.bus_signals = [
            self.bus.connect('/plugins/filebrowser', 'root_changed',
                             self.root_changed, None),
            self.bus.connect('/plugins/filebrowser', 'inserted',
                             self.inserted, None),
            self.bus.connect('/plugins/filebrowser', 'deleted',
                             self.deleted, None)
        ]

        self.refresh()

    def do_deactivate(self):
        self.clear_monitors()
        self.git_status_thread.terminate()

        for gobject, sids in self.gobject_signals.items():
            for sid in sids:
                # BeditMessageBus.disconnect() shadows GObject.disconnect()
                GObject.Object.disconnect(gobject, sid)

        for sid in self.bus_signals:
            self.bus.disconnect(sid)

        self.file_nodes = FileNodes()
        self.gobject_signals = {}
        self.bus_signals = []

        self.refresh()

    def refresh(self):
        if self.bus.is_registered('/plugins/filebrowser', 'refresh'):
            self.bus.send('/plugins/filebrowser', 'refresh')

    def get_view_activatable_by_view(self, view):
        for view_activatable in self.view_activatables:
            if view_activatable.view == view:
                return view_activatable

        return None

    def get_view_activatable_by_location(self, location):
        for view_activatable in self.view_activatables:
            buf = view_activatable.view.get_buffer()
            if buf is None:
                continue

            view_location = buf.get_file().get_location()
            if view_location is None:
                continue

            if view_location.equal(location):
                return view_activatable

        return None

    def notify_status(self, view_activatable, psepc):
        location = view_activatable.view.get_buffer().get_file().get_location()
        if location is None:
            return

        if location not in self.file_nodes:
            return

        repo = self.get_repository(location)
        if repo is not None:
            self.git_status_thread.push(repo, location)

    def tab_removed(self, window, tab):
        view = tab.get_view()

        # Need to remove the view activatable otherwise update_location()
        # might use the view's status and not the file's actual status
        view_activatable = self.get_view_activatable_by_view(view)
        if view_activatable is not None:
            self.view_activatables.remove(view_activatable)

        location = view.get_buffer().get_file().get_location()
        if location is None:
            return

        if location not in self.file_nodes:
            return

        repo = self.get_repository(location)
        if repo is not None:
            self.git_status_thread.push(repo, location)

    def focus_in_event(self, window, event):
        # Enables the file monitors so they can cause things
        # to update again. We disabled them when the focus
        # was lost and we will instead do a full update now.
        self.has_focus = True

        self.app_activatable.clear_repositories()

        for view_activatable in self.view_activatables:
            # Must reload the location's contents, not just rediff
            GLib.idle_add(view_activatable.update_location)

        for location in self.file_nodes:
            # Still need to update the git status
            # as the file could now be in .gitignore
            repo = self.get_repository(location)
            if repo is not None:
                self.git_status_thread.push(repo, location)

    def focus_out_event(self, window, event):
        # Disables the file monitors so they don't
        # cause anything to update. We will do a
        # full update when we have focus again.
        self.has_focus = False

    def unregistered(self, bus, object_path, method):
        # Avoid warnings like crazy if the file browser becomes disabled
        if object_path == '/plugins/filebrowser' and method == 'root_changed':
            self.clear_monitors()
            self.git_status_thread.clear()
            self.file_nodes = FileNodes()

    def get_repository(self, location, is_dir=False):
        return self.app_activatable.get_repository(location, is_dir)

    def root_changed(self, bus, msg, data=None):
        self.clear_monitors()
        self.git_status_thread.clear()
        self.file_nodes = FileNodes()

        location = msg.location

        repo = self.get_repository(location, True)
        if repo is not None:
            self.monitor_directory(location)

    def inserted(self, bus, msg, data=None):
        location = msg.location

        repo = self.get_repository(location, msg.is_directory)
        if repo is None:
            return

        if msg.is_directory:
            self.monitor_directory(location)

        else:
            self.file_nodes[location] = FileNode(msg)
            self.git_status_thread.push(repo, location)

    def deleted(self, bus, msg, data=None):
        location = msg.location
        uri = location.get_uri()

        if uri in self.monitors:
            self.monitors[uri].cancel()
            del self.monitors[uri]

        else:
            try:
                del self.file_nodes[location]

            except KeyError:
                pass

    def update_location(self, result):
        location, status = result

        # The node may have been deleted
        # before the status was determined
        try:
            file_node = self.file_nodes[location]

        except KeyError:
            return

        if status is None or not status & Ggit.StatusFlags.IGNORED:
            view_activatable = self.get_view_activatable_by_location(location)
            if view_activatable is not None:
                status = view_activatable.status

        markup = GLib.markup_escape_text(file_node.name)

        if status is not None:
            if status & Ggit.StatusFlags.INDEX_NEW or \
                    status & Ggit.StatusFlags.WORKING_TREE_NEW or \
                    status & Ggit.StatusFlags.INDEX_MODIFIED or \
                    status & Ggit.StatusFlags.WORKING_TREE_MODIFIED:
                markup = '<span weight="bold">%s</span>' % (markup)

            elif status & Ggit.StatusFlags.INDEX_DELETED or \
                    status & Ggit.StatusFlags.WORKING_TREE_DELETED:
                markup = '<span strikethrough="true">%s</span>' % (markup)

        self.bus.send_sync('/plugins/filebrowser', 'set_markup',
                           id=file_node.id, markup=markup)

    def clear_monitors(self):
        for uri in self.monitors:
            self.monitors[uri].cancel()

        self.monitors = {}

    def monitor_directory(self, location):
        try:
            monitor = location.monitor(Gio.FileMonitorFlags.NONE, None)

        except GLib.Error as e:
            debug('Failed to monitor directory "%s": %s' %
                  (location.get_uri(), e))
            return

        self.monitors[location.get_uri()] = monitor
        monitor.connect('changed', self.monitor_changed)

    def monitor_changed(self, monitor, file_a, file_b, event_type):
        # Don't update anything as we will do
        # a full update when we have focus again
        if not self.has_focus:
            return

        # Only monitor for changes as the file browser
        # will emit signals for the other event types
        if event_type != Gio.FileMonitorEvent.CHANGED:
            return

        for f in (file_a, file_b):
            if f is None:
                continue

            # Must let the view activatable know
            # that its location's contents have changed
            view_activatable = self.get_view_activatable_by_location(f)
            if view_activatable is not None:
                # Must reload the location's contents, not just rediff
                GLib.idle_add(view_activatable.update_location)

                # Still need to update the git status
                # as the file could now be in .gitignore

            if f in self.file_nodes:
                repo = self.get_repository(f)
                if repo is not None:
                    self.git_status_thread.push(repo, f)

# ex:ts=4:et:
