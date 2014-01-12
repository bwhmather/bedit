plugin_LTLIBRARIES += plugins/sort/libsort.la

plugins_sort_libsort_la_LDFLAGS = $(PLUGIN_LIBTOOL_FLAGS)
plugins_sort_libsort_la_LIBADD = $(GEDIT_LIBS)
plugins_sort_libsort_la_CPPFLAGS = -I$(top_srcdir)
plugins_sort_libsort_la_CFLAGS =			\
	$(GEDIT_CFLAGS) 				\
	$(WARN_CFLAGS)					\
	$(DISABLE_DEPRECATED_CFLAGS)

sort_resource_deps = $(call GRESDEPS,plugins/sort/gedit-sort.gresource.xml)
plugins/sort/gedit-sort-resources.c: $(sort_resource_deps)
	$(GRESGEN)

BUILT_SOURCES += plugins/sort/gedit-sort-resources.c

plugins_sort_libsort_la_SOURCES =		\
	plugins/sort/gedit-sort-plugin.h	\
	plugins/sort/gedit-sort-plugin.c	\
	plugins/sort/gedit-sort-resources.c

EXTRA_DIST += $(sort_resource_deps)

plugin_in_files += plugins/sort/sort.plugin.desktop.in
