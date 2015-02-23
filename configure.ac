#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_INIT([pipecut], p[0.70] [david@NetBSD.org], 
	[pipecut], [http://www.pipecut.org/])
AC_PREREQ([2.68])
AM_INIT_AUTOMAKE([1.11 -Wall -Werror])
AC_PROG_CC
AC_PROG_MAKE_SET

# Put various checks and such here

AC_CANONICAL_HOST
case $host_os in
	*freebsd* )
		AC_MSG_RESULT([Host is FreeBSD])
		CFLAGS="$CFLAGS -I/usr/local/include -I."
		CXXFLAGS="$CXXFLAGS -I/usr/local/include -I."
		LDFLAGS="$LDFLAGS -L/usr/local/lib"
	;;
esac


# Run the Makefile generator (non-autoconf) in sz.
sz-0.9.2/maker
#
## Checks for libraries.
## FIXME: Replace `main' with a function in `-lsz':
AC_CHECK_LIB([sqlite3], [sqlite3_initialize], [ ], [
	AC_MSG_RESULT([TESTING TESTING])
	AC_MSG_RESULT([$CFLAGS])
	AC_MSG_RESULT([$LDFLAGS])
	AC_MSG_ERROR([Can't find sqlite3 library])
]) 

## FIXME: Replace `main' with a function in `-lvers':
#AC_CHECK_LIB([vers], [main])
#
## Checks for header files.
AC_CHECK_HEADERS([bsd/string.h])
AC_CHECK_HEADERS([sys/queue.h])
# From TRE development cycle AC_CHECK_HEADERS([tre-0.8.0/lib/regex.h])
AC_CHECK_HEADERS([fcntl.h limits.h stddef.h stdlib.h string.h termios.h unistd.h sqlite3.h])
#
## Checks for typedefs, structures, and compiler characteristics.
#AC_CHECK_HEADER_STDBOOL
#AC_TYPE_SIZE_T
#
## Checks for library functions.
##AC_FUNC_FORK
##AC_FUNC_MALLOC
##AC_FUNC_REALLOC
##AC_CHECK_FUNCS([dup2 memchr memmove memset regcomp strchr strdup strpbrk strspn strtol])
#
# From TRE development cycle AM_CONDITIONAL(TRE_AGREP, false)
#
#
##     checks for programs
##     checks for libraries
##     checks for header files
##     checks for types
##     checks for structures
##     checks for compiler characteristics
##     checks for library functions
##     checks for system services
#
#AC_CHECK_HEADER(sz.h)
AC_CONFIG_SRCDIR([pipecut.c])
AC_CONFIG_HEADERS([config.h])
# From TRE development cycle AC_CONFIG_FILES([tre-0.8.0/Makefile])
AC_CONFIG_FILES([Makefile])
AC_OUTPUT




