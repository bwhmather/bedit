# -*- coding: UTF-8 -*-
#    Gedit External Tools plugin
#    Copyright (C) 2005-2006  Steve Frécinaux <steve@istique.net>
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program; if not, write to the Free Software
#    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

__all__ = ('ExternalToolsPlugin', 'Manager', 'OutputPanel', 'Capture', 'UniqueById')

from gi.repository import GLib, Gio, GObject, Gtk, Gedit, PeasGtk
from .manager import Manager
from .library import ToolLibrary
from .outputpanel import OutputPanel
from .capture import Capture
from .functions import *

class ToolMenu(object):
    def __init__(self, library, window, panel, menu):
        super(ToolMenu, self).__init__()
        self._library = library
        self._window = window
        self._panel = panel
        self._menu = menu
        self._action_tools = {}

        self.update()

    def deactivate(self):
        self.remove()

    def remove(self):
        self._menu.remove_all()

        for name, tool in self._action_tools.items():
            self._window.remove_action(name)

            if tool.shortcut:
                app = Gio.Application.get_default()
                app.remove_accelerator(tool.shortcut)
        self._action_tools = {}

    def _insert_directory(self, directory, menu):
        for d in sorted(directory.subdirs, key=lambda x: x.name.lower()):
            submenu = Gio.Menu()
            menu.append_submenu(d.name.replace('_', '__'), submenu)
            section = Gio.Menu()
            submenu.append_section(None, section)

            self._insert_directory(d, section)

        for tool in sorted(directory.tools, key=lambda x: x.name.lower()):
            action_name = 'external-tool_%X_%X' % (id(tool), id(tool.name))
            self._action_tools[action_name] = tool

            action = Gio.SimpleAction(name=action_name)
            action.connect('activate', capture_menu_action, self._window, self._panel, tool)
            self._window.add_action(action)

            item = Gio.MenuItem.new(tool.name.replace('_', '__'), "win.%s" % action_name)
            item.set_attribute_value("hidden-when", GLib.Variant.new_string("action-disabled"))
            menu.append_item(item)

            if tool.shortcut:
                app = Gio.Application.get_default()
                app.add_accelerator(tool.shortcut, "win.%s" % action_name, None)

    def update(self):
        self.remove()
        self._insert_directory(self._library.tree, self._menu)
        self.filter(self._window.get_active_document())

    def filter_language(self, language, item):
        if not item.languages:
            return True
        
        if not language and 'plain' in item.languages:
            return True
        
        if language and (language.get_id() in item.languages):
            return True
        else:
            return False

    def filter(self, document):
        if document is None:
            titled = False
            remote = False
            language = None
        else:
            titled = document.get_location() is not None
            remote = not document.is_local()
            language = document.get_language()

        states = {
            'always': True,
            'all' : document is not None,
            'local': titled and not remote,
            'remote': titled and remote,
            'titled': titled,
            'untitled': not titled,
        }

        for name, tool in self._action_tools.items():
            action = self._window.lookup_action(name)
            if action:
                action.set_enabled(states[tool.applicability] and
                                   self.filter_language(language, tool))

# FIXME: restore the launch of the manager on configure using PeasGtk.Configurable
class WindowActivatable(GObject.Object, Gedit.WindowActivatable):
    __gtype_name__ = "ExternalToolsWindowActivatable"

    window = GObject.property(type=Gedit.Window)

    def __init__(self):
        GObject.Object.__init__(self)
        self._manager = None
        self._manager_default_size = None
        self.menu = None

    def do_activate(self):
        # Ugly hack... we need to get access to the activatable to update the menuitems
        self.window._external_tools_window_activatable = self
        self._library = ToolLibrary()

        action = Gio.SimpleAction(name="manage_tools")
        action.connect("activate", lambda action, parameter: self.open_dialog())
        self.window.add_action(action)

        self.gear_menu = self.extend_gear_menu("ext9")
        item = Gio.MenuItem.new(_("Manage _External Tools..."), "win.manage_tools")
        self.gear_menu.append_menu_item(item)
        external_tools_submenu = Gio.Menu()
        item = Gio.MenuItem.new_submenu(_("External _Tools"), external_tools_submenu)
        self.gear_menu.append_menu_item(item)
        external_tools_submenu_section = Gio.Menu()
        external_tools_submenu.append_section(None, external_tools_submenu_section)

        # Create output console
        self._output_buffer = OutputPanel(self.plugin_info.get_data_dir(), self.window)

        self.menu = ToolMenu(self._library, self.window, self._output_buffer, external_tools_submenu_section)

        bottom = self.window.get_bottom_panel()
        image = Gtk.Image.new_from_icon_name("system-run-symbolic", Gtk.IconSize.MENU)
        bottom.add_item(self._output_buffer.panel,
                        "GeditExternalToolsShellOutput",
                        _("Tool Output"),
                        image)

    def do_update_state(self):
        if self.menu is not None:
            self.menu.filter(self.window.get_active_document())

    def do_deactivate(self):
        self.window._external_tools_window_activatable = None
        self.menu.deactivate()
        self.window.remove_action("manage_tools")

        bottom = self.window.get_bottom_panel()
        bottom.remove_item(self._output_buffer.panel)

    def open_dialog(self):
        if not self._manager:
            self._manager = Manager(self.plugin_info.get_data_dir())

            if self._manager_default_size:
                self._manager.dialog.set_default_size(*self._manager_default_size)

            self._manager.dialog.connect('destroy', self.on_manager_destroy)
            self._manager.connect('tools-updated', self.on_manager_tools_updated)

        window = Gio.Application.get_default().get_active_window()
        self._manager.run(window)

        return self._manager.dialog

    def update_manager(self, tool):
        if self._manager:
            self._manager.tool_changed(tool, True)

    def on_manager_destroy(self, dialog):
        self._manager_default_size = self._manager.get_final_size()
        self._manager = None

    def on_manager_tools_updated(self, manager):
        for window in Gio.Application.get_default().get_windows():
            window._external_tools_window_activatable.menu.update()

# ex:ts=4:et:
