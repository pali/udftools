bin_PROGRAMS = wrudf
wrudf_LDADD = $(top_builddir)/libudffs/libudffs.la -lreadline -lncurses
wrudf_SOURCES = wrudf.c wrudf-cmnd.c wrudf-desc.c wrudf-cdrw.c wrudf-cdr.c ide-pc.c wrudf.h ide-pc.h ../include/ecma_167.h ../include/osta_udf.h ../include/bswap.h

AM_CPPFLAGS = -D_LARGEFILE64_SOURCE -D_GNU_SOURCE -DDEBUG
