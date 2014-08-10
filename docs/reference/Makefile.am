## Process this file with automake to produce Makefile.in

# see /gtk-doc/examples/Makefile.am

# We require automake 1.6 at least.
AUTOMAKE_OPTIONS = 1.6

# The name of the module
DOC_MODULE = gedit

# The top-level XML file (SGML in the past).
DOC_MAIN_SGML_FILE = gedit-docs.xml

# The directory containing the source code. Relative to $(srcdir).
DOC_SOURCE_DIR = ../../gedit

# Extra options to supply to gtkdoc-scan.
# e.g. SCAN_OPTIONS = --deprecated-guards="GTK_DISABLE_DEPRECATED" 
SCAN_OPTIONS = --rebuild-types

# Extra options to supply to gtkdoc-mkdb.
MKDB_OPTIONS = --xml-mode --output-format=xml

# Used for dependencies. The docs will be rebuilt if any of these change.
HFILE_GLOB = $(top_srcdir)/gedit/*.h
CFILE_GLOB = $(top_srcdir)/gedit/*.c

# Header files to ignore when scanning (These are internal to gedit).
IGNORE_HFILES =					\
	gedit-app-osx.h				\
	gedit-app-win32.h			\
	gedit-app-x11.h				\
	gedit-close-confirmation-dialog.h 	\
	gedit-dirs.h				\
	gedit-documents-panel.h			\
	gedit-encodings-dialog.h		\
	gedit-enum-types.h			\
	gedit-file-chooser-dialog.h		\
	gedit-highlight-mode-dialog.h		\
	gedit-highlight-mode-selector.h		\
	gedit-history-entry.h			\
	gedit-io-error-info-bar.h		\
	gedit-marshal.h				\
	gedit-menu-stack-switcher.h		\
	gedit-metadata-manager.h		\
	gedit-multi-notebook.h			\
	gedit-notebook.h			\
	gedit-notebook-popup-menu.h		\
	gedit-notebook-stack-switcher.h		\
	gedit-open-document-selector.h		\
	gedit-open-menu-button.h		\
	gedit-plugins-engine.h			\
	gedit-preferences-dialog.h		\
	gedit-print-job.h			\
	gedit-print-preview.h			\
	gedit-recent.h				\
	gedit-replace-dialog.h			\
	gedit-settings.h			\
	gedit-small-button.h			\
	gedit-status-menu-button.h		\
	gedit-tab-label.h			\
	gedit-view-frame.h			\
	gedit-view-holder.h			\
	gedit-window-private.h

# Images to copy into HTML directory.
HTML_IMAGES =

# Extra SGML files that are included by $(DOC_MAIN_SGML_FILE).
content_files =

# CFLAGS and LDFLAGS for compiling gtkdoc-scangobj with your library.
# Only needed if you are using gtkdoc-scangobj to dynamically query widget
# signals and properties.
GTKDOC_CFLAGS =			\
	-I$(top_srcdir)/gedit	\
	-I$(top_builddir)/gedit \
	-I$(top_srcdir)		\
	-I$(top_builddir)	\
	$(GEDIT_CFLAGS)

GTKDOC_LIBS =						\
	$(top_builddir)/gedit/libgedit.la		\
	$(GEDIT_LIBS)

MAINTAINERCLEANFILES = gedit.types

# This includes the standard gtk-doc make rules, copied by gtkdocize.
include $(top_srcdir)/gtk-doc.make

-include $(top_srcdir)/git.mk
