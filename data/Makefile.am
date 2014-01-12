data_desktopdir = $(datadir)/applications
data_desktop_in_files = data/gedit.desktop.in
data_desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

data_appdatadir = $(datadir)/appdata
data_appdata_in_files = data/gedit.appdata.xml.in
data_appdata_DATA = $(appdata_in_files:.xml.in=.xml)

data_servicedir = $(datadir)/dbus-1/services
data_service_in_files = data/org.gnome.gedit.service.in
data_service_DATA = $(service_in_files:.service.in=.service)

$(data_service_DATA): $(data_service_in_files) Makefile
	@sed -e "s|\@bindir\@|$(bindir)|" $<> $@

dist_man_MANS = data/gedit.1

data_pkgconfigdir = $(libdir)/pkgconfig
data_pkgconfig_DATA = data/gedit.pc

data_bugreportdir = $(libexecdir)/gedit
dist_data_bugreport_SCRIPTS = data/gedit-bugreport.sh

gsettings_ENUM_NAMESPACE = org.gnome.gedit
gsettings_ENUM_FILES = $(top_srcdir)/gedit/*.h

gsettings_SCHEMAS += data/org.gnome.gedit.gschema.xml

if OS_OSX
dist_gsettingsschema_DATA += data/gedit-osx.gschema.override
endif

data_convertdir = $(datadir)/GConf/gsettings
dist_data_convert_DATA = data/gedit.convert

if OS_OSX
data_geditdatadir = $(datadir)/gedit
dist_data_geditdata_DATA = osx.accels osx.css
endif

EXTRA_DIST += 					\
	$(data_desktop_in_files)		\
	$(data_appdata_in_files)		\
	$(data_service_in_files)		\
	data/gedit.pc.in

CLEANFILES +=	 			\
	$(data_appdata_DATA)		\
	$(data_desktop_DATA)		\
	$(data_service_DATA)		\
	$(data_pkgconfig_DATA)

DISTCLEANFILES +=	 		\
	$(data_desktop_DATA)		\
	$(data_service_DATA)		\
	$(data_pkgconfig_DATA)
