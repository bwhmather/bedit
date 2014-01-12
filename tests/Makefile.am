tests_progs_cppflags =		\
	-I$(top_srcdir)		\
	-I$(top_srcdir)/gedit

tests_progs_cflags = $(GEDIT_CFLAGS)
tests_progs_ldadd = $(top_builddir)/gedit/libgedit.la

TESTS                                += tests/document-input-stream
tests_document_input_stream_SOURCES   = tests/document-input-stream.c
tests_document_input_stream_LDADD     = $(tests_progs_ldadd)
tests_document_input_stream_CPPFLAGS  = $(tests_progs_cppflags)
tests_document_input_stream_CFLAGS    = $(tests_progs_cflags)

TESTS                                 += tests/document-output-stream
tests_document_output_stream_SOURCES   = tests/document-output-stream.c
tests_document_output_stream_LDADD     = $(tests_progs_ldadd)
tests_document_output_stream_CPPFLAGS  = $(tests_progs_cppflags)
tests_document_output_stream_CFLAGS    = $(tests_progs_cflags)

TESTS                          += tests/document-loader
tests_document_loader_SOURCES   = tests/document-loader.c
tests_document_loader_LDADD     = $(tests_progs_ldadd)
tests_document_loader_CPPFLAGS  = $(tests_progs_cppflags)
tests_document_loader_CFLAGS    = $(tests_progs_cflags)

TESTS                         += tests/document-saver
tests_document_saver_SOURCES   = tests/document-saver.c
tests_document_saver_LDADD     = $(tests_progs_ldadd)
tests_document_saver_CPPFLAGS  = $(tests_progs_cppflags)
tests_document_saver_CFLAGS    = $(tests_progs_cflags)

EXTRA_DIST += tests/setup-document-saver.sh
