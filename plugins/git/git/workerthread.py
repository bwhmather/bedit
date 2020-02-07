# workerthread.py
# This file is part of Bedit.
#
# Copyright (C) 2020 - Ben Mather
#
# Based on plugins/git/git/workerthread.py from Gedit Plugins.
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

from gi.repository import GLib

import abc
import collections
import queue
import threading
import traceback

from .debug import debug


class WorkerThread(threading.Thread):
    __metaclass__ = abc.ABCMeta

    __sentinel = object()

    def __init__(self, callback, chunk_size=1, *args, **kwargs):
        super().__init__(*args, **kwargs)

        self.__callback = callback
        self.__chunk_size = chunk_size

        self.__quit = threading.Event()
        self.__has_idle = threading.Event()

        self.__tasks = queue.Queue()
        self.__results = collections.deque()

    @abc.abstractmethod
    def handle_task(self, *args, **kwargs):
        raise NotImplementedError

    # TODO: add, put, push?
    def push(self, *args, **kwargs):
        self.__tasks.put((args, kwargs))

    def __close(self, process_results):
        self.__quit.set()

        # Prevent the queue.get() from blocking forever
        self.__tasks.put(self.__sentinel)

        super().join()

        if not process_results:
            self.__results.clear()

        else:
            while self.__in_idle() is GLib.SOURCE_CONTINUE:
                pass

    def terminate(self):
        self.__close(False)

    def join(self):
        self.__close(True)

    def clear(self):
        old_tasks = self.__tasks
        self.__tasks = queue.Queue(1)

        # Prevent the queue.get() from blocking forever
        old_tasks.put(self.__sentinel)

        # Block until the old queue has finished, otherwise
        # a old result could be added to the new results queue
        self.__tasks.put(self.__sentinel)
        self.__tasks.put(self.__sentinel)

        old_tasks = self.__tasks
        self.__tasks = queue.Queue()

        # Switch to the new queue
        old_tasks.put(self.__sentinel)

        # Finally, we can now create a new deque without
        # the possibility of any old results being added to it
        self.__results.clear()

    def run(self):
        while not self.__quit.is_set():
            task = self.__tasks.get()
            if task is self.__sentinel:
                continue

            args, kwargs = task

            try:
                result = self.handle_task(*args, **kwargs)

            except Exception:
                traceback.print_exc()
                continue

            self.__results.append(result)

            # Avoid having an idle for every result
            if not self.__has_idle.is_set():
                self.__has_idle.set()

                debug('%s<%s>: result callback idle started' %
                      (type(self).__name__, self.name))
                GLib.source_set_name_by_id(GLib.idle_add(self.__in_idle),
                                           '[bedit] git %s result callback idle' %
                                           (type(self).__name__,))

    def __in_idle(self):
        try:
            for i in range(self.__chunk_size):
                result = self.__results.popleft()

                try:
                    self.__callback(result)

                except Exception:
                    traceback.print_exc()

        except IndexError:
            # Must be cleared before we check the results length
            self.__has_idle.clear()

            # Only remove the idle when there are no more items,
            # some could have been added after the IndexError was raised
            if len(self.__results) == 0:
                debug('%s<%s>: result callback idle finished' %
                      (type(self).__name__, self.name))
                return GLib.SOURCE_REMOVE

        return GLib.SOURCE_CONTINUE

# ex:ts=4:et:
