cmake_minimum_required(VERSION 2.8)
PROJECT(libasi CXX C)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
set(UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")
set(LIB_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/lib")

set(LIBASI_VERSION "0.1.0214")
set(LIBASI_SOVERSION "0")

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})

########### indi_asicam ###########

if (CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "armv5l")
set(ASILIB "${CMAKE_CURRENT_SOURCE_DIR}/armv5/libASICamera2.so.0.1.0214")
elseif (CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "armv6l")
set(ASILIB "${CMAKE_CURRENT_SOURCE_DIR}/armv6/libASICamera2.so.0.1.0214")
elseif (CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "armv7l")
set(ASILIB "${CMAKE_CURRENT_SOURCE_DIR}/armhf/libASICamera2.so.0.1.0214")
elseif (CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "x86_64")
set(ASILIB "${CMAKE_CURRENT_SOURCE_DIR}/x64/libASICamera2.so.0.1.0214")
elseif (CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "i[3-6]86")
set(ASILIB "${CMAKE_CURRENT_SOURCE_DIR}/x86/libASICamera2.so.0.1.0214")
endif ()

# Make sure symbolic links are installed
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/make_libasi_symlink.cmake "
exec_program(${CMAKE_COMMAND} ARGS -E create_symlink ${LIB_INSTALL_DIR}/libASICamera2.so.${LIBASI_VERSION} ${LIB_INSTALL_DIR}/libASICamera2.so.${LIBASI_SOVERSION})\n
exec_program(${CMAKE_COMMAND} ARGS -E create_symlink ${LIB_INSTALL_DIR}/libASICamera2.so.${LIBASI_SOVERSION} ${LIB_INSTALL_DIR}/libASICamera2.so)\n
")

install(FILES ASICamera2.h DESTINATION include/libasi)
install(FILES ${ASILIB} DESTINATION ${LIB_INSTALL_DIR})
install(SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/make_libasi_symlink.cmake)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/99-asi.rules DESTINATION ${UDEVRULES_INSTALL_DIR})
