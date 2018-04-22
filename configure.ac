dnl Process this file with autoconf to produce a configure script.
AC_PREREQ([2.63])
AC_INIT(udftools, 2.0, , , [https://github.com/pali/udftools/])
AC_CONFIG_AUX_DIR(config)
AM_CONFIG_HEADER(include/config.h:include/config.in)
AM_INIT_AUTOMAKE([subdir-objects])

dnl Checks for programs.
AC_PROG_CC_C99
AC_DISABLE_SHARED
AM_PROG_LIBTOOL

if test "$ac_cv_prog_cc_c99" = "no"; then
	AC_MSG_ERROR([Your C compiler does not support ISO C99.])
fi

PKG_PROG_PKG_CONFIG

dnl Checks for libraries, by using pkg-config when available
if test -n "${PKG_CONFIG}" ; then
  PKG_CHECK_MODULES([READLINE], [readline], [readline_found=yes], [readline_found=no])
fi

if test "${readline_found}" != "yes" ; then
  AC_CHECK_LIB(readline, readline,
               [AC_SUBST([READLINE_LIBS], [-lreadline])],
               [])
fi

dnl Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS(fcntl.h sys/time.h unistd.h getopt.h string.h readline/readline.h)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_C_BIGENDIAN
AC_HEADER_TIME
AC_STRUCT_TM
AC_SYS_LARGEFILE

dnl Checks for library functions.
AC_SUBST(LTLIBOBJS)

AM_CONDITIONAL(USE_READLINE, test "$ac_cv_lib_readline_readline" = "yes" -a "$ac_cv_header_readline_readline_h" = "yes")

AC_CONFIG_FILES(Makefile libudffs/Makefile mkudffs/Makefile cdrwtool/Makefile pktsetup/Makefile udffsck/Makefile udfinfo/Makefile udflabel/Makefile wrudf/Makefile doc/Makefile)

AC_OUTPUT
