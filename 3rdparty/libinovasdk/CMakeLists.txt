cmake_minimum_required(VERSION 3.0)
PROJECT(libinovasdk C CXX)

set(INOVASDK_VERSION "1.3.5")
set(INOVASDK_SOVERSION "1")

include(GNUInstallDirs)

set(FIRMWARE_INSTALL_DIR "/lib/firmware")
set(UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")
set(LIB_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-error")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error")

IF(UNIX AND NOT WIN32 AND NOT APPLE)
 IF (CMAKE_SYSTEM_PROCESSOR MATCHES "armv+")
    exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/libinovasdk.so.armv6 ${CMAKE_BINARY_DIR}/libinovasdk.so.${INOVASDK_VERSION})
 #ELSEIF (CMAKE_SYSTEM_PROCESSOR MATCHES "armv7l")
 #   exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/libinovasdk.so.armv7 ${CMAKE_BINARY_DIR}/libinovasdk.so.${INOVASDK_VERSION})
 ELSEIF (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/libinovasdk.so.armv8 ${CMAKE_BINARY_DIR}/libinovasdk.so.${INOVASDK_VERSION})
 ELSEIF(CMAKE_SIZEOF_VOID_P MATCHES "8")
  exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/libinovasdk.so.x64 ${CMAKE_BINARY_DIR}/libinovasdk.so.${INOVASDK_VERSION})
 ELSE(CMAKE_SIZEOF_VOID_P MATCHES "8")
  exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/libinovasdk.so.x86 ${CMAKE_BINARY_DIR}/libinovasdk.so.${INOVASDK_VERSION})
 ENDIF (CMAKE_SYSTEM_PROCESSOR MATCHES "armv6l")
ENDIF(UNIX AND NOT WIN32 AND NOT APPLE)

install( FILES ${CMAKE_BINARY_DIR}/libinovasdk.so.${INOVASDK_VERSION} DESTINATION ${LIB_INSTALL_DIR}${LIB_POSTFIX})
install( FILES inovasdk.h DESTINATION include/inovasdk)
install( FILES 99-inovaplx.rules DESTINATION ${UDEVRULES_INSTALL_DIR})
# Make sure symbolic links are installed
install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink \"libinovasdk.so.${INOVASDK_VERSION}\" \"libinovasdk.so.${INOVASDK_SOVERSION}\" 
WORKING_DIRECTORY \"\$ENV{DESTDIR}/${BUILD_ROOT}${LIB_INSTALL_DIR}${LIB_POSTFIX}\" )" )
install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink \"libinovasdk.so.${INOVASDK_SOVERSION}\" \"libinovasdk.so\" WORKING_DIRECTORY 
\"\$ENV{DESTDIR}/${BUILD_ROOT}${LIB_INSTALL_DIR}${LIB_POSTFIX}\" )" )
