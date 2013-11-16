cmake_minimum_required(VERSION 2.4.7)
PROJECT(libfishcamp C CXX)

set(LIBFISHCAMP_VERSION "1.0")
set(LIBFISHCAMP_SOVERSION "1")

set(FIRMWARE_INSTALL_DIR "/etc/fishcamp")
set(LIB_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/lib")

IF(UNIX AND NOT WIN32 AND NOT APPLE)
 IF(CMAKE_SIZEOF_VOID_P MATCHES "8")
    SET(LIB_64 TRUE)
    MARK_AS_ADVANCED(LIB_64)
 ENDIF(CMAKE_SIZEOF_VOID_P MATCHES "8")
ENDIF(UNIX AND NOT WIN32 AND NOT APPLE)
  IF(NOT DEFINED LIB_64)
     SET(LIB_64 "")
  ENDIF(NOT DEFINED LIB_64)


if (LIB_64)
exec_program(cp ARGS ${CMAKE_SOURCE_DIR}/libfishcamp64.bin ${CMAKE_BINARY_DIR}/libFcLinux.so.${LIBFISHCAMP_VERSION})
else (LIB_64)
exec_program(cp ARGS ${CMAKE_SOURCE_DIR}/libfishcamp32.bin ${CMAKE_BINARY_DIR}/libFcLinux.so.${LIBFISHCAMP_VERSION})
endif(LIB_64)

# Make sure symbolic links are installed
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/make_libfishcamp_symlink.cmake "
exec_program(${CMAKE_COMMAND} ARGS -E create_symlink ${LIB_INSTALL_DIR}${LIB_POSTFIX}/libFcLinux.so.${LIBFISHCAMP_VERSION} ${LIB_INSTALL_DIR}${LIB_POSTFIX}/libFcLinux.so.${LIBFISHCAMP_SOVERSION})\n
exec_program(${CMAKE_COMMAND} ARGS -E create_symlink ${LIB_INSTALL_DIR}${LIB_POSTFIX}/libFcLinux.so.${LIBFISHCAMP_SOVERSION} ${LIB_INSTALL_DIR}${LIB_POSTFIX}/libFcLinux.so)\n
")

install( FILES ${CMAKE_BINARY_DIR}/libFcLinux.so.${LIBFISHCAMP_VERSION} DESTINATION ${LIB_INSTALL_DIR}${LIB_POSTFIX})
install( FILES gdr_usb.hex Guider_mono_rev16_intel.srec DESTINATION ${FIRMWARE_INSTALL_DIR})
install( SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/make_libfishcamp_symlink.cmake)
