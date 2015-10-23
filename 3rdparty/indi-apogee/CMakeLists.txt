cmake_minimum_required(VERSION 2.4.7)
PROJECT(indi-apogee CXX C)

set(CMAKE_CXX_FLAGS "-std=c++0x ${CMAKE_CXX_FLAGS}")
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
set(BIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/bin")

find_package(CFITSIO REQUIRED)
find_package(INDI REQUIRED)
find_package(APOGEE REQUIRED)
find_package(ZLIB REQUIRED)

#configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${CFITSIO_INCLUDE_DIR})
include_directories( ${APOGEE_INCLUDE_DIR})

########### QSI ###########
set(indiapogee_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/apogee_ccd.cpp
   )

add_executable(indi_apogee_ccd ${indiapogee_SRCS})

target_link_libraries(indi_apogee_ccd ${INDI_DRIVER_LIBRARIES} ${CFITSIO_LIBRARIES} ${APOGEE_LIBRARY})

install(TARGETS indi_apogee_ccd RUNTIME DESTINATION bin )

install(FILES indi_apogee.xml DESTINATION ${INDI_DATA_DIR})
