prefix=@CMAKE_INSTALL_PREFIX@
exec_prefix=@CMAKE_INSTALL_PREFIX@
libdir=@LIB_DESTINATION@
includedir=@INCLUDE_INSTALL_DIR@

Name: libindi
Description: Instrument Neutral Distributed Interface
URL=http://www.indilib.org/
Version: @CMAKE_INDI_VERSION_STRING@
Libs: -L@LIB_DESTINATION@ -lindi
Cflags: -I@INCLUDE_INSTALL_DIR@ -@INCLUDE_INSTALL_DIR@/libindi
