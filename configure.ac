#
# Process this file with autoconf to produce a configure script.
#
AC_PREREQ(2.59)
AC_INIT(trainCtrl, 1.0.0, chris@theknight.co.uk)
AM_INIT_AUTOMAKE([subdir-objects])
AC_CONFIG_SRCDIR([src/trainCtrl.c])
AC_CONFIG_HEADER([config.h])

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O

PKG_CHECK_MODULES([DEPS], [gtk+-3.0 libxml-2.0])
AC_SUBST(DEPS_CFLAGS)
AC_SUBST(DEPS_LIBS)

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([ctype.h math.h stdio.h stdlib.h string.h time.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_STRUCT_TM
REVISION=1
LIBDIR=${libexecdir}
AC_SUBST([REVISION])
AC_SUBST([LIBDIR])

AC_CONFIG_FILES([Makefile trainCtrl.spec])
AC_CONFIG_FILES([buildrpm.sh],[chmod +x buildrpm.sh])
AC_CONFIG_FILES([builddeb.sh],[chmod +x builddeb.sh])
AC_CONFIG_FILES([debian/control debian/changelog])
AC_OUTPUT

