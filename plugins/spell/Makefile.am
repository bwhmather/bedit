if ENABLE_SPELL

plugin_LTLIBRARIES += plugins/spell/libspell.la

plugins_spell_libspell_la_LDFLAGS = $(PLUGIN_LIBTOOL_FLAGS)
plugins_spell_libspell_la_LIBADD  =		\
	$(top_builddir)/gedit/libgedit.la	\
	$(GEDIT_LIBS)				\
	$(GSPELL_LIBS)

plugins_spell_libspell_la_CPPFLAGS =				\
	-I$(top_srcdir)						\
	-I$(top_srcdir)/plugins/spell

plugins_spell_libspell_la_CFLAGS =				\
	$(GEDIT_CFLAGS) 					\
	$(GSPELL_CFLAGS)					\
	$(WARN_CFLAGS)						\
	$(DISABLE_DEPRECATED_CFLAGS)

plugins_spell_libspell_la_SOURCES =			\
	plugins/spell/gedit-spell-plugin.c 		\
	plugins/spell/gedit-spell-plugin.h 		\
	plugins/spell/gedit-spell-app-activatable.c	\
	plugins/spell/gedit-spell-app-activatable.h

plugin_in_files += plugins/spell/spell.plugin.desktop.in

else

dist_plugin_in_files += plugins/spell/spell.plugin.desktop.in

endif
