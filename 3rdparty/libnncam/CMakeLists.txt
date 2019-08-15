cmake_minimum_required(VERSION 3.0)
PROJECT(libnncam CXX C)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake_modules/")
include(GNUInstallDirs)

set(UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")
set(LIB_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")

# Using Version: 34.14088.2019.0307
set(LIBNNCAM_VERSION "1.34.1")
set(LIBNNCAM_SOVERSION "1")

IF(APPLE)

set(FIRMWARE_INSTALL_DIR "/usr/local/lib/indi/DriverSupport/libnncam")

exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/mac/libnncam.bin ${CMAKE_BINARY_DIR}/libnncam.${LIBNNCAM_VERSION}.dylib)
install( FILES ${CMAKE_BINARY_DIR}/libnncam.${LIBNNCAM_VERSION}.dylib DESTINATION ${LIB_INSTALL_DIR}${LIB_POSTFIX})

# This is a temporary work-around until a better linking solution is found for MacOS
exec_program(ln ARGS -s ${LIB_INSTALL_DIR}/libnncam.${LIBNNCAM_VERSION}.dylib ${LIB_INSTALL_DIR}/libnncam.${LIBNNCAM_SOVERSION}.dylib)
exec_program(ln ARGS -s ${LIB_INSTALL_DIR}/libnncam.${LIBNNCAM_SOVERSION}.dylib ${LIB_INSTALL_DIR}/libnncam.dylib)

ELSE(APPLE)

set(UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")

IF(UNIX AND NOT WIN32)
  IF (CMAKE_SYSTEM_PROCESSOR MATCHES "armv+")
    exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/armhf/libnncam.bin ${CMAKE_BINARY_DIR}/libnncam.so.${LIBNNCAM_VERSION})
  ELSEIF (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/arm64/libnncam.bin ${CMAKE_BINARY_DIR}/libnncam.so.${LIBNNCAM_VERSION})
  ELSEIF(CMAKE_SIZEOF_VOID_P MATCHES "8")
    exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/x64/libnncam.bin ${CMAKE_BINARY_DIR}/libnncam.so.${LIBNNCAM_VERSION})
  ELSE()
    exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/x86/libnncam.bin ${CMAKE_BINARY_DIR}/libnncam.so.${LIBNNCAM_VERSION})
  ENDIF ()
ENDIF(UNIX AND NOT WIN32)

install( FILES ${CMAKE_BINARY_DIR}/libnncam.so.${LIBNNCAM_VERSION} DESTINATION ${LIB_INSTALL_DIR}${LIB_POSTFIX})
# Make sure symbolic links are installed
install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink \"libnncam.so.${LIBNNCAM_VERSION}\" \"libnncam.so.${LIBNNCAM_SOVERSION}\" WORKING_DIRECTORY
\"\$ENV{DESTDIR}/${BUILD_ROOT}${LIB_INSTALL_DIR}${LIB_POSTFIX}\" )" )
install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink \"libnncam.so.${LIBNNCAM_SOVERSION}\" \"libnncam.so\" WORKING_DIRECTORY
\"\$ENV{DESTDIR}/${BUILD_ROOT}${LIB_INSTALL_DIR}${LIB_POSTFIX}\" )" )

ENDIF(APPLE)

install( FILES nncam.h DESTINATION include/libnncam)


if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/99-nncam.rules DESTINATION ${UDEVRULES_INSTALL_DIR})
endif()
