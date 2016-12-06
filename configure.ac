AC_INIT([libs3], [2.0])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([foreign subdir-objects tar-pax])
AC_PROG_CC
AC_DISABLE_STATIC
LT_INIT
PKG_CHECK_MODULES([curl], [libcurl])
PKG_CHECK_MODULES([xml], [libxml-2.0])
rm -f GNUmakefile
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
