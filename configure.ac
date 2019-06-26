dnl Process this file with autoconf to produce a configure script.
AC_PREREQ([2.64])
AC_INIT(udftools, 2.1, , , [https://github.com/pali/udftools/])
AC_CONFIG_AUX_DIR(config)
AM_CONFIG_HEADER(include/config.h:include/config.in)
AM_INIT_AUTOMAKE

dnl Checks for programs.
AC_PROG_CC_C99
AC_DISABLE_SHARED
AM_PROG_LIBTOOL

AS_IF([test "$ac_cv_prog_cc_c99" = "no"], [AC_MSG_ERROR([Your C compiler does not support ISO C99.])])

dnl Checks for libraries.
AC_CHECK_LIB(readline, readline, [ ])

dnl Checks for header files.
AC_CHECK_HEADERS(readline/readline.h)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_C_BIGENDIAN
AC_SYS_LARGEFILE

PKG_PROG_PKG_CONFIG
PKG_CHECK_MODULES(UDEV, [udev], [ac_cv_udevdir=`$PKG_CONFIG --variable=udevdir udev`])
AC_SUBST(UDEVDIR, $ac_cv_udevdir)

dnl Checks for library functions.
AC_SUBST(LTLIBOBJS)

AM_CONDITIONAL(USE_READLINE, test "$ac_cv_lib_readline_readline" = "yes" -a "$ac_cv_header_readline_readline_h" = "yes")

AC_CONFIG_FILES(Makefile libudffs/Makefile mkudffs/Makefile cdrwtool/Makefile pktsetup/Makefile udffsck/Makefile udfinfo/Makefile udflabel/Makefile wrudf/Makefile doc/Makefile)

AC_OUTPUT
