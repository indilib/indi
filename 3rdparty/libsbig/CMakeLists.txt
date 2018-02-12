cmake_minimum_required(VERSION 3.0)
PROJECT(libsbig C CXX)

set(SBIG_VERSION "2.1.3")
set(SBIG_SOVERSION "2")

include(GNUInstallDirs)

set(FIRMWARE_INSTALL_DIR "/lib/firmware")
set(UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")
set(LIB_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-error")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error")

IF(UNIX AND NOT WIN32 AND NOT APPLE)
 IF (CMAKE_SYSTEM_PROCESSOR MATCHES "armv6l")
    exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/libsbig_arm_v6.bin ${CMAKE_BINARY_DIR}/libsbig.so.${SBIG_VERSION})
 ELSEIF (CMAKE_SYSTEM_PROCESSOR MATCHES "armv7l")
    exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/libsbig_arm_v7.bin ${CMAKE_BINARY_DIR}/libsbig.so.${SBIG_VERSION})
 ELSEIF (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/libsbig_arm_v8.bin ${CMAKE_BINARY_DIR}/libsbig.so.${SBIG_VERSION})
 ELSEIF(CMAKE_SIZEOF_VOID_P MATCHES "8")
  exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/libsbig64.bin ${CMAKE_BINARY_DIR}/libsbig.so.${SBIG_VERSION})
 ELSE()
  exec_program(cp ARGS ${CMAKE_CURRENT_SOURCE_DIR}/libsbig32.bin ${CMAKE_BINARY_DIR}/libsbig.so.${SBIG_VERSION})
 ENDIF (CMAKE_SYSTEM_PROCESSOR MATCHES "armv6l")
ENDIF(UNIX AND NOT WIN32 AND NOT APPLE)

install( FILES ${CMAKE_BINARY_DIR}/libsbig.so.${SBIG_VERSION} DESTINATION ${LIB_INSTALL_DIR}${LIB_POSTFIX})
install( FILES sbigudrv.h DESTINATION include/libsbig)
install( FILES sbigucam.hex sbiglcam.hex sbigfcam.hex sbigpcam.hex stfga.bin DESTINATION ${FIRMWARE_INSTALL_DIR})
install( FILES 51-sbig-debian.rules DESTINATION ${UDEVRULES_INSTALL_DIR})
# Make sure symbolic links are installed
install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink \"libsbig.so.${SBIG_VERSION}\" \"libsbig.so.${SBIG_SOVERSION}\" WORKING_DIRECTORY 
\"\$ENV{DESTDIR}/${BUILD_ROOT}${LIB_INSTALL_DIR}${LIB_POSTFIX}\" )" )
install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink \"libsbig.so.${SBIG_SOVERSION}\" \"libsbig.so\" WORKING_DIRECTORY 
\"\$ENV{DESTDIR}/${BUILD_ROOT}${LIB_INSTALL_DIR}${LIB_POSTFIX}\" )" )
