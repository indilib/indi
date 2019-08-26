cmake_minimum_required(VERSION 3.0)
PROJECT(libstarshootg CXX C)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake_modules/")
include(GNUInstallDirs)

set(UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")
set(LIB_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")

# Using Starshoot G SDK Version: 38.14715.2019.0523
set(LIBSTARSHOOTG_VERSION "1.38.1")
set(LIBSTARSHOOTG_SOVERSION "1")

IF(APPLE)

set(FIRMWARE_INSTALL_DIR "/usr/local/lib/indi/DriverSupport/libstarshootg")

exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/mac/libstarshootg.bin ${CMAKE_BINARY_DIR}/libstarshootg.${LIBSTARSHOOTG_VERSION}.dylib)
install( FILES ${CMAKE_BINARY_DIR}/libstarshootg.${LIBSTARSHOOTG_VERSION}.dylib DESTINATION ${LIB_INSTALL_DIR}${LIB_POSTFIX})

# This is a temporary work-around until a better linking solution is found for MacOS
exec_program(ln ARGS -s ${LIB_INSTALL_DIR}/libstarshootg.${LIBSTARSHOOTG_VERSION}.dylib ${LIB_INSTALL_DIR}/libstarshootg.${LIBSTARSHOOTG_SOVERSION}.dylib)
exec_program(ln ARGS -s ${LIB_INSTALL_DIR}/libstarshootg.${LIBSTARSHOOTG_SOVERSION}.dylib ${LIB_INSTALL_DIR}/libstarshootg.dylib)

ELSE(APPLE)

set(UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")

IF(UNIX AND NOT WIN32)
  IF (CMAKE_SYSTEM_PROCESSOR MATCHES "armv+")
    exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/armhf/libstarshootg.bin ${CMAKE_BINARY_DIR}/libstarshootg.so.${LIBSTARSHOOTG_VERSION})
  ELSEIF (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/arm64/libstarshootg.bin ${CMAKE_BINARY_DIR}/libstarshootg.so.${LIBSTARSHOOTG_VERSION})
  ELSEIF(CMAKE_SIZEOF_VOID_P MATCHES "8")
    exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/x64/libstarshootg.bin ${CMAKE_BINARY_DIR}/libstarshootg.so.${LIBSTARSHOOTG_VERSION})
  ELSE()
    exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/x86/libstarshootg.bin ${CMAKE_BINARY_DIR}/libstarshootg.so.${LIBSTARSHOOTG_VERSION})
  ENDIF ()
ENDIF(UNIX AND NOT WIN32)

install( FILES ${CMAKE_BINARY_DIR}/libstarshootg.so.${LIBSTARSHOOTG_VERSION} DESTINATION ${LIB_INSTALL_DIR}${LIB_POSTFIX})
# Make sure symbolic links are installed
install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink \"libstarshootg.so.${LIBSTARSHOOTG_VERSION}\" \"libstarshootg.so.${LIBSTARSHOOTG_SOVERSION}\" WORKING_DIRECTORY
\"\$ENV{DESTDIR}/${BUILD_ROOT}${LIB_INSTALL_DIR}${LIB_POSTFIX}\" )" )
install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink \"libstarshootg.so.${LIBSTARSHOOTG_SOVERSION}\" \"libstarshootg.so\" WORKING_DIRECTORY
\"\$ENV{DESTDIR}/${BUILD_ROOT}${LIB_INSTALL_DIR}${LIB_POSTFIX}\" )" )

ENDIF(APPLE)

install( FILES starshootg.h DESTINATION include/libstarshootg)


if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/99-starshootg.rules DESTINATION ${UDEVRULES_INSTALL_DIR})
endif()
