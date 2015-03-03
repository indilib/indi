cmake_minimum_required(VERSION 2.8)
PROJECT(indi_asicam CXX C)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
set(BIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/bin")
set(UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")
set(LIB_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/lib")

set(LIBASI_VERSION "0.1.0214")
set(LIBASI_SOVERSION "0")

set(CMAKE_CXX_FLAGS "-std=c++0x ${CMAKE_CXX_FLAGS}")

find_package(CFITSIO REQUIRED)
find_package(INDI REQUIRED)
find_package(ASI REQUIRED)
find_package(ZLIB REQUIRED)
find_package(USB-1 REQUIRED)
find_package(Threads REQUIRED)

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${LIBASI_INCLUDE_DIR})
include_directories( ${CFITSIO_INCLUDE_DIR})

########### indi_asi_ccd ###########

set(indi_asicam_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/asi_ccd.cpp
   )

add_executable(indi_asi_ccd ${indi_asicam_SRCS})

target_link_libraries(indi_asi_ccd ${INDI_DRIVER_LIBRARIES} ${CFITSIO_LIBRARIES} ${LIBUSB_1_LIBRARIES} ${ZLIB_LIBRARY} ${ASI_LIBRARY} ${CMAKE_THREAD_LIBS_INIT})

install(TARGETS indi_asi_ccd RUNTIME DESTINATION bin)
install(FILES indi_asiccd.xml DESTINATION ${INDI_DATA_DIR})
