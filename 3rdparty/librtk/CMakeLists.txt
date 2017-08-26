cmake_minimum_required(VERSION 2.8)
PROJECT(librtk C CXX)

set(RTK_VERSION "1.0.0")
set(RTK_SOVERSION "1")

set(LIB_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/lib")

IF(UNIX AND NOT WIN32 AND NOT APPLE)
 IF (CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "armv6l")
    exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/librtk.so.armv6 ${CMAKE_BINARY_DIR}/librtk.so.${RTK_VERSION})
 ELSEIF (CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "armv7l")
    exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/librtk.so.armv7 ${CMAKE_BINARY_DIR}/librtk.so.${RTK_VERSION})
 ELSEIF (CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "aarch64")
    exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/librtk.so.armv8 ${CMAKE_BINARY_DIR}/librtk.so.${RTK_VERSION})
 ELSEIF(CMAKE_SIZEOF_VOID_P MATCHES "8")
  exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/librtk.so.x64 ${CMAKE_BINARY_DIR}/librtk.so.${RTK_VERSION})
 ELSE()
  exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/librtk.so.x86 ${CMAKE_BINARY_DIR}/librtk.so.${RTK_VERSION})
 ENDIF (CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "armv6l")
ENDIF(UNIX AND NOT WIN32 AND NOT APPLE)

# Make sure symbolic links are installed
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/make_rtk_symlink.cmake "
exec_program(${CMAKE_COMMAND} ARGS -E create_symlink ${LIB_INSTALL_DIR}${LIB_POSTFIX}/librtk.so.${RTK_VERSION} ${LIB_INSTALL_DIR}${LIB_POSTFIX}/librtk.so.${RTK_SOVERSION})\n
exec_program(${CMAKE_COMMAND} ARGS -E create_symlink ${LIB_INSTALL_DIR}${LIB_POSTFIX}/librtk.so.${RTK_SOVERSION} ${LIB_INSTALL_DIR}${LIB_POSTFIX}/librtk.so)\n
")

install( FILES ${CMAKE_BINARY_DIR}/librtk.so.${RTK_VERSION} DESTINATION ${LIB_INSTALL_DIR}${LIB_POSTFIX})
install( FILES librtk.h DESTINATION include/librtk)
install( SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/make_rtk_symlink.cmake)
