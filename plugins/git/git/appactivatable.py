# appactivatable.py
# This file is part of Bedit.
#
# Copyright (C) 2020 - Ben Mather
#
# Based on plugins/git/git/appactivatable.py from Gedit Plugins.
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

from gi.repository import GLib, GObject, Gio, Bedit, Ggit


class GitAppActivatable(GObject.Object, Bedit.AppActivatable):
    app = GObject.Property(type=Bedit.App)

    __instance = None

    def __init__(self):
        super().__init__()

        Ggit.init()

        GitAppActivatable.__instance = self

    def do_activate(self):
        self.clear_repositories()

    def do_deactivate(self):
        self.__git_repos = None
        self.__workdir_repos = None

    @classmethod
    def get_instance(cls):
        return cls.__instance

    def clear_repositories(self):
        self.__git_repos = {}
        self.__workdir_repos = {}

    def get_repository(self, location, is_dir, *, allow_git_dir=False):
        # The repos are cached by the directory
        dir_location = location if is_dir else location.get_parent()
        dir_uri = dir_location.get_uri()

        # Fast Path
        try:
            return self.__workdir_repos[dir_uri]

        except KeyError:
            pass

        try:
            repo = self.__git_repos[dir_uri]

        except KeyError:
            pass

        else:
            return repo if allow_git_dir else None

        # Doing remote operations is too slow
        if not location.has_uri_scheme("file"):
            return None

        # Must check every dir, otherwise submodules will have issues
        try:
            repo_file = Ggit.Repository.discover(location)

        except GLib.Error:
            # Prevent trying to find a git repository
            # for every file in this directory
            self.__workdir_repos[dir_uri] = None
            return None

        repo_uri = repo_file.get_uri()

        # Reuse the repo if requested multiple times
        try:
            repo = self.__git_repos[repo_uri]

        except KeyError:
            repo = Ggit.Repository.open(repo_file)

            # TODO: this was around even when not used, on purpose?
            head = repo.get_head()
            commit = repo.lookup(head.get_target(), Ggit.Commit)
            tree = commit.get_tree()

            self.__git_repos[repo_uri] = repo

        # Need to keep the caches for workdir and
        # the .git dir separate to support allow_git_dir
        if dir_uri.startswith(repo_uri):
            top_uri = repo_uri
            repos = self.__git_repos

        else:
            top_uri = repo.get_workdir().get_uri()
            repos = self.__workdir_repos

        # Avoid trouble with symbolic links
        while dir_uri.startswith(top_uri):
            repos[dir_uri] = repo

            dir_location = dir_location.get_parent()
            dir_uri = dir_location.get_uri()

            # Avoid caching the repo all the
            # way up to the top dir each time
            if dir_uri in repos:
                break

        if repos is self.__git_repos:
            return repo if allow_git_dir else None

        return repo


# ex:ts=4:et:
