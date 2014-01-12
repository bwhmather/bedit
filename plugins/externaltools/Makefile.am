if !OS_WIN32
if ENABLE_PYTHON

include plugins/externaltools/tools/Makefile.am
include plugins/externaltools/data/Makefile.am

EXTRA_DIST += plugins/externaltools/scripts/gedit-tool-merge.pl

plugin_in_files += plugins/externaltools/externaltools.plugin.desktop.in
plugin_gsettings_SCHEMAS +=							\
	plugins/externaltools/org.gnome.gedit.plugins.externaltools.gschema.xml

else

dist_plugin_in_files += plugins/externaltools/externaltools.plugin.desktop.in
dist_plugin_gsettings_SCHEMAS +=						\
	plugins/externaltools/org.gnome.gedit.plugins.externaltools.gschema.xml


endif

else

dist_plugin_in_files += plugins/externaltools/externaltools.plugin.desktop.in
dist_plugin_gsettings_SCHEMAS +=						\
	plugins/externaltools/org.gnome.gedit.plugins.externaltools.gschema.xml

endif
