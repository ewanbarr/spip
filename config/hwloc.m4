#
# SWIN_LIB_HWLOC([ACTION-IF-FOUND [,ACTION-IF-NOT-FOUND]])
#
# This m4 macro checks availability of the HWLOC Library
#
# HWLOC_CFLAGS - autoconfig variable with flags required for compiling
# HWLOC_LIBS   - autoconfig variable with flags required for linking
# HAVE_HWLOC   - automake conditional
# HAVE_HWLOC   - pre-processor macro in config.h
#
# This macro tries to get HWLOC cflags and libs using the
# pkg-config program.  If that is not available, it
# will try to link using:
#
#    -lhwloc
#
# ----------------------------------------------------------
AC_DEFUN([SWIN_LIB_HWLOC],
[
  AC_PROVIDE([SWIN_LIB_HWLOC])

  SWIN_PACKAGE_OPTIONS([hwloc])

  AC_MSG_CHECKING([for HWLOC libary installation])

  HWLOC_CFLAGS=`pkg-config --cflags hwloc`
  HWLOC_LIBS=`pkg-config --libs hwloc`

  ac_save_CFLAGS="$CFLAGS"
  ac_save_LIBS="$LIBS"
  LIBS="$ac_save_LIBS $HWLOC_LIBS"
  CFLAGS="$ac_save_CFLAGS $HWLOC_CFLAGS"

  AC_TRY_LINK([#include <hwloc.h>],[hwloc_topology_t topology; hwloc_topology_init(&topology);],
              have_hwloc=yes, have_hwloc=no)

  AC_MSG_RESULT($have_hwloc)

  LIBS="$ac_save_LIBS"
  CFLAGS="$ac_save_CFLAGS"

  if test x"$have_hwloc" = xyes; then
    AC_DEFINE([HAVE_HWLOC], [1], [Define to 1 if you have the HWLOC library])
    [$1]
  else
    AC_MSG_WARN([HWLOC-dependent code will not be compiled.])
    HWLOC_CFLAGS=""
    HWLOC_LIBS=""
    [$2]
  fi

  AC_SUBST(HWLOC_CFLAGS)
  AC_SUBST(HWLOC_LIBS)
  AM_CONDITIONAL(HAVE_HWLOC, [test x"$have_hwloc" = xyes])

])

