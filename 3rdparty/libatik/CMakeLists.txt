cmake_minimum_required(VERSION 3.0)
PROJECT(libatik C CXX)

set(ATIK_VERSION "2.1.0")
set(ATIK_SOVERSION "2")

include(GNUInstallDirs)

set(LIB_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")

IF(APPLE)

set(FIRMWARE_INSTALL_DIR "/usr/local/lib/indi/DriverSupport/atik")

exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/mac/libatikcameras.bin ${CMAKE_BINARY_DIR}/libatikcameras${ATIK_VERSION}.dylib)
install( FILES ${CMAKE_BINARY_DIR}/libatikcameras${ATIK_VERSION}.dylib DESTINATION ${LIB_INSTALL_DIR}${LIB_POSTFIX})

exec_program(ln ARGS -s ${LIB_INSTALL_DIR}/libatikcameras${ATIK_VERSION}.dylib ${LIB_INSTALL_DIR}/libatikcameras${ATIK_SOVERSION}.dylib)
exec_program(ln ARGS -s ${LIB_INSTALL_DIR}/libatikcameras${ATIK_SOVERSION}.dylib ${LIB_INSTALL_DIR}/libatikcameras.dylib)
	
ELSE(APPLE)

set(UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")

IF(UNIX AND NOT WIN32)
  IF (CMAKE_SYSTEM_PROCESSOR MATCHES "armv+")
    exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/armhf/libatikcameras.bin ${CMAKE_BINARY_DIR}/libatikcameras.so.${ATIK_VERSION})
  ELSEIF (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/arm64/libatikcameras.bin ${CMAKE_BINARY_DIR}/libatikcameras.so.${ATIK_VERSION})
  ELSEIF(CMAKE_SIZEOF_VOID_P MATCHES "8")
    exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/x64/libatikcameras.bin ${CMAKE_BINARY_DIR}/libatikcameras.so.${ATIK_VERSION})
  ELSE()
    exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/x86/libatikcameras.bin ${CMAKE_BINARY_DIR}/libatikcameras.so.${ATIK_VERSION})
  ENDIF ()
ENDIF(UNIX AND NOT WIN32)

install( FILES ${CMAKE_BINARY_DIR}/libatikcameras.so.${ATIK_VERSION} DESTINATION ${LIB_INSTALL_DIR}${LIB_POSTFIX})
# Make sure symbolic links are installed
install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink \"libatikcameras.so.${ATIK_VERSION}\" \"libatikcameras.so.${ATIK_SOVERSION}\" WORKING_DIRECTORY 
\"\$ENV{DESTDIR}/${BUILD_ROOT}${LIB_INSTALL_DIR}${LIB_POSTFIX}\" )" )
install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink \"libatikcameras.so.${ATIK_SOVERSION}\" \"libatikcameras.so\" WORKING_DIRECTORY
\"\$ENV{DESTDIR}/${BUILD_ROOT}${LIB_INSTALL_DIR}${LIB_POSTFIX}\" )" )

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
install( FILES 99-atik.rules DESTINATION ${UDEVRULES_INSTALL_DIR})
endif()

ENDIF(APPLE)

install( FILES AtikCameras.h DESTINATION include/libatik)



