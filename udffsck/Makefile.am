noinst_PROGRAMS = udffsck test
udffsck_LDADD = $(top_builddir)/libudffs/libudffs.la
udffsck_SOURCES = main.c utils.c utils.h udffsck.c udffsck.h options.c options.h udf.h ../include/ecma_167.h ../include/osta_udf.h ../mkudffs/mkudffs.h ../mkudffs/defaults.h ../mkudffs/file.h ../libudffs/crc.c ../include/libudffs.h ../include/udf_endian.h

test_SOURCES = test.c udffsck.c udffsck.h ../include/ecma_167.h ../include/osta_udf.h ../mkudffs/mkudffs.h ../mkudffs/defaults.h ../mkudffs/file.h ../libudffs/crc.c ../include/libudffs.h ../include/udf_endian.h

if DEBUG
AM_CFLAGS = -I$(top_srcdir)/include -D_LARGEFILE64_SOURCE -DBLKSSZGET -DBLKGETSIZE64 -std=c99  -fsanitize=address
else
AM_CFLAGS = -I$(top_srcdir)/include -D_LARGEFILE64_SOURCE -DBLKSSZGET -DBLKGETSIZE64 -std=c99
endif

AM_LDFLAGS = -lcmocka

