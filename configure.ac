#                                               -*- Autoconf -*-
# This files generates the configure file.
#
# If you change this file, you usually should run
# autoreconf -fvi
AC_PREREQ(2.61)

# This is where the version information is set
AC_INIT([SPIP], [0.0.1], [andrew.jameson@gmail.com])

AC_CONFIG_AUX_DIR(config)
# Set this file to one that is in your directory
AC_CONFIG_SRCDIR([src/spip.c])
AC_PROG_LIBTOOL

# Automake initialization
AM_INIT_AUTOMAKE
AM_CONFIG_HEADER([config.h])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AM_PROG_CC_C_O

# Checks for libraries.

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_CONFIG_FILES([Makefile
  src/Makefile
  src/Network/Makefile
  src/Formats/SKA1/Makefile
  src/Formats/MeerKAT/Makefile
  src/Dada/Makefile
  scripts/Makefile
  web/Makefile
  web/controls/Makefile
  web/images/Makefile
  web/js/Makefile
  web/status/Makefile
  web/test/Makefile
  web/timing/Makefile])
AC_OUTPUT 
