plugin_LTLIBRARIES += plugins/time/libtime.la

plugins_time_libtime_la_LDFLAGS = $(PLUGIN_LIBTOOL_FLAGS)
plugins_time_libtime_la_LIBADD = $(GEDIT_LIBS)
plugins_time_libtime_la_CPPFLAGS = -I$(top_srcdir)
plugins_time_libtime_la_CFLAGS =			\
	$(GEDIT_CFLAGS) 				\
	$(WARN_CFLAGS)					\
	$(DISABLE_DEPRECATED_CFLAGS)

time_resource_deps = $(call GRESDEPS,plugins/time/gedit-time.gresource.xml)
plugins/time/gedit-time-resources.c: $(time_resource_deps)
	$(GRESGEN)

BUILT_SOURCES += plugins/time/gedit-time-resources.c

plugins_time_libtime_la_SOURCES =		\
	plugins/time/gedit-time-plugin.h	\
	plugins/time/gedit-time-plugin.c	\
	plugins/time/gedit-time-resources.c

EXTRA_DIST += $(time_resource_deps)

plugin_gsettings_SCHEMAS += plugins/time/org.gnome.gedit.plugins.time.gschema.xml
gsettings_ENUMS += plugins/time/org.gnome.gedit.plugins.time.enums.xml

plugins/time/org.gnome.gedit.plugins.time.enums.xml: plugins/time/gedit-time-plugin.c
	$(GSETTINGS_ENUMS_GEN)

plugin_in_files += plugins/time/time.plugin.desktop.in
