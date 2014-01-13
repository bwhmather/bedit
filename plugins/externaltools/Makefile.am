externaltools_gschema = \
	plugins/externaltools/org.gnome.gedit.plugins.externaltools.gschema.xml

externaltools_plugin_in = plugins/externaltools/externaltools.plugin.desktop.in

if !OS_WIN32
if ENABLE_PYTHON

include plugins/externaltools/tools/Makefile.am
include plugins/externaltools/data/Makefile.am

EXTRA_DIST += plugins/externaltools/scripts/gedit-tool-merge.pl

plugin_in_files += $(externaltools_plugin_in)
plugin_gsettings_SCHEMAS += $(externaltools_gschema)

else

dist_plugin_in_files += $(externaltools_plugin_in)
dist_plugin_gsettings_SCHEMAS += $(externaltools_schema)

endif

else

dist_plugin_in_files += $(externaltools_plugin_in)
dist_plugin_gsettings_SCHEMAS += $(externaltools_schema)

endif
