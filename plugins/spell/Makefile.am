if ENABLE_ENCHANT

plugin_LTLIBRARIES += plugins/spell/libspell.la

plugins_spell_libspell_la_LDFLAGS = $(PLUGIN_LIBTOOL_FLAGS)
plugins_spell_libspell_la_LIBADD  =		\
	$(top_builddir)/gedit/libgedit.la	\
	$(GEDIT_LIBS)				\
	$(ENCHANT_LIBS)

plugins_spell_libspell_la_CPPFLAGS =				\
	-I$(top_srcdir)						\
	-I$(top_builddir)/plugins/spell

plugins_spell_libspell_la_CFLAGS =				\
	$(GEDIT_CFLAGS) 					\
	$(ENCHANT_CFLAGS)					\
	$(WARN_CFLAGS)						\
	$(DISABLE_DEPRECATED_CFLAGS)

plugins_spell_built_sources =				\
	plugins/spell/gedit-spell-marshal.c		\
	plugins/spell/gedit-spell-marshal.h		\
	plugins/spell/gedit-spell-resources.c

plugins_spell_libspell_la_SOURCES =			\
	plugins/spell/gedit-spell-plugin.c 		\
	plugins/spell/gedit-spell-plugin.h 		\
	plugins/spell/gedit-spell-app-activatable.c	\
	plugins/spell/gedit-spell-app-activatable.h	\
	plugins/spell/gedit-spell-checker.c 		\
	plugins/spell/gedit-spell-checker.h		\
	plugins/spell/gedit-spell-checker-dialog.c	\
	plugins/spell/gedit-spell-checker-dialog.h	\
	plugins/spell/gedit-spell-checker-language.c	\
	plugins/spell/gedit-spell-checker-language.h	\
	plugins/spell/gedit-spell-language-dialog.c	\
	plugins/spell/gedit-spell-language-dialog.h	\
	plugins/spell/gedit-automatic-spell-checker.c	\
	plugins/spell/gedit-automatic-spell-checker.h	\
	plugins/spell/gedit-spell-utils.c		\
	plugins/spell/gedit-spell-utils.h		\
	$(plugins_spell_built_sources)

# Mac OSX convenience library and ldflags
if OS_OSX
noinst_LTLIBRARIES += plugins/spell/libosx.la

plugins_spell_libosx_la_LDFLAGS = -framework Cocoa
plugins_spell_libosx_la_LIBADD = -lobjc
plugins_spell_libosx_la_CFLAGS = -xobjective-c

plugins_spell_libosx_la_SOURCES = 		\
	plugins/spell/gedit-spell-osx.c		\
	plugins/spell/gedit-spell-osx.h

plugins_spell_libspell_la_LIBADD += plugins/spell/libosx.la
plugins_spell_libspell_la_LDFLAGS += $(GTK_MAC_LIBS)
plugins_spell_libspell_la_CFLAGS += $(GTK_MAC_CFLAGS)
endif

plugins/spell/gedit-spell-marshal.h: plugins/spell/gedit-spell-marshal.list $(GLIB_GENMARSHAL)
	$(AM_V_GEN) $(GLIB_GENMARSHAL) $< --header --prefix=gedit_marshal > $@

plugins/spell/gedit-spell-marshal.c: plugins/spell/gedit-spell-marshal.list $(GLIB_GENMARSHAL)
	$(AM_V_GEN) echo "#include \"gedit-spell-marshal.h\"" > $@ && \
	$(GLIB_GENMARSHAL) $< --body --prefix=gedit_marshal >> $@

spell_resource_deps = $(call GRESDEPS,plugins/spell/gedit-spell.gresource.xml)
plugins/spell/gedit-spell-resources.c: $(spell_resource_deps)
	$(GRESGEN)

plugin_in_files += plugins/spell/spell.plugin.desktop.in

BUILT_SOURCES += $(plugins_spell_built_sources)

EXTRA_DIST += 					\
	plugins/spell/gedit-spell-marshal.list	\
	$(spell_resource_deps)

else

dist_plugin_in_files += plugins/spell/spell.plugin.desktop.in

endif
