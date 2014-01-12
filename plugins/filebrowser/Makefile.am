plugin_LTLIBRARIES += plugins/filebrowser/libfilebrowser.la

plugins_filebrowser_libfilebrowser_la_LDFLAGS = $(PLUGIN_LIBTOOL_FLAGS)
plugins_filebrowser_libfilebrowser_la_LIBADD = $(GEDIT_LIBS)
plugins_filebrowser_libfilebrowser_la_CPPFLAGS =			\
	-I$(top_srcdir)							\
	-I$(top_builddir)/plugins/filebrowser

plugins_filebrowser_libfilebrowser_la_CFLAGS =				\
	$(GEDIT_CFLAGS) 						\
	$(WARN_CFLAGS)							\
	$(DISABLE_DEPRECATED_CFLAGS)

plugins_filebrowser_BUILTSOURCES =				\
	plugins/filebrowser/gedit-file-browser-enum-types.h	\
	plugins/filebrowser/gedit-file-browser-enum-types.c	\
	plugins/filebrowser/gedit-file-browser-marshal.h	\
	plugins/filebrowser/gedit-file-browser-marshal.c	\
	plugins/filebrowser/gedit-file-browser-resources.c

plugins_filebrowser_messages_NOINST_H_FILES =						\
	plugins/filebrowser/messages/gedit-file-browser-message-activation.h		\
	plugins/filebrowser/messages/gedit-file-browser-message-add-context-item.h	\
	plugins/filebrowser/messages/gedit-file-browser-message-add-filter.h		\
	plugins/filebrowser/messages/gedit-file-browser-message-get-root.h		\
	plugins/filebrowser/messages/gedit-file-browser-message-get-view.h		\
	plugins/filebrowser/messages/gedit-file-browser-message-id.h			\
	plugins/filebrowser/messages/gedit-file-browser-message-id-location.h		\
	plugins/filebrowser/messages/gedit-file-browser-message-set-emblem.h		\
	plugins/filebrowser/messages/gedit-file-browser-message-set-markup.h		\
	plugins/filebrowser/messages/gedit-file-browser-message-set-root.h		\
	plugins/filebrowser/messages/messages.h

plugins_filebrowser_libfilebrowser_la_NOINST_H_FILES =		\
	plugins/filebrowser/gedit-file-bookmarks-store.h	\
	plugins/filebrowser/gedit-file-browser-error.h		\
	plugins/filebrowser/gedit-file-browser-store.h		\
	plugins/filebrowser/gedit-file-browser-view.h		\
	plugins/filebrowser/gedit-file-browser-widget.h		\
	plugins/filebrowser/gedit-file-browser-utils.h		\
	plugins/filebrowser/gedit-file-browser-plugin.h		\
	plugins/filebrowser/gedit-file-browser-messages.h	\
	$(plugins_filebrowser_messages_NOINST_H_FILES)

plugins_filebrowser_messages_sources =							\
	plugins/filebrowser/messages/gedit-file-browser-message-activation.c		\
	plugins/filebrowser/messages/gedit-file-browser-message-add-context-item.c	\
	plugins/filebrowser/messages/gedit-file-browser-message-add-filter.c		\
	plugins/filebrowser/messages/gedit-file-browser-message-get-root.c		\
	plugins/filebrowser/messages/gedit-file-browser-message-get-view.c		\
	plugins/filebrowser/messages/gedit-file-browser-message-id.c			\
	plugins/filebrowser/messages/gedit-file-browser-message-id-location.c		\
	plugins/filebrowser/messages/gedit-file-browser-message-set-emblem.c		\
	plugins/filebrowser/messages/gedit-file-browser-message-set-markup.c		\
	plugins/filebrowser/messages/gedit-file-browser-message-set-root.c

plugins_filebrowser_libfilebrowser_la_SOURCES =			\
	$(plugins_filebrowser_BUILTSOURCES) 			\
	plugins/filebrowser/gedit-file-bookmarks-store.c	\
	plugins/filebrowser/gedit-file-browser-store.c 		\
	plugins/filebrowser/gedit-file-browser-view.c 		\
	plugins/filebrowser/gedit-file-browser-widget.c		\
	plugins/filebrowser/gedit-file-browser-utils.c		\
	plugins/filebrowser/gedit-file-browser-plugin.c		\
	plugins/filebrowser/gedit-file-browser-messages.c	\
	$(plugins_filebrowser_messages_sources)			\
	$(plugins_filebrowser_libfilebrowser_la_NOINST_H_FILES)

