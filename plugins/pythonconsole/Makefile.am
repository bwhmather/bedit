if ENABLE_PYTHON

plugins_pythonconsoledir = $(plugindir)/pythonconsole
plugins_pythonconsole_PYTHON =				\
	plugins/pythonconsole/pythonconsole/__init__.py	\
	plugins/pythonconsole/pythonconsole/console.py	\
	plugins/pythonconsole/pythonconsole/config.py

plugins_pythonconsole_uidir = $(GEDIT_PLUGINS_DATA_DIR)/pythonconsole/ui
dist_plugins_pythonconsole_ui_DATA = plugins/pythonconsole/pythonconsole/config.ui

plugin_gsettings_SCHEMAS +=							\
	plugins/pythonconsole/org.gnome.gedit.plugins.pythonconsole.gschema.xml

plugin_in_files += plugins/pythonconsole/pythonconsole.plugin.desktop.in

else

dist_plugin_gsettings_SCHEMAS +=							\
	plugins/pythonconsole/org.gnome.gedit.plugins.pythonconsole.gschema.xml

dist_plugin_in_files += plugins/pythonconsole/pythonconsole.plugin.desktop.in

endif
