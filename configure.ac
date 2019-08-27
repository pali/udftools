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
AC_PROG_LN_S
AC_PROG_MKDIR_P

AS_IF([test "$ac_cv_prog_cc_c99" = "no"], [AC_MSG_ERROR([Your C compiler does not support ISO C99.])])

dnl Checks for libraries, by using pkg-config when available
PKG_PROG_PKG_CONFIG
AS_IF([test -n "${PKG_CONFIG}"], [PKG_CHECK_MODULES([READLINE], [readline], [readline_found=yes], [readline_found=no])])

AS_IF([test "${readline_found}" != "yes"],
  [AC_CHECK_LIB(readline, readline,
               [AC_CHECK_HEADERS(readline/readline.h,
		                 [AC_SUBST([READLINE_LIBS], [-lreadline])
				 readline_found=yes],
				 [readline_found=no])],
               [readline_found=no])
  ])

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_C_BIGENDIAN
AC_SYS_LARGEFILE

PKG_CHECK_MODULES(UDEV, [udev], [ac_cv_udevdir=`$PKG_CONFIG --variable=udevdir udev`], [ac_cv_udevdir=""])
AM_CONDITIONAL(UDEVDIR, [test "$ac_cv_udevdir" != ""])
AC_SUBST(UDEVDIR, $ac_cv_udevdir)

dnl Checks for library functions.
AC_SUBST(LTLIBOBJS)

AM_CONDITIONAL(USE_READLINE, test "$readline_found" = "yes")

AC_CONFIG_FILES(Makefile libudffs/Makefile mkudffs/Makefile cdrwtool/Makefile pktsetup/Makefile udffsck/Makefile udfinfo/Makefile udflabel/Makefile wrudf/Makefile doc/Makefile)

AC_OUTPUT
