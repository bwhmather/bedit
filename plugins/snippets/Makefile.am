if ENABLE_PYTHON

plugins_snippetsdir = $(plugindir)/snippets
plugins_snippets_PYTHON = 				\
	plugins/snippets/snippets/__init__.py 		\
	plugins/snippets/snippets/appactivatable.py 	\
	plugins/snippets/snippets/windowactivatable.py 	\
	plugins/snippets/snippets/document.py 		\
	plugins/snippets/snippets/library.py 		\
	plugins/snippets/snippets/snippet.py 		\
	plugins/snippets/snippets/parser.py 		\
	plugins/snippets/snippets/placeholder.py 	\
	plugins/snippets/snippets/manager.py 		\
	plugins/snippets/snippets/helper.py 		\
	plugins/snippets/snippets/singleton.py 		\
	plugins/snippets/snippets/shareddata.py 	\
	plugins/snippets/snippets/substitutionparser.py \
	plugins/snippets/snippets/importer.py 		\
	plugins/snippets/snippets/exporter.py 		\
	plugins/snippets/snippets/languagemanager.py 	\
	plugins/snippets/snippets/completion.py 	\
	plugins/snippets/snippets/signals.py

plugins_snippets_uidir = $(GEDIT_PLUGINS_DATA_DIR)/snippets/ui
dist_plugins_snippets_ui_DATA = plugins/snippets/snippets/snippets.ui

plugins_snippets_snippetsdir = $(GEDIT_PLUGINS_DATA_DIR)/snippets
dist_plugins_snippets_snippets_DATA =		\
	plugins/snippets/data/chdr.xml		\
	plugins/snippets/data/cpp.xml		\
	plugins/snippets/data/css.xml		\
	plugins/snippets/data/c.xml		\
	plugins/snippets/data/docbook.xml	\
	plugins/snippets/data/fortran.xml	\
	plugins/snippets/data/global.xml	\
	plugins/snippets/data/haskell.xml	\
	plugins/snippets/data/html.xml		\
	plugins/snippets/data/idl.xml		\
	plugins/snippets/data/javascript.xml	\
	plugins/snippets/data/java.xml		\
	plugins/snippets/data/latex.xml		\
	plugins/snippets/data/mallard.xml	\
	plugins/snippets/data/markdown.xml	\
	plugins/snippets/data/perl.xml		\
	plugins/snippets/data/php.xml		\
	plugins/snippets/data/python.xml	\
	plugins/snippets/data/ruby.xml		\
	plugins/snippets/data/sh.xml		\
	plugins/snippets/data/snippets.xml	\
	plugins/snippets/data/tcl.xml		\
	plugins/snippets/data/xml.xml		\
	plugins/snippets/data/xslt.xml

plugins_snippets_langdir = $(GEDIT_PLUGINS_DATA_DIR)/snippets/lang
dist_plugins_snippets_lang_DATA = plugins/snippets/data/lang/snippets.lang

plugin_in_files += plugins/snippets/snippets.plugin.desktop.in

else

dist_plugin_in_files += plugins/snippets/snippets.plugin.desktop.in

endif
