sbin_PROGRAMS = mkudffs
mkudffs_LDADD = $(top_builddir)/libudffs/libudffs.la
mkudffs_SOURCES = main.c mkudffs.c defaults.c file.c options.c mkudffs.h defaults.h file.h options.h ../include/ecma_167.h ../include/osta_udf.h ../include/libudffs.h ../include/bswap.h

AM_CPPFLAGS = -I$(top_srcdir)/include

install-exec-hook:
	cd "$(DESTDIR)$(sbindir)" && ln -sf mkudffs$(EXEEXT) mkfs.udf$(EXEEXT)

uninstall-hook:
	cd "$(DESTDIR)$(sbindir)" && rm -f mkfs.udf$(EXEEXT)
