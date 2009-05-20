cmake_minimum_required(VERSION 2.4.7)

set(SBIG_VERSION "2.0.0")
set(SBIG_SOVERSION "2")

set(FIRMWARE_INSTALL_DIR "/lib/firmware")
set(RULES_INSTALL_DIR "/etc/udev/rules.d")
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
exec_program(cp ARGS ${CMAKE_SOURCE_DIR}/libsbigudrv64.so ${CMAKE_BINARY_DIR}/libsbigudrv.so.${SBIG_VERSION})
else (LIB_64)
exec_program(cp ARGS ${CMAKE_SOURCE_DIR}/libsbigudrv32.so ${CMAKE_BINARY_DIR}/libsbigudrv.so.${SBIG_VERSION})
endif(LIB_64)

# Make sure symbolic links are installed
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/make_sbig_symlink.cmake "
exec_program(${CMAKE_COMMAND} ARGS -E create_symlink ${LIB_INSTALL_DIR}${LIB_POSTFIX}/libsbigudrv.so.${SBIG_VERSION} ${LIB_INSTALL_DIR}${LIB_POSTFIX}/libsbigudrv.so.${SBIG_SOVERSION})\n
exec_program(${CMAKE_COMMAND} ARGS -E create_symlink ${LIB_INSTALL_DIR}${LIB_POSTFIX}/libsbigudrv.so.${SBIG_SOVERSION} ${LIB_INSTALL_DIR}${LIB_POSTFIX}/libsbigudrv.so)\n
")

install( FILES ${CMAKE_BINARY_DIR}/libsbigudrv.so.${SBIG_VERSION} DESTINATION ${LIB_INSTALL_DIR}${LIB_POSTFIX})
install( FILES sbigucam.hex sbiglcam.hex sbigfcam.hex stfga.bin DESTINATION ${FIRMWARE_INSTALL_DIR})
install( FILES 99-sbig.rules DESTINATION ${RULES_INSTALL_DIR})
install( SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/make_sbig_symlink.cmake)
