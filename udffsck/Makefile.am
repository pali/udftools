if WORDS_LITTLEENDIAN
sbin_PROGRAMS = udffsck
udffsck_LDADD = $(top_builddir)/libudffs/libudffs.la
udffsck_SOURCES = main.c utils.c utils.h udffsck.c udffsck.h options.c options.h log.c log.h ../include/ecma_167.h ../include/osta_udf.h ../mkudffs/mkudffs.h ../mkudffs/defaults.h ../mkudffs/file.h ../libudffs/crc.c ../include/libudffs.h ../include/udf_endian.h

AM_CFLAGS = -I$(top_srcdir)/include 
AM_LDFLAGS = -lm -ldl 
#-Wl,--no-as-needed

if TESTS
test_SOURCES = test.c
test_LDFLAGS = -lcmocka -lm
test_CFLAGS = -DBASIC_TESTS 
noinst_PROGRAMS = test

testextra1_SOURCES = test.c
testextra1_LDFLAGS = -lcmocka -lm
testextra1_CFLAGS = -DEXTRA_TESTS=1 
noinst_PROGRAMS += testextra1

testextra2_SOURCES = test.c
testextra2_LDFLAGS = -lcmocka -lm
testextra2_CFLAGS = -DEXTRA_TESTS=2 
noinst_PROGRAMS += testextra2

unittest_LDADD = $(top_builddir)/libudffs/libudffs.la
unittest_SOURCES = unit-test.c utils.c utils.h udffsck.c udffsck.h options.c options.h log.c log.h ../include/ecma_167.h ../include/osta_udf.h ../mkudffs/mkudffs.h ../mkudffs/defaults.h ../mkudffs/file.h ../libudffs/crc.c ../include/libudffs.h ../include/udf_endian.h
unittest_LDFLAGS = -lcmocka -lm -I$(top_srcdir)/include
#unittest_CFLAGS =  
noinst_PROGRAMS += unittest
endif

if DEBUG
AM_CFLAGS += -fsanitize=address -DDEBUG 
endif
endif
