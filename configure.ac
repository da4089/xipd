dnl -*- sh -*-
########################################################################
########################################################################

AC_INIT(xipd.c, "davida@pobox.com")
AC_PREREQ(2.50)

AM_INIT_AUTOMAKE(xipd, 1.0.0) dnl, no_config_h_defines)
AM_CONFIG_HEADER(config.h)


########################################################################

# Check programs

AC_PROG_CC
AC_HEADER_STDC
AC_PROG_LN_S

########################################################################

# Check options

# Elvin

AC_ARG_WITH(elvin,AC_HELP_STRING([--with-elvin=DIR],[prefix for Elvin4 installation]))
if test x"$with_elvin" = xno ; then
    AC_MSG_ERROR([etrap cannot be compiled without Elvin4])
    exit 1
fi

# Locate the elvin-config script
AC_PATH_PROG(ELVIN_CONFIG, elvin-config, not found, $withval/bin:$PATH)
if test "$ELVIN_CONFIG" = "not found" ; then
	echo "WARNING: \`elvin-config' is missing on your system.  It"
	echo "         is normally installed as part of the Elvin4 libraries,"
	echo "         which is available from http://elvin.dstc.com"
	exit 1
fi

# Update our flags to pick up the elvin includes and libraries
CFLAGS="$CFLAGS `$ELVIN_CONFIG --cflags`"
LIBS="`$ELVIN_CONFIG --libs vin4c` $LIBS"

AC_CHECK_LIB(vin4c, elvin_client_connect, true, exit 1)


# File /proc/interrupts exists

have_proc_interrupts=no
AC_CACHE_CHECK([whether /proc/interrupts contains keyboard data],
    ac_cv_have_proc_interrupts,
    [ac_cv_have_proc_interrupts=no
     if grep keyboard /proc/interrupts >/dev/null 2>&1 ; then
       ac_cv_have_proc_interrupts=yes
     fi
    ])
have_proc_interrupts=$ac_cv_have_proc_interrupts
if test "$have_proc_interrupts" = yes; then
    AC_DEFINE(HAVE_PROC_INTERRUPTS,1,[Define if file /proc/interrupts exists])
fi


# Library support for X11

AC_PATH_X
AC_PATH_XTRA

if test "x$no_x" != xyes; then

    # Library support for X11 DPMS Extension
    
    have_dpms=no
    AC_CHECK_HEADER(X11/extensions/dpms.h, [have_dpms=yes])
    
    # if that succeeded, then check for the DPMS code in the libraries
    if test "$have_dpms" = yes; then
    
        xlibs="$X_LIBS $X_PRE_LIBS -lX11 $X_EXTRA_LIBS $LIBS"
    
        # first look in -lXext (this is where it is with XFree86 4.0)
        have_dpms=no
        AC_CHECK_LIB(Xext, DPMSInfo, 
                     [have_dpms=yes 
                      xlibs="$X_LIBS $X_PRE_LIBS -lXext -lX11 $X_EXTRA_LIBS $LIBS"],
                     [true], 
                     [$xlibs])
    
        # if that failed, look in -lXdpms (this is where it was in XFree86 3.x)
        if test "$have_dpms" = no; then
            AC_CHECK_LIB(Xdpms, DPMSInfo,
                         [have_dpms=yes
                          xlibs="$X_LIBS $X_PRE_LIBS -lXdpms -lXext -lX11 $X_EXTRA_LIBS $LIBS"],
                         [true],
                         [$xlibs])
        fi
    fi
    
    # if that succeeded, then we've really got it.
    if test "$have_dpms" = yes; then
        AC_DEFINE(HAVE_DPMS_EXTENSION,1,[Define if X libraries have DPMS extension support])
	LIBS="$xlibs"
	CFLAGS="$CFLAGS $X_CFLAGS"
    fi
fi


########################################################################

# Check headers

AC_CHECK_HEADERS(\
    arpa/inet.h \
    errno.h \
    fcntl.h \
    getopt.h \
    libgen.h \
    netdb.h \
    netinet/in.h \
    stdlib.h \
    stdio.h \
    string.h \
    sys/socket.h \
    sys/stat.h \
    sys/types.h \
)


########################################################################

# Check functions

AC_CHECK_FUNCS(\
    gettimeofday \
    inet_ntop \
)

#  basename

AC_CHECK_LIB(c, basename, [AC_DEFINE(HAVE_BASENAME)], [
  AC_CHECK_LIB(gen, basename, [                          dnl solaris
    AC_DEFINE(HAVE_BASENAME,1,[Define if you have the basename() function])
    LIBS="$LIBS -lgen"
  ], [
    AC_CHECK_LIB(util, basename, [                       dnl openbsd
      AC_DEFINE(HAVE_BASENAME,1,[Define if you have the basename() function])
      LIBS="$LIBS -lutil"
    ], [])
  ])
])
AC_SUBST(HAVE_BASENAME)


########################################################################

AC_CONFIG_FILES(Makefile)
AC_OUTPUT


########################################################################
