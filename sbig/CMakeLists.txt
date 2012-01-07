cmake_minimum_required(VERSION 2.4.7)

set(SBIG_VERSION "2.0.1")
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

##################################################
# if you are building in-source, this is the same as CMAKE_SOURCE_DIR, otherwise 
# this is the top level directory of your build tree 
MESSAGE( STATUS "CMAKE_BINARY_DIR:         " ${CMAKE_BINARY_DIR} )

# if you are building in-source, this is the same as CMAKE_CURRENT_SOURCE_DIR, otherwise this 
# is the directory where the compiled or generated files from the current CMakeLists.txt will go to 
MESSAGE( STATUS "CMAKE_CURRENT_BINARY_DIR: " ${CMAKE_CURRENT_BINARY_DIR} )

# this is the directory, from which cmake was started, i.e. the top level source directory 
MESSAGE( STATUS "CMAKE_SOURCE_DIR:         " ${CMAKE_SOURCE_DIR} )

# this is the directory where the currently processed CMakeLists.txt is located in 
MESSAGE( STATUS "CMAKE_CURRENT_SOURCE_DIR: " ${CMAKE_CURRENT_SOURCE_DIR} )

# contains the full path to the top level directory of your build tree 
MESSAGE( STATUS "PROJECT_BINARY_DIR: " ${PROJECT_BINARY_DIR} )

# contains the full path to the root of your project source directory,
# i.e. to the nearest directory where CMakeLists.txt contains the PROJECT() command 
MESSAGE( STATUS "PROJECT_SOURCE_DIR: " ${PROJECT_SOURCE_DIR} )

# set this variable to specify a common place where CMake should put all executable files
# (instead of CMAKE_CURRENT_BINARY_DIR)
MESSAGE( STATUS "EXECUTABLE_OUTPUT_PATH: " ${EXECUTABLE_OUTPUT_PATH} )

# set this variable to specify a common place where CMake should put all libraries 
# (instead of CMAKE_CURRENT_BINARY_DIR)
MESSAGE( STATUS "LIBRARY_OUTPUT_PATH:     " ${LIBRARY_OUTPUT_PATH} )

# tell CMake to search first in directories listed in CMAKE_MODULE_PATH
# when you use FIND_PACKAGE() or INCLUDE()
MESSAGE( STATUS "CMAKE_MODULE_PATH: " ${CMAKE_MODULE_PATH} )

# this is the complete path of the cmake which runs currently (e.g. /usr/local/bin/cmake) 
MESSAGE( STATUS "CMAKE_COMMAND: " ${CMAKE_COMMAND} )

# this is the CMake installation directory 
MESSAGE( STATUS "CMAKE_ROOT: " ${CMAKE_ROOT} )

# this is the filename including the complete path of the file where this variable is used. 
MESSAGE( STATUS "CMAKE_CURRENT_LIST_FILE: " ${CMAKE_CURRENT_LIST_FILE} )

# this is linenumber where the variable is used
MESSAGE( STATUS "CMAKE_CURRENT_LIST_LINE: " ${CMAKE_CURRENT_LIST_LINE} )

# this is used when searching for include files e.g. using the FIND_PATH() command.
MESSAGE( STATUS "CMAKE_INCLUDE_PATH: " ${CMAKE_INCLUDE_PATH} )

# this is used when searching for libraries e.g. using the FIND_LIBRARY() command.
MESSAGE( STATUS "CMAKE_LIBRARY_PATH: " ${CMAKE_LIBRARY_PATH} )

# the complete system name, e.g. "Linux-2.4.22", "FreeBSD-5.4-RELEASE" or "Windows 5.1" 
MESSAGE( STATUS "CMAKE_SYSTEM: " ${CMAKE_SYSTEM} )

# the short system name, e.g. "Linux", "FreeBSD" or "Windows"
MESSAGE( STATUS "CMAKE_SYSTEM_NAME: " ${CMAKE_SYSTEM_NAME} )

# only the version part of CMAKE_SYSTEM 
MESSAGE( STATUS "CMAKE_SYSTEM_VERSION: " ${CMAKE_SYSTEM_VERSION} )

# the processor name (e.g. "Intel(R) Pentium(R) M processor 2.00GHz") 
MESSAGE( STATUS "CMAKE_SYSTEM_PROCESSOR: " ${CMAKE_SYSTEM_PROCESSOR} )

###############################################################################


MESSAGE(STATUS "File in CMAKE_CURRENT_BINARY_DIR")
exec_program(ls ARGS ${CMAKE_CURRENT_BINARY_DIR})
MESSAGE(STATUS "File in CMAKE_BINARY_DIR")
exec_program(ls ARGS ${CMAKE_BINARY_DIR})
MESSAGE(STATUS "File in CMAKE_SOURCE_DIR")
exec_program(ls ARGS ${CMAKE_SOURCE_DIR})
MESSAGE(STATUS "File in PROJECT_SOURCE_DIR")
exec_program(ls ARGS ${PROJECT_SOURCE_DIR})


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
