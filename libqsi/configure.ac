AC_INIT([qsiapi],[6.4.1],[support@qsimaging.com],[qsiapi], www.qsimaging.com)
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])

AC_CHECK_HEADER([tiff.h],,[AC_MSG_ERROR([Error: unable to locate libtiff development headers. Please install libtiff-devel and try again. http://www.libtiff.org/])])

AM_INIT_AUTOMAKE([-Wall -Werror])
AC_COPYRIGHT(qsicopyright.txt)
AC_PROG_CXX
AC_LANG([C++])

LT_PREREQ([2.2])
LT_INIT
LT_LANG([C++])

dnl Check for usb stack selection
dnl Default is libftdi (open source).
dnl Use the configure option --enable-libftd2xx for the ftd2xx stack from FTDI corp.
dnl --------------------------------------------------------------------------------
LIBFTD2XX_REQUIRED=1.0.4
LIBFTDI_REQUIRED=0.19

AC_ARG_ENABLE(libftd2xx,
        AS_HELP_STRING([--enable-libftd2xx],[Enable libftd2xx support]),
        [ if test "$enableval" = "yes"; then
                user_enabled_ftd2xx=1
          fi ],
        [ user_enabled_ftd2xx=0 ])

AC_MSG_CHECKING(if libftd2xx support is enabled)
if test "$user_enabled_ftd2xx" = 1; then
        AC_MSG_RESULT(yes)
        enable_ftd2xx="yes"
        FTD2XX_CFLAGS="-I/usr/local/include"
        FTD2XX_LIBS="-L/usr/local/lib -lftd2xx"
        AC_SUBST(FTD2XX_CFLAGS) 
        AC_SUBST(FTD2XX_LIBS)        
else
        AC_MSG_RESULT(no)
        enable_ftd2xx="no"               
fi

dnl---------------------------------------------------------------------------------
AC_ARG_ENABLE(libftdi,
        AS_HELP_STRING([--enable-libftdi],[Enable libftdi support]),
        [ if test "$enableval" = "yes"; then
                user_enabled_ftdi=1
          fi ],
        [ user_enabled_ftdi=0 ])

AC_MSG_CHECKING(if libftdi support is enabled)
if test "$user_enabled_ftdi" = 1; then
        AC_MSG_RESULT(yes)
        enable_ftdi="yes"
else
		if test $enable_ftd2xx = "yes"; then
			AC_MSG_RESULT(no)
			enable_ftdi="no"
		else
			AC_MSG_RESULT(yes)
			enable_ftdi="yes"
		fi    
fi

if test x$enable_ftdi = xyes; then
        FTDI_CFLAGS="-I/usr/local/include"
        FTDI_LIBS="-L/usr/local/lib -lftdi"
        AC_SUBST(FTDI_CFLAGS) 
        AC_SUBST(FTDI_LIBS) 
fi

AM_CONDITIONAL(ENABLE_LIBFTDI, [test x$enable_ftdi = xyes])
AM_CONDITIONAL(ENABLE_LIBFTD2XX, [test x$enable_ftd2xx = xyes])

dnl --------------------------------------------------------------------------------
 
AC_CONFIG_FILES([libqsiapi-config])
AC_CONFIG_COMMANDS([default],[chmod a+x libqsiapi-config],[])
AC_CONFIG_FILES(Makefile doc/Makefile lib/Makefile src/Makefile demo_src/Makefile)

AC_OUTPUT
