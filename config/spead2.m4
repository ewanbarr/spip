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

    # spead2 requires C++
    AC_LANG_PUSH(C++)

    ac_tmp_CXXFLAGS="$CXXFLAGS"
    CXXFLAGS="$ac_tmp_CXXFLAGS -std=c++11"

    SWIN_PACKAGE_FIND([spead2],[spead2/recv_udp.h])
    SWIN_PACKAGE_TRY_COMPILE([spead2],[#include <spead2/recv_udp.h>])

    CXXFLAGS="$ac_tmp_CXXFLAGS $spead2_CFLAGS -std=c++11"

    SWIN_PACKAGE_FIND([spead2],[lib$SPEAD2.*])
    SWIN_PACKAGE_TRY_LINK([spead2],[#include <spead2/common_defines.h>],
                      [spead2::item_pointer_t sort_mask],
                      [-l$SPEAD2 -lboost_system])

    CXXFLAGS="$ac_tmp_CXXFLAGS"

    AC_LANG_POP()

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
  SPEAD2_CFLAGS="$spead2_CFLAGS -std=c++11"

  AC_SUBST(SPEAD2_LIBS)
  AC_SUBST(SPEAD2_CFLAGS)
  AM_CONDITIONAL(HAVE_SPEAD2,[test "$have_spead2" = yes])

])

