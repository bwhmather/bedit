if ENABLE_ZEITGEIST

plugin_LTLIBRARIES += plugins/zeitgeist/libzeitgeist.la

plugins_zeitgeist_libzeitgeist_la_LDFLAGS = $(PLUGIN_LIBTOOL_FLAGS)
plugins_zeitgeist_libzeitgeist_la_LIBADD = $(GEDIT_LIBS) $(ZEITGEIST_LIBS)
plugins_zeitgeist_libzeitgeist_la_CPPFLAGS = -I$(top_srcdir)
plugins_zeitgeist_libzeitgeist_la_CFLAGS =			\
	$(GEDIT_CFLAGS) 				\
	$(ZEITGEIST_CFLAGS)				\
	$(WARN_CFLAGS)					\
	$(DISABLE_DEPRECATED_CFLAGS)

plugins_zeitgeist_libzeitgeist_la_SOURCES =		\
	plugins/zeitgeist/gedit-zeitgeist-plugin.h	\
	plugins/zeitgeist/gedit-zeitgeist-plugin.c

EXTRA_DIST += $(zeitgeist_resource_deps)

plugin_in_files += plugins/zeitgeist/zeitgeist.plugin.desktop.in

else

dist_plugin_in_files += plugins/zeitgeist/zeitgeist.plugin.desktop.in

endif
