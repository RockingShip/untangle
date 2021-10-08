AC_INIT([untangle], [2.10.2])
AC_CONFIG_SRCDIR([configure.ac])
AC_CONFIG_AUX_DIR([m4])
AC_CONFIG_MACRO_DIRS([m4])
AM_INIT_AUTOMAKE([foreign])
AM_CONFIG_HEADER([config.h])
LT_INIT(disable-shared)

AC_PROG_INSTALL
AM_PROG_AS
AC_PROG_CC
AC_PROG_CXX
AC_PROG_LN_S

AC_CHECK_FUNCS([mmap])

AC_ARG_WITH([jansson], [AS_HELP_STRING([--without-jansson], [disable support for jansson])], [], [with_jansson=test])

AS_IF([test "x$with_jansson" != "xno"], [
	PKG_CHECK_MODULES([LIBJANSSON], [jansson], [], [:])
])
if test "x$LIBJANSSON_LIBS" != "x"; then
	ENABLE_JANSSON="yes"
	AC_DEFINE([ENABLE_JANSSON], [1], [Enable use of libjansson])
else
	ENABLE_JANSSON="no"
fi

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

echo "
untangle configuration:
  jansson: ${ENABLE_JANSSON}
"
echo "You can now run 'make' and 'make install'"
