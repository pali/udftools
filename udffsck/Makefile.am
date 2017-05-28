if WORDS_LITTLEENDIAN
sbin_PROGRAMS = udffsck
udffsck_LDADD = $(top_builddir)/libudffs/libudffs.la
udffsck_SOURCES = main.c utils.c utils.h udffsck.c udffsck.h options.c options.h udf.h ../include/ecma_167.h ../include/osta_udf.h ../mkudffs/mkudffs.h ../mkudffs/defaults.h ../mkudffs/file.h ../libudffs/crc.c ../include/libudffs.h ../include/udf_endian.h

AM_CFLAGS = -I$(top_srcdir)/include 
AM_LDFLAGS = -lm

if DEBUG
test_SOURCES = test.c
test_LDFLAGS = -lcmocka -lm
test_CFLAGS = -DBASIC_TESTS -fsanitize=address -DDEBUG 
noinst_PROGRAMS = test

testextra_SOURCES = test.c
testextra_LDFLAGS = -lcmocka -lm
testextra_CFLAGS = -DEXTRA_TESTS -fsanitize=address -DDEBUG 
noinst_PROGRAMS += testextra

AM_CFLAGS += -fsanitize=address -DDEBUG 
endif
endif
