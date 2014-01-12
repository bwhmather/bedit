if ENABLE_UPDATER

plugin_LTLIBRARIES += plugins/checkupdate/libcheckupdate.la

plugins_checkupdate_libcheckupdate_la_SOURCES =			\
	plugins/checkupdate/gedit-check-update-plugin.h		\
	plugins/checkupdate/gedit-check-update-plugin.c

plugins_checkupdate_libcheckupdate_la_LDFLAGS = $(PLUGIN_LIBTOOL_FLAGS)
plugins_checkupdate_libcheckupdate_la_LIBADD = $(GEDIT_LIBS) $(LIBSOUP_LIBS)
plugins_checkupdate_libcheckupdate_la_CPPFLAGS = -I$(top_srcdir)
plugins_checkupdate_libcheckupdate_la_CFLAGS =	\
	$(GEDIT_CFLAGS) 			\
	$(LIBSOUP_CFLAGS)			\
	$(WARN_CFLAGS)				\
	$(DISABLE_DEPRECATED_CFLAGS)

plugin_in_files += plugins/checkupdate/checkupdate.plugin.desktop.in

plugin_gsettings_SCHEMAS +=							\
	plugins/checkupdate/org.gnome.gedit.plugins.checkupdate.gschema.xml

else

dist_plugin_in_files += plugins/checkupdate/checkupdate.plugin.desktop.in

dist_plugin_gsettings_SCHEMAS +=						\
	plugins/checkupdate/org.gnome.gedit.plugins.checkupdate.gschema.xml

endif
