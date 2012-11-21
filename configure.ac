# Build configuration system for nulltty
#
# Mark Shroyer <code@markshroyer.com>
# Wed Nov 21 00:16:32 EST 2012

AC_PREREQ([2.68])

### VERSION INFORMATION ###################################################

# Package name and version number
AC_INIT([nulltty], [0.0.1], [code@markshroyer.com])

###########################################################################

# Make sure ./configure isn't being run from outer space...
AC_CONFIG_SRCDIR([lib/strlcpy.c])

AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])

# Prevent autoconf from molesting CFLAGS.
CFLAGS="$CFLAGS"

AM_INIT_AUTOMAKE([foreign -Wall -Werror])

AC_PROG_CC
AM_PROG_CC_C_O

AC_CONFIG_HEADERS([config.h])
AC_C_INLINE

AC_PROG_RANLIB

# Support explicit debug compilation (with optimization disabled).
AC_MSG_CHECKING([whether to build with debug information])
AC_ARG_ENABLE([debug],
        [AS_HELP_STRING([--enable-debug],
                [enable debug data generation @<:@default=no@:>@])],
        [debugit="$enableval"],
        [debugit=no])
AC_MSG_RESULT([$debugit])

AC_CHECK_FUNC([strlcat], [],
    [AC_CHECK_LIB([bsd], [strlcat], [],
        [AC_REPLACE_FUNCS([strlcat])])])
AC_CHECK_FUNC([strlcpy], [],
    [AC_CHECK_LIB([bsd], [strlcpy], [],
        [AC_REPLACE_FUNCS([strlcpy])])])

AX_CHECK_CFLAGS([-std=c99])
AX_CHECK_CFLAGS([-Wall -Werror])
AX_CHECK_CFLAGS([-pedantic])

if test x"$debugit" = x"yes"; then
    AC_DEFINE([DEBUG], [], [Debug Mode])
    CFLAGS+=" -g3 -O0"
else
    AC_DEFINE([NDEBUG], [], [No-debug Mode])
    CFLAGS+=" -O2"
fi

CFLAGS=$(echo "$CFLAGS" | sed 's/^ *//')

AC_CONFIG_FILES([Makefile
                 lib/Makefile])
AC_OUTPUT

printf "\n"
printf "    _______________________\n"
printf "  =(__    ___      __     _)=\n"
printf "    |                     |\n"
printf "    |                     |\n"
printf "    |                     |\r"
printf "    |  ${PACKAGE_NAME} v${PACKAGE_VERSION}\n"
printf "    |                     |\n"
printf "    |__    ___   __    ___|\n"
printf "  =(_______________________)=\n"
printf "\n"
printf "Install prefix:     $prefix\n"
printf "Compiler:           ${CC}\n"
printf "CFLAGS:             ${CFLAGS}\n"
printf "LIBS:               ${LIBS}\n"
printf "\n"
