plugin_LTLIBRARIES += plugins/modelines/libmodelines.la

plugins_modelines_libmodelines_la_LDFLAGS = $(PLUGIN_LIBTOOL_FLAGS)
plugins_modelines_libmodelines_la_LIBADD = $(GEDIT_LIBS)
plugins_modelines_libmodelines_la_CPPFLAGS = -I$(top_srcdir)
plugins_modelines_libmodelines_la_CFLAGS = 		\
	$(GEDIT_CFLAGS) 				\
	$(WARN_CFLAGS)					\
	$(DISABLE_DEPRECATED_CFLAGS)

plugins_modelines_libmodelines_la_SOURCES =		\
	plugins/modelines/gedit-modeline-plugin.h	\
	plugins/modelines/gedit-modeline-plugin.c	\
	plugins/modelines/modeline-parser.h		\
	plugins/modelines/modeline-parser.c

plugins_modelinesdir = $(GEDIT_PLUGINS_DATA_DIR)/modelines
dist_plugins_modelines_DATA = plugins/modelines/language-mappings

plugin_in_files += plugins/modelines/modelines.plugin.desktop.in
