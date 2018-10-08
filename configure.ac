# https://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html#Updating-version-info
m4_define([lft_major_version], [0])
m4_define([lft_minor_version], [9])
m4_define([lft_micro_version], [8])
m4_define([libfaketime_version],
	[lft_major_version.lft_minor_version.lft_micro_version])

AC_PREREQ([2.68])
AC_INIT([libfaketime], [libfaketime_version])
AC_SUBST([LIBTOOL_DEPS])
#AC_CONFIG_MACRO_DIRS([m4])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AM_CONDITIONAL(MACOS, test `uname` = "Darwin")
AM_CONDITIONAL(SOLARIS, test `uname` = "SunOS")

eval PREFIX=`test "$prefix" = NONE && prefix=$ac_default_prefix; eval echo "${prefix}"`
eval LIBPREFIX="$PREFIX/lib"
LIBFAKETIME_MAJOR_VERSION=lft_major_version
AC_SUBST(PREFIX)
AC_SUBST(LIBPREFIX)
AC_SUBST(LIBVERSION)
AC_DEFINE_UNQUOTED(PREFIX, "$PREFIX", [The install prefix for the package])
AC_DEFINE_UNQUOTED(LIBPREFIX, "$LIBPREFIX", [The install prefix for libraries])
AC_DEFINE(LIBVERSION, ["lft_major_version"], [The library version number])

AC_CONFIG_SRCDIR([src])
AC_CONFIG_HEADERS([config.h])

# libtool versioning
LIBFAKETIME_VERSION=libfaketime_version
AC_SUBST(LIBFAKETIME_VERSION)


# Checks for programs.
AC_PROG_CC
AM_PROG_AR

# Checks for libraries.
AC_CHECK_LIB([c], [timespecadd], [timespecmul])

## FORESHADOWING...
## AC_CHECK_LIB([cunit], [CU_assertImplementation])

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h sys/param.h unistd.h stdbool.h time.h netinet/in.h math.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT32_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([memset])

LT_INIT
AC_CONFIG_FILES([Makefile src/Makefile test/Makefile man/Makefile])
AC_OUTPUT
