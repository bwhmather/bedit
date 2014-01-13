pythonconsole_gschema = \
	plugins/pythonconsole/org.gnome.gedit.plugins.pythonconsole.gschema.xml

pythonconsole_plugin_in = plugins/pythonconsole/pythonconsole.plugin.desktop.in

if ENABLE_PYTHON

plugins_pythonconsoledir = $(plugindir)/pythonconsole
plugins_pythonconsole_PYTHON =				\
	plugins/pythonconsole/pythonconsole/__init__.py	\
	plugins/pythonconsole/pythonconsole/console.py	\
	plugins/pythonconsole/pythonconsole/config.py

plugins_pythonconsole_uidir = $(GEDIT_PLUGINS_DATA_DIR)/pythonconsole/ui
dist_plugins_pythonconsole_ui_DATA = plugins/pythonconsole/pythonconsole/config.ui

plugin_gsettings_SCHEMAS += $(pythonconsole_gschema)
plugin_in_files += $(pythonconsole_plugin_in)

else

dist_plugin_gsettings_SCHEMAS += $(pythonconsole_gschema)
dist_plugin_in_files += $(pythonconsole_plugin_in)

endif
