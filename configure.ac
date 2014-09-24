#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([pms],[0.50],[kimtjen@gmail.com])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_COPYRIGHT(Copyright (c) 2006-2014 Kim Tore Jensen)
AC_CONFIG_SRCDIR([src/pms.c])
AC_CONFIG_HEADERS([build.h])
AC_CONFIG_MACRO_DIR([m4])

# Checks for programs.
AC_PROG_CC

# Checks for ncurses.
AX_WITH_CURSES
if test "x$ax_cv_ncursesw" != xyes && test "x$ax_cv_ncurses" != xyes; then
    AC_MSG_ERROR([requires either NcursesW or Ncurses library])
fi

# Checks for libmpdclient.
PKG_CHECK_MODULES([libmpdclient], [libmpdclient >= 2])

# Checks for pthreads.
AX_PTHREAD

# Checks for header files.
AC_CHECK_HEADERS([locale.h netdb.h stdlib.h sys/socket.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([gettimeofday memset select setlocale socket])

AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
