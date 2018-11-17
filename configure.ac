dnl Process this file with autoconf to produce a configure script.
AC_PREREQ([2.64])
AC_INIT(udftools, 2.0, , , [https://github.com/pali/udftools/])
AC_CONFIG_AUX_DIR(config)
AM_CONFIG_HEADER(include/config.h:include/config.in)
AM_INIT_AUTOMAKE

dnl Checks for programs.
AC_PROG_CC_C99
AC_DISABLE_SHARED
AM_PROG_LIBTOOL

if test "$ac_cv_prog_cc_c99" = "no"; then
	AC_MSG_ERROR([Your C compiler does not support ISO C99.])
fi

dnl Checks for libraries.
AC_CHECK_LIB(readline, readline, [ ])

dnl Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS(fcntl.h sys/time.h unistd.h getopt.h string.h readline/readline.h)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_C_BIGENDIAN
AC_HEADER_TIME
AC_STRUCT_TM
AC_SYS_LARGEFILE

PKG_PROG_PKG_CONFIG
PKG_CHECK_MODULES(UDEV, [udev], [ac_cv_udevdir=`$PKG_CONFIG --variable=udevdir udev`])
AC_SUBST(UDEVDIR, $ac_cv_udevdir)

dnl Checks for library functions.
AC_SUBST(LTLIBOBJS)

AM_CONDITIONAL(USE_READLINE, test "$ac_cv_lib_readline_readline" = "yes" -a "$ac_cv_header_readline_readline_h" = "yes")

AC_CONFIG_FILES(Makefile libudffs/Makefile mkudffs/Makefile cdrwtool/Makefile pktsetup/Makefile udffsck/Makefile udfinfo/Makefile udflabel/Makefile wrudf/Makefile doc/Makefile)

AC_ARG_ENABLE(debug,
AS_HELP_STRING([--enable-debug],
[enable debugging, default: no]),
[case "${enableval}" in
    yes) debug=true ;;
    no)  debug=false ;;
    *)   AC_MSG_ERROR([bad value ${enableval} for --enable-debug]) ;;
esac],
[debug=false])

AM_CONDITIONAL(DEBUG, test x"$debug" = x"true")

AC_ARG_ENABLE(tests,
AS_HELP_STRING([--enable-tests],
[enable tests building, default: no]),
[case "${enableval}" in
    yes) tests=true ;;
    no)  tests=false ;;
    *)   AC_MSG_ERROR([bad value ${enableval} for --enable-tests]) ;;
esac],
[tests=false])

AM_CONDITIONAL(TESTS, test x"$tests" = x"true")

AM_CONDITIONAL(WORDS_LITTLEENDIAN, test "x$ac_cv_c_bigendian" = "xno")
AM_CONDITIONAL(WORDS_BIGENDIAN, test "x$ac_cv_c_bigendian" = "xyes")

AC_OUTPUT
