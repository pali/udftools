lib_LTLIBRARIES     = libudffs.la
libudffs_la_SOURCES = crc.c extent.c desc.c file.c unicode.c ../include/getopt.h ../include/libudffs.h ../include/ecma_167.h ../include/osta_udf.h ../include/defaults.h ../include/udf_endian.h ../include/bswap.h
EXTRA_libudffs_la_SOURCES = getopt.c
libudffs_la_LIBADD = @AC_LIBOBJ@
libudffs_la_LDFLAGS = -version-info 1:0:0

AM_CPPFLAGS = -D_LARGEFILE64_SOURCE
