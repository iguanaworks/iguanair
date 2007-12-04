#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT(iguanaIR,
  esyscmd(grep Version: ../iguanaIR.spec | sed 's/Version: *//' | tr -d '\n'),
  support@iguanaworks.net)
AC_CONFIG_SRCDIR([daemon.c])
AC_CONFIG_HEADER([config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET

# Checks for libraries.
AC_CHECK_LIB([popt], [poptGetContext])
AC_CHECK_LIB([pthread], [pthread_create])
AC_CHECK_LIB([rt], [clock_gettime])

# Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_CHECK_HEADERS([float.h limits.h stddef.h stdint.h stdlib.h string.h sys/param.h sys/socket.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_C_INLINE
AC_TYPE_MODE_T
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_STRUCT_TM
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_C_VOLATILE

# Checks for library functions.
AC_FUNC_LSTAT
AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_FUNC_SELECT_ARGTYPES
AC_TYPE_SIGNAL
AC_FUNC_STRFTIME
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([floor gettimeofday memset mkdir select socket strchr strdup strerror strrchr strspn strstr])

# TODO: make this actually check that this is linux and not darwin
CPPFLAGS=" -DLINUX"
CFLAGS="-Wall -pedantic"

# AX_CFLAGS_GCC_OPTION(-Wextra)
# TODO: really just want this when we NEED -fpic
# AX_CFLAGS_GCC_OPTION(-fpic)

# enable and disable debugging
AC_ARG_ENABLE(debug,
  [  --enable-debug configure with debugging features enabled ],
  [ CPPFLAGS+=" -D_DEBUG"
    CFLAGS+=" -g" ],
  [ CFLAGS+=" -O2" ]
)

# Allow the user to override clock_gettime detection
AC_ARG_ENABLE(clock_gettime,
  [  --disable-clock_gettime do not use clock_gettime even if it exists],
  [ if test x$enableval = xno; then
        DISABLE_CLOCK_GETTIME=1
    fi ],
)
if ! test x$DISABLE_CLOCK_GETTIME = x1; then
    AC_CHECK_FUNC(clock_gettime,
        AC_DEFINE(USE_CLOCK_GETTIME, 1, [use the available clock_gettime])
    )
fi

# pull the version of libusb
AC_CHECK_LIB([usb], [usb_get_busses],
  [
    VERSION=0
    for num in `libusb-config --version | tr '.' ' '`
    do
      VERSION=$(($VERSION * 100 + $num))
    done

    AC_DEFINE_UNQUOTED(LIBUSB_VERSION, $VERSION, [version of libusb])
  ]
)

# Allow the user to override clock_gettime detection
AC_ARG_ENABLE(python,
  [  --disable-python do build the Python interface.],
  [ if test x$enableval == xno; then
        USE_PYTHON=no
    fi ],
)
if test x$USE_PYTHON != xno; then
    # fetch the python version
    AC_PYTHON_DEVEL()
fi

# Output some files
AC_CONFIG_FILES([Makefile])
AC_OUTPUT(autoconf/stamp-h)
