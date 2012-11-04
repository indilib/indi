cmake_minimum_required(VERSION 2.4.7)
PROJECT(indi_sbig C CXX)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake_modules/")
set(BIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/bin")

find_package(CFITSIO REQUIRED)
find_package(INDI REQUIRED)
find_package(SBIG REQUIRED)
find_package(ZLIB REQUIRED)


configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${CFITSIO_INCLUDE_DIR})

########### SBIG ###########
set(indisbig_SRCS
   ${CMAKE_SOURCE_DIR}/sbigcam.cpp
   )
   
add_executable(indi_sbig_ccd ${indisbig_SRCS})

target_link_libraries(indi_sbig_ccd ${INDI_MAIN_LIBRARIES} ${CFITSIO_LIBRARIES} ${SBIG_LIBRARIES} z)

install(TARGETS indi_sbig_ccd RUNTIME DESTINATION bin )

install(FILES indi_sbig.xml DESTINATION ${INDI_DATA_DIR})
