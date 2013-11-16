cmake_minimum_required(VERSION 2.4.7)
PROJECT(indi-fishcamp CXX C)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
set(BIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/bin")

find_package(CFITSIO REQUIRED)
find_package(INDI REQUIRED)
find_package(ZLIB REQUIRED)
find_package(FISHCAMP REQUIRED)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${CFITSIO_INCLUDE_DIR})

############# FISHCAMP STARFISH CCD ###############
set(fishcampccd_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/indi_fishcamp.cpp
)

add_executable(indi_fishcamp_ccd ${fishcampccd_SRCS})

target_link_libraries(indi_fishcamp_ccd ${FISHCAMP_LIBRARIES} ${INDI_DRIVER_LIBRARIES} ${CFITSIO_LIBRARIES} m ${ZLIB_LIBRARY})

install(TARGETS indi_fishcamp_ccd RUNTIME DESTINATION bin)

install(FILES indi_fishcamp.xml DESTINATION ${INDI_DATA_DIR})
