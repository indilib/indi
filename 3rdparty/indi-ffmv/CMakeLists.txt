cmake_minimum_required(VERSION 2.4.7)
PROJECT(indi-ffmv CXX C)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
set(BIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/bin")
set(UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")

find_package(CFITSIO REQUIRED)
find_package(INDI REQUIRED)
find_package(ZLIB REQUIRED)
find_package(DC1394 REQUIRED)

#configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${CFITSIO_INCLUDE_DIR})
include_directories( ${DC1394_INCLUDE_DIR})

########### QSI ###########
set(indiffmv_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/ffmv_ccd.cpp
   )

add_executable(indi_ffmv_ccd ${indiffmv_SRCS})

target_link_libraries(indi_ffmv_ccd ${INDI_DRIVER_LIBRARIES} ${CFITSIO_LIBRARIES} ${DC1394_LIBRARIES} )

install(TARGETS indi_ffmv_ccd RUNTIME DESTINATION bin )

install(FILES indi_ffmv.xml DESTINATION ${INDI_DATA_DIR})
install(FILES 99-fireflymv.rules DESTINATION ${UDEVRULES_INSTALL_DIR})

