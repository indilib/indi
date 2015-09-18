cmake_minimum_required(VERSION 2.8)
PROJECT(indi_miccd CXX C)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
set(BIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/bin")
set(UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")
set(LIB_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/lib")

set(CMAKE_CXX_FLAGS "-std=c++0x ${CMAKE_CXX_FLAGS}")

find_package(CFITSIO REQUIRED)
find_package(INDI REQUIRED)
find_package(ZLIB REQUIRED)
find_package(USB-1 REQUIRED)

set(INDI_MI_VERSION_MAJOR 0)
set(INDI_MI_VERSION_MINOR 1)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${CFITSIO_INCLUDE_DIR})

if (CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "armv5l")
    set(MILIB ${CMAKE_CURRENT_SOURCE_DIR}/libmiccdarmv5.bin)
elseif (CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "armv*")
    set(MILIB ${CMAKE_CURRENT_SOURCE_DIR}/libmiccdarmv6.bin)
elseif (CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "x86_64")
    set(MILIB ${CMAKE_CURRENT_SOURCE_DIR}/libmiccd64.bin)
elseif (CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "i[3-6]86")
    set(MILIB ${CMAKE_CURRENT_SOURCE_DIR}/libmiccd32.bin)
endif ()

########### MI CCD ###########
set(indi_miccd_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/mi_ccd.cpp
   )

add_executable(indi_mi_ccd ${indi_miccd_SRCS})

target_link_libraries(indi_mi_ccd ${INDI_DRIVER_LIBRARIES} ${CFITSIO_LIBRARIES} ${MILIB} ${LIBUSB_1_LIBRARIES} ${ZLIB_LIBRARY})

if (CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "arm*")
target_link_libraries(indi_mi_ccd rt)
endif (CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "arm*")

install(TARGETS indi_mi_ccd RUNTIME DESTINATION bin)
install(FILES indi_miccd.xml DESTINATION ${INDI_DATA_DIR})
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/99-miccd.rules DESTINATION ${UDEVRULES_INSTALL_DIR})
