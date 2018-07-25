PROJECT(indi_webcam C CXX)
cmake_minimum_required(VERSION 2.4.7)

set (WEBCAM_VERSION_MAJOR 0)
set (WEBCAM_VERSION_MINOR 2)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake_modules/")
include(GNUInstallDirs)

set(BIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/bin")

set(CMAKE_CXX_FLAGS "-g -std=c++0x ${CMAKE_CXX_FLAGS}")
#Include (CheckCXXSourceCompiles)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations" )
SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wno-deprecated-declarations")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-deprecated-declarations" )
SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Wno-deprecated-declarations")

find_package(INDI REQUIRED)
find_package(CFITSIO REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)
find_package(FFmpeg REQUIRED)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_webcam.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_webcam.xml)
set(INDI_DATA_DIR "${CMAKE_INSTALL_PREFIX}/share/indi")

if (NOT CFITSIO_FOUND OR CFITSIO_VERSION_MAJOR LESS 3)
  message(FATAL_ERROR "CFITSIO version too old, Please install cfitsio 3.x and try again. http://heasarc.gsfc.nasa.gov/fitsio/fitsio.html")
endif (NOT CFITSIO_FOUND OR CFITSIO_VERSION_MAJOR LESS 3)

#macro_bool_to_01(CFITSIO_FOUND HAVE_CFITSIO_H)
#macro_log_feature(CFITSIO_FOUND "libcfitsio" "A library for reading and writing data files in FITS (Flexible Image Transport System) data format" "http://heasarc.gsfc.nasa.gov/fitsio/fitsio.html" FALSE "3.03" "Provides INDI with FITS I/O support.")

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${FFMPEG_INCLUDE_DIR})

if (CFITSIO_FOUND)
  include_directories(${CFITSIO_INCLUDE_DIR})
endif (CFITSIO_FOUND)

########### OpenCV ###############
set(webcam_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/indi_webcam.cpp )


add_executable(indi_webcam_ccd ${webcam_SRCS})

target_link_libraries(indi_webcam_ccd  ${INDI_LIBRARIES} ${INDI_DRIVER_LIBRARIES} ${FFMPEG_LIBRARIES} -lavdevice -lswscale ${ZLIB_LIBRARY} ${CMAKE_THREAD_LIBS_INIT})

install(TARGETS indi_webcam_ccd RUNTIME DESTINATION bin )

install( FILES  ${CMAKE_CURRENT_BINARY_DIR}/indi_webcam.xml DESTINATION ${INDI_DATA_DIR})

