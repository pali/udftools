noinst_PROGRAMS = udffsck
mkudffs_LDADD = $(top_builddir)/libudffs/libudffs.la
udffsck_SOURCES = main.c utils.c utils.h udffsck.c udffsck.h options.c options.h udf.h ../include/ecma_167.h ../include/osta_udf.h ../mkudffs/mkudffs.h ../mkudffs/defaults.h ../mkudffs/file.h ../libudffs/crc.c ../include/libudffs.h ../include/udf_endian.h

AM_CPPFLAGS = -I$(top_srcdir)/include -D_LARGEFILE64_SOURCE -DBLKSSZGET -DBLKGETSIZE64 -std=c99
