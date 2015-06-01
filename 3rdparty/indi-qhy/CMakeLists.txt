cmake_minimum_required(VERSION 2.8)
PROJECT(indi_qhy CXX C)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
set(BIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/bin")
set(INDI_DATA_DIR "${CMAKE_INSTALL_PREFIX}/share/indi")

find_package(CFITSIO REQUIRED)
find_package(INDI REQUIRED)
find_package(QHY REQUIRED)
find_package(ZLIB REQUIRED)
find_package(USB-1 REQUIRED)

set(INDI_QHY_VERSION_MAJOR 1)
set(INDI_QHY_VERSION_MINOR 4)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${CFITSIO_INCLUDE_DIR})
include_directories( ${QHY_INCLUDE_DIR})

########### QHY CCD ###########

if(APPLE)
set(indiqhy_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/qhy_ccd.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/qhy_fw.cpp
   )
else()
set(indiqhy_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/qhy_ccd.cpp
   )
endif(APPLE)

add_executable(indi_qhy_ccd ${indiqhy_SRCS})

target_link_libraries(indi_qhy_ccd ${INDI_DRIVER_LIBRARIES} ${CFITSIO_LIBRARIES} ${QHY_LIBRARIES} ${LIBUSB_1_LIBRARIES})

if (CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "arm*")
target_link_libraries(indi_qhy_ccd rt)
endif (CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "arm*")

install(TARGETS indi_qhy_ccd RUNTIME DESTINATION bin )

install(FILES indi_qhy.xml DESTINATION ${INDI_DATA_DIR})

