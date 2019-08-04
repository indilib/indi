cmake_minimum_required(VERSION 3.0)
PROJECT(libtoupcam CXX C)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake_modules/")
include(GNUInstallDirs)

set(UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")
set(LIB_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")

# Using ToupCamera SDK Version: 39.15195.2019.0723
set(LIBTOUPCAM_VERSION "1.39.0")
set(LIBTOUPCAM_SOVERSION "1")

IF(APPLE)

set(FIRMWARE_INSTALL_DIR "/usr/local/lib/indi/DriverSupport/libtoupcam")

exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/mac/libtoupcam.bin ${CMAKE_BINARY_DIR}/libtoupcam.${LIBTOUPCAM_VERSION}.dylib)
install( FILES ${CMAKE_BINARY_DIR}/libtoupcam.${LIBTOUPCAM_VERSION}.dylib DESTINATION ${LIB_INSTALL_DIR}${LIB_POSTFIX})

# This is a temporary work-around until a better linking solution is found for MacOS
exec_program(ln ARGS -s ${LIB_INSTALL_DIR}/libtoupcam.${LIBTOUPCAM_VERSION}.dylib ${LIB_INSTALL_DIR}/libtoupcam.${LIBTOUPCAM_SOVERSION}.dylib)
exec_program(ln ARGS -s ${LIB_INSTALL_DIR}/libtoupcam.${LIBTOUPCAM_SOVERSION}.dylib ${LIB_INSTALL_DIR}/libtoupcam.dylib)

ELSE(APPLE)

set(UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")

IF(UNIX AND NOT WIN32)
  IF (CMAKE_SYSTEM_PROCESSOR MATCHES "armv+")
    exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/armhf/libtoupcam.bin ${CMAKE_BINARY_DIR}/libtoupcam.so.${LIBTOUPCAM_VERSION})
  ELSEIF (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/arm64/libtoupcam.bin ${CMAKE_BINARY_DIR}/libtoupcam.so.${LIBTOUPCAM_VERSION})
  ELSEIF(CMAKE_SIZEOF_VOID_P MATCHES "8")
    exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/x64/libtoupcam.bin ${CMAKE_BINARY_DIR}/libtoupcam.so.${LIBTOUPCAM_VERSION})
  ELSE()
    exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/x86/libtoupcam.bin ${CMAKE_BINARY_DIR}/libtoupcam.so.${LIBTOUPCAM_VERSION})
  ENDIF ()
ENDIF(UNIX AND NOT WIN32)

install( FILES ${CMAKE_BINARY_DIR}/libtoupcam.so.${LIBTOUPCAM_VERSION} DESTINATION ${LIB_INSTALL_DIR}${LIB_POSTFIX})
# Make sure symbolic links are installed
install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink \"libtoupcam.so.${LIBTOUPCAM_VERSION}\" \"libtoupcam.so.${LIBTOUPCAM_SOVERSION}\" WORKING_DIRECTORY
\"\$ENV{DESTDIR}/${BUILD_ROOT}${LIB_INSTALL_DIR}${LIB_POSTFIX}\" )" )
install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink \"libtoupcam.so.${LIBTOUPCAM_SOVERSION}\" \"libtoupcam.so\" WORKING_DIRECTORY
\"\$ENV{DESTDIR}/${BUILD_ROOT}${LIB_INSTALL_DIR}${LIB_POSTFIX}\" )" )

ENDIF(APPLE)

install( FILES toupcam.h DESTINATION include/libtoupcam)


if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/99-toupcam.rules DESTINATION ${UDEVRULES_INSTALL_DIR})
endif()
