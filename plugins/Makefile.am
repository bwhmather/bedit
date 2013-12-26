DIST_SUBDIRS = \
	checkupdate	\
	docinfo		\
	externaltools	\
	filebrowser	\
	modelines	\
	pythonconsole	\
	quickopen	\
	snippets	\
	sort		\
	spell 		\
	time		\
	zeitgeist

SUBDIRS = \
	docinfo		\
	filebrowser	\
	modelines	\
	sort		\
	time

if ENABLE_PYTHON
SUBDIRS += \
	pythonconsole	\
	quickopen	\
	snippets
endif

if !OS_WIN32
if ENABLE_PYTHON
SUBDIRS      += externaltools
endif
endif

if ENABLE_ENCHANT
SUBDIRS      += spell
endif

if ENABLE_UPDATER
SUBDIRS      += checkupdate
endif

if ENABLE_ZEITGEIST
SUBDIRS      += zeitgeist
endif

-include $(top_srcdir)/git.mk
