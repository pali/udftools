lib_LTLIBRARIES     = libudffs.la
libudffs_la_SOURCES = crc.c extent.c file.c unicode.c ../include/getopt.h ../include/libudffs.h ../include/ecma_167r3.h ../include/osta_udf201.h ../include/defaults.h ../include/udf_endian.h
EXTRA_libudffs_la_SOURCES = getopt.c
libudffs_la_LIBADD = @LIBOBJS@
libudffs_la_LDFLAGS = -version-info 1:0:0

AM_CPPFLAGS = -D_LARGEFILE64_SOURCE
