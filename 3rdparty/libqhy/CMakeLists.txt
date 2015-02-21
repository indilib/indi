cmake_minimum_required(VERSION 2.8)
PROJECT(libqhy C CXX)

set(LIBQHY_VERSION "0.1.1")
set(LIBQHY_SOVERSION "0")

IF (APPLE)

install(FILES qhyccd.h qhyccdcamdef.h qhyccderr.h qhyccdstruct.h DESTINATION include/libqhy)
install(FILES libqhy.dylib DESTINATION lib)

ELSE()

set(FIRMWARE_INSTALL_DIR "/lib/firmware/qhy")
set(UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")
set(LIB_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/lib")

IF(UNIX AND NOT WIN32)
 IF (CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "armv6l")
  exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/libqhy_arm_v6.bin ${CMAKE_BINARY_DIR}/libqhy.so.${LIBQHY_VERSION})
 ELSEIF(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "armv7l")
  exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/libqhy_arm_v7.bin ${CMAKE_BINARY_DIR}/libqhy.so.${LIBQHY_VERSION})
 ELSEIF(CMAKE_SIZEOF_VOID_P MATCHES "8")
  exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/libqhy_64.bin ${CMAKE_BINARY_DIR}/libqhy.so.${LIBQHY_VERSION})
 ELSE()
  exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/libqhy_32.bin ${CMAKE_BINARY_DIR}/libqhy.so.${LIBQHY_VERSION})
 ENDIF (CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "armv6l")
ENDIF(UNIX AND NOT WIN32 AND NOT APPLE)

# Make sure symbolic links are installed
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/make_libqhy_symlink.cmake "
exec_program(${CMAKE_COMMAND} ARGS -E create_symlink ${LIB_INSTALL_DIR}${LIB_POSTFIX}/libqhy.so.${LIBQHY_VERSION} ${LIB_INSTALL_DIR}${LIB_POSTFIX}/libqhy.so.${LIBQHY_SOVERSION})\n
exec_program(${CMAKE_COMMAND} ARGS -E create_symlink ${LIB_INSTALL_DIR}${LIB_POSTFIX}/libqhy.so.${LIBQHY_SOVERSION} ${LIB_INSTALL_DIR}${LIB_POSTFIX}/libqhy.so)\n
")

install(FILES qhyccd.h qhyccdcamdef.h qhyccderr.h qhyccdstruct.h DESTINATION include/libqhy)
install(FILES ${CMAKE_BINARY_DIR}/libqhy.so.${LIBQHY_VERSION} DESTINATION ${LIB_INSTALL_DIR}${LIB_POSTFIX})
install(FILES 85-qhy.rules DESTINATION ${UDEVRULES_INSTALL_DIR})
install(
   CODE "
   file(GLOB QHY_FIRMWARE ${CMAKE_CURRENT_SOURCE_DIR}/firmware/*) \n
   file(INSTALL DESTINATION ${FIRMWARE_INSTALL_DIR} TYPE FILE FILES \${QHY_FIRMWARE})"
 )

install(SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/make_libqhy_symlink.cmake)

ENDIF (APPLE)

