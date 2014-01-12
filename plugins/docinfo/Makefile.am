plugin_LTLIBRARIES += plugins/docinfo/libdocinfo.la

plugins_docinfo_libdocinfo_la_SOURCES =			\
	plugins/docinfo/gedit-docinfo-plugin.h		\
	plugins/docinfo/gedit-docinfo-plugin.c		\
	plugins/docinfo/gedit-docinfo-resources.c

plugins_docinfo_libdocinfo_la_LDFLAGS  = $(PLUGIN_LIBTOOL_FLAGS)
plugins_docinfo_libdocinfo_la_LIBADD   = $(GEDIT_LIBS)
plugins_docinfo_libdocinfo_la_CPPFLAGS = -I$(top_srcdir)
plugins_docinfo_libdocinfo_la_CFLAGS   =		\
	$(GEDIT_CFLAGS) 				\
	$(WARN_CFLAGS)					\
	$(DISABLE_DEPRECATED_CFLAGS)

docinfo_resources_deps = $(call GRESDEPS,plugins/docinfo/gedit-docinfo.gresource.xml)
plugins/docinfo/gedit-docinfo-resources.c: $(docinfo_resources_deps)
	$(GRESGEN)

plugin_in_files += plugins/docinfo/docinfo.plugin.desktop.in

BUILT_SOURCES += plugins/docinfo/gedit-docinfo-resources.c
EXTRA_DIST += $(docinfo_resources_deps)
