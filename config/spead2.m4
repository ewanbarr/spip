dnl @synopsis SWIN_LIB_SPEAD2
dnl
AC_DEFUN([SWIN_LIB_SPEAD2],
[
  AC_PROVIDE([SWIN_LIB_SPEAD2])

  AC_REQUIRE([SWIN_PACKAGE_OPTIONS])
  SWIN_PACKAGE_OPTIONS([spead2])

  AC_MSG_CHECKING([for SPEAD2 Library installation])

  if test x"$SPEAD2" == x; then
    SPEAD2=spead2
  fi

  if test "$have_spead2" != "user disabled"; then

    SWIN_PACKAGE_FIND([spead2],[spead2/udp_recv.h])
    SWIN_PACKAGE_TRY_COMPILE([spead2],[#include <spead2/udp_recv.h>])

    SWIN_PACKAGE_FIND([spead2],[lib$SPEAD2.*])
    SWIN_PACKAGE_TRY_LINK([spead2],[#include <spead2/udp_recv.h>],
                          [ spead2::recv::udp_reader reader;],
                          [-l$SPEAD2 -lboost_system])

  fi

  AC_MSG_RESULT([$have_spead2])

  if test x"$have_spead2" = xyes; then

    AC_DEFINE([HAVE_SPEAD2],[1],
              [Define if the SPEAD2 Library is present])
    [$1]

  else
    :
    [$2]
  fi

  SPEAD2_LIBS="$spead2_LIBS"
  SPEAD2_CFLAGS="$spead2_CFLAGS"

  AC_SUBST(SPEAD2_LIBS)
  AC_SUBST(SPEAD2_CFLAGS)
  AM_CONDITIONAL(HAVE_SPEAD2,[test "$have_spead2" = yes])

])