plugin_in_files += plugins/filebrowser/filebrowser.plugin.desktop.in

filebrowser_resources_deps = $(call GRESDEPS,plugins/filebrowser/gedit-file-browser.gresource.xml)
plugins/filebrowser/gedit-file-browser-resources.c: $(filebrowser_resources_deps)
	$(GRESGEN)

filebrowser_enum_c_template = plugins/filebrowser/gedit-file-browser-enum-types.c.template
filebrowser_enum_register_c_template = plugins/filebrowser/gedit-file-browser-enum-register.c.template
filebrowser_enum_h_template = plugins/filebrowser/gedit-file-browser-enum-types.h.template

plugins/filebrowser/gedit-file-browser-enum-types.h: $(filebrowser_enum_h_template) $(plugins_filebrowser_libfilebrowser_la_NOINST_H_FILES) $(GLIB_MKENUMS)
	$(AM_V_GEN) (cd $(srcdir) &&						\
	 $(GLIB_MKENUMS)							\
		--template $(filebrowser_enum_h_template)			\
		$(plugins_filebrowser_libfilebrowser_la_NOINST_H_FILES)) > $@

plugins/filebrowser/gedit-file-browser-enum-types.c: $(filebrowser_enum_c_template) $(filebrowser_enum_register_c_template) $(plugins_filebrowser_libfilebrowser_la_NOINST_H_FILES) $(GLIB_MKENUMS)
	$(AM_V_GEN) 								\
	(cd $(srcdir) &&							\
	 $(GLIB_MKENUMS)							\
		--template $(filebrowser_enum_c_template) 			\
		$(plugins_filebrowser_libfilebrowser_la_NOINST_H_FILES) &&	\
	 $(GLIB_MKENUMS)							\
		--template $(filebrowser_enum_register_c_template)		\
		$(plugins_filebrowser_libfilebrowser_la_NOINST_H_FILES)) > $@

plugins/filebrowser/gedit-file-browser-marshal.h: plugins/filebrowser/gedit-file-browser-marshal.list $(GLIB_GENMARSHAL)
	$(AM_V_GEN) $(GLIB_GENMARSHAL) $< --header --prefix=gedit_file_browser_marshal > $@

plugins/filebrowser/gedit-file-browser-marshal.c: plugins/filebrowser/gedit-file-browser-marshal.list $(GLIB_GENMARSHAL)
	$(AM_V_GEN) echo "#include \"gedit-file-browser-marshal.h\"" > $@ && \
	$(GLIB_GENMARSHAL) $< --body --prefix=gedit_file_browser_marshal >> $@

plugin_gsettings_SCHEMAS += plugins/filebrowser/org.gnome.gedit.plugins.filebrowser.gschema.xml
gsettings_ENUMS += plugins/filebrowser/org.gnome.gedit.plugins.filebrowser.enums.xml

plugins/filebrowser/org.gnome.gedit.plugins.filebrowser.enums.xml: $(plugins_filebrowser_libfilebrowser_la_NOINST_H_FILES)
	$(GSETTINGS_ENUMS_GEN)

plugins-filebrowser-update-messages:
	$(top_srcdir)/tools/generate-message.py --output-directory $(builddir)/plugins/filebrowser/messages/  --gpl --program gedit $(srcdir)/plugins/filebrowser/messages.xml

BUILT_SOURCES += $(plugins_filebrowser_BUILTSOURCES)

EXTRA_DIST +=								\
	$(filebrowser_resources_deps)					\
	plugins/filebrowser/gedit-file-browser-enum-types.h.template	\
	plugins/filebrowser/gedit-file-browser-enum-types.c.template	\
	plugins/filebrowser/gedit-file-browser-enum-register.c.template	\
	plugins/filebrowser/gedit-file-browser-marshal.list
