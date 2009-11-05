# -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT(iguanaIR,
  esyscmd(grep Version: ../packaging/fedora/iguanaIR.spec | sed 's/Version: *//' | tr -d '\n'),
  support@iguanaworks.net)
AC_PREFIX_DEFAULT([/usr])
AC_CONFIG_SRCDIR([daemon.c])
AC_CONFIG_HEADER([config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET

# Checks for libraries.
AC_CHECK_LIB([popt], [poptGetContext],,
  [
    echo "ERROR: Could not find popt."
    echo "       You probably need to install the popt development package.  In Ubuntu the package is libpopt-dev, in Fedora install popt-devel."
  ]
)
AC_CHECK_LIB([pthread], [pthread_create])
AC_CHECK_LIB([rt], [clock_gettime],
  [
    RTLIB=-lrt
    AC_SUBST([RTLIB])
  ]
)

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
AC_CHECK_FUNCS([floor gettimeofday memset mkdir select socket strchr strdup strerror strrchr strspn strstr mach_absolute_time])

# Some default environment variable values
CFLAGS="-Wall -pedantic"

# use -Wextra when possible
AX_CFLAGS_GCC_OPTION(-Wextra)

# TODO: really just want this when we NEED -fPIC
# AX_CFLAGS_GCC_OPTION(-fPIC)

# enable and disable debugging
AC_ARG_ENABLE(debug,
  [  --enable-debug configure with debugging features enabled ],
  [ if test x$enableval = xyes; then
      ENABLE_DEBUG_FLAGS=yes
    fi ]
)
if test x$ENABLE_DEBUG_FLAGS = xyes; then
  CPPFLAGS="$CPPFLAGS -D_DEBUG"
  CFLAGS="$CFLAGS -g"
else
  CFLAGS="$CFLAGS -O2"
fi

# Allow the user to override clock_gettime detection
AC_ARG_ENABLE(clock_gettime,
  [  --disable-clock_gettime do not use clock_gettime even if it exists],
  [ if test x$enableval = xno; then
        DISABLE_CLOCK_GETTIME=yes
    fi ],
)
if ! test x$DISABLE_CLOCK_GETTIME = xyes; then
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
    # Older versions of libusb do not play with threads at all
    # nicely.  This driver was actually developed on such a version
    # of libusb.  However, in later versions threads seem to not eat
    # the responses for URBs intended for other threads.  By
    # defining LIBUSB_NO_THREADS you can request the slower (around
    # 1/4 the speed for USB transaction heavy workloads), but safer,
    # behavior where 2 threads never use the same libusb object at
    # once.
    #
    # version 0.1.8 and earlier need more than this (i.e. NOT SUPPORTED)
    # version 0.1.10a and earlier need this
    # version 0.1.11 and later do not need this;
    #                works with kernel-2.6.5 and later
    # version 0.1.12 only works with kernel-2.6.10 and later
    #
    if test $VERSION -lt 111; then
      AC_DEFINE(LIBUSB_NO_THREADS, 1, [whether to use threads with libusb])
    else
      AC_ARG_ENABLE(no-threads,
          [  --disable-no-threads enable the "no threads" option ],
          [ if test "x$enableval" == "xno"; then
                LIBUSB_THREADS_OPTIONAL=no
            fi
          ]
      )
      if test x$LIBUSB_THREADS_OPTIONAL != xno; then
        AC_DEFINE(LIBUSB_NO_THREADS, 1, [whether to use threads with libusb])
        AC_DEFINE(LIBUSB_NO_THREADS_OPTION, 1,
                  [whether to makes threads optional with libusb])
      fi
    fi

    # Want to use the constantly incrementing devnum if it exists.
    # This functionality was introduced in version 0.1.9 of libusb.
    #
    if test $VERSION -lt 109; then
      AC_DEFINE(LIBUSB_DEVNUM(dev), (dev->bus->location),
                                    [what to use for the libusb devnum])
    else
      AC_DEFINE(LIBUSB_DEVNUM(dev), (dev->devnum),
                                    [what to use for the libusb devnum])
    fi
  ],
  [
    echo "ERROR: Could not find libusb."
    echo "       You probably need to install the libusb development package.  In Ubuntu the package is libusb-dev, in Fedora install libusb-devel."
  ]
)

# Allow the user to override python detection
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

# look for swig
AC_PROG_SWIG(1.3.31)

# Output some files
AC_CONFIG_FILES([Make.settings Makefile drivers/Makefile])
AC_OUTPUT(autoconf/stamp-h)
