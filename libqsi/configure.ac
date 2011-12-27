AC_INIT([qsiapi],[6.0.3],[support@qsimaging.com],[qsiapi])
AC_CONFIG_AUX_DIR([build-aux])

AC_CHECK_HEADER([tiff.h],,[AC_MSG_ERROR([Error: unable to locate libtiff development headers. Please install libtiff-devel and try again. http://www.libtiff.org/])])

#AC_CHECK_LIB([ftd2xx],[FT_Open])
 
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([-Wall -Werror])
AC_COPYRIGHT(qsicopyright.txt)
AC_PROG_CXX
AC_LANG([C++])
AC_SUBST(AM_CXXFLAGS, [-g -O0])
LT_INIT
LT_LANG([C++])

AC_CONFIG_FILES([libqsiapi-config])
AC_CONFIG_COMMANDS([default],[chmod a+x libqsiapi-config],[])
AC_CONFIG_FILES(Makefile doc/Makefile lib/Makefile src/Makefile demo_src/Makefile)

AC_OUTPUT
