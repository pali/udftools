if WORDS_LITTLEENDIAN
noinst_PROGRAMS = udffsck
udffsck_LDADD = $(top_builddir)/libudffs/libudffs.la
udffsck_SOURCES = main.c utils.c utils.h udffsck.c udffsck.h options.c options.h udf.h ../include/ecma_167.h ../include/osta_udf.h ../mkudffs/mkudffs.h ../mkudffs/defaults.h ../mkudffs/file.h ../libudffs/crc.c ../include/libudffs.h ../include/udf_endian.h

test_SOURCES = test.c

AM_CFLAGS = -I$(top_srcdir)/include -std=c99 
AM_LDFLAGS = -lm

if DEBUG
AM_CFLAGS += -fsanitize=address -DDEBUG
noinst_PROGRAMS += test
AM_LDFLAGS += -lcmocka
endif
endif
