if ENABLE_PYTHON

plugins_quickopendir = $(plugindir)/quickopen
plugins_quickopen_PYTHON =				\
	plugins/quickopen/quickopen/__init__.py		\
	plugins/quickopen/quickopen/popup.py		\
	plugins/quickopen/quickopen/virtualdirs.py

plugin_in_files += plugins/quickopen/quickopen.plugin.desktop.in

else

dist_plugin_in_files += plugins/quickopen/quickopen.plugin.desktop.in

endif
