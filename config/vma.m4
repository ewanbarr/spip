dnl @synopsis SWIN_LIB_VMA
dnl
AC_DEFUN([SWIN_LIB_VMA],
[
  AC_PROVIDE([SWIN_LIB_VMA])

  AC_REQUIRE([SWIN_PACKAGE_OPTIONS])
  SWIN_PACKAGE_OPTIONS([vma])

  AC_MSG_CHECKING([for VMA Library installation])

  if test x"$VMA" == x; then
    VMA=vma
  fi

  if test "$have_vma" != "user disabled"; then

    SWIN_PACKAGE_FIND([vma],[mellanox/vma_extra.h])
    SWIN_PACKAGE_TRY_COMPILE([vma],[#include <mellanox/vma_extra.h>])

    SWIN_PACKAGE_FIND([vma],[lib$VMA.*])
    SWIN_PACKAGE_TRY_LINK([vma],[#include <mellanox/vma_extra.h>],
                          [ struct vma_api_t *vma_api = vma_get_api(); ],
                          [-l$VMA])

  fi

  AC_MSG_RESULT([$have_vma])

  if test x"$have_vma" = xyes; then

    AC_DEFINE([HAVE_VMA],[1],
              [Define if the VMA Library is present])
    [$1]

  else
    :
    [$2]
  fi

  VMA_LIBS="$vma_LIBS"
  VMA_CFLAGS="$vma_CFLAGS"

  AC_SUBST(VMA_LIBS)
  AC_SUBST(VMA_CFLAGS)
  AM_CONDITIONAL(HAVE_VMA,[test "$have_vma" = yes])

])

