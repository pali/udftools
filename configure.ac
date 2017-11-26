dnl Process this file with autoconf to produce a configure script.
AC_INIT(udftools, 1.3, , , [https://github.com/pali/udftools/])
AC_CONFIG_AUX_DIR(config)
AM_CONFIG_HEADER(include/config.h:include/config.in)
AM_INIT_AUTOMAKE

dnl Checks for programs.
AC_PROG_CC
AC_DISABLE_SHARED
AM_PROG_LIBTOOL

dnl Checks for libraries.
AC_CHECK_LIB(readline, readline, , AC_MSG_ERROR([cannot find -lreadline.]))

dnl Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS(fcntl.h malloc.h sys/time.h unistd.h endian.h getopt.h sys/isa_defs.h string.h strings.h)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_HEADER_TIME
AC_STRUCT_TM
AC_SYS_LARGEFILE

dnl Checks for library functions.
AC_SUBST(LTLIBOBJS)

AC_CONFIG_FILES(Makefile libudffs/Makefile mkudffs/Makefile cdrwtool/Makefile pktsetup/Makefile udffsck/Makefile udfinfo/Makefile wrudf/Makefile doc/Makefile)

AC_OUTPUT
