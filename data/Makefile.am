desktopdir 	 = $(datadir)/applications
desktop_in_files = gedit.desktop.in
desktop_DATA 	 = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

@INTLTOOL_XML_RULE@

appdatadir = $(datadir)/appdata
appdata_DATA = $(appdata_in_files:.xml.in=.xml)
appdata_in_files = gedit.appdata.xml.in

servicedir = $(datadir)/dbus-1/services
service_in_files = org.gnome.gedit.service.in
service_DATA = $(service_in_files:.service.in=.service)

$(service_DATA): $(service_in_files) Makefile
	@sed -e "s|\@bindir\@|$(bindir)|" $<> $@

man_MANS = gedit.1

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = gedit.pc

bugreportdir = $(libexecdir)/gedit
dist_bugreport_SCRIPTS = gedit-bugreport.sh

gsettings_ENUM_NAMESPACE = org.gnome.gedit
gsettings_ENUM_FILES = $(top_srcdir)/gedit/*.h
gsettings_SCHEMAS = org.gnome.gedit.gschema.xml

@GSETTINGS_RULES@

if OS_OSX
gsettingsschema_DATA = gedit-osx.gschema.override
endif

convertdir = $(datadir)/GConf/gsettings
convert_DATA = gedit.convert

if OS_OSX
geditdatadir = $(datadir)/gedit
geditdata_DATA = osx.accels osx.css
endif

EXTRA_DIST = 					\
	$(desktop_in_files)			\
	$(appdata_in_files)			\
	$(service_in_files)			\
	$(man_MANS)				\
	gedit.pc.in				\
	org.gnome.gedit.gschema.xml.in		\
	gedit.convert				\
	osx.accels				\
	osx.css					\
	gedit-osx.gschema.override

CLEANFILES =	 			\
	$(appdata_DATA)			\
	$(desktop_DATA)			\
	$(service_DATA)			\
	$(pkgconfig_DATA)		\
	$(gsettings_SCHEMAS)

DISTCLEANFILES =	 		\
	$(desktop_DATA)			\
	$(service_DATA)			\
	$(pkgconfig_DATA)		\
	$(gsettings_SCHEMAS)

MAINTAINERCLEANFILES = $(gsettings_SCHEMAS:.xml=.valid)

-include $(top_srcdir)/git.mk
