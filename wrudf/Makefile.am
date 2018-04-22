bin_PROGRAMS = wrudf
wrudf_LDADD = $(top_builddir)/libudffs/libudffs.la
wrudf_SOURCES = wrudf.c wrudf-cmnd.c wrudf-desc.c wrudf-cdrw.c wrudf-cdr.c ide-pc.c wrudf.h ide-pc.h ../include/ecma_167.h ../include/osta_udf.h ../include/bswap.h

AM_CPPFLAGS = -I$(top_srcdir)/include

if USE_READLINE
wrudf_LDADD += $(READLINE_LIBS)
AM_CPPFLAGS += -DUSE_READLINE
endif
