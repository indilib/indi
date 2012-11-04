cmake_minimum_required(VERSION 2.4.7)
PROJECT(generic_ccd CXX C)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake_modules/")
set(BIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/bin")

find_package(CFITSIO REQUIRED)
find_package(INDI REQUIRED)
find_package(ZLIB REQUIRED)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${CFITSIO_INCLUDE_DIR})

############# GENERIC CCD ###############
if (CFITSIO_FOUND)

set(genericccd_SRCS
	${CMAKE_SOURCE_DIR}/generic_ccd.cpp
)

add_executable(indi_generic_ccd ${genericccd_SRCS})

target_link_libraries(indi_generic_ccd ${INDI_DRIVER_LIBRARIES} ${CFITSIO_LIBRARIES} m z)

install(TARGETS indi_generic_ccd RUNTIME DESTINATION bin)

endif (CFITSIO_FOUND)

install(FILES indi_generic_ccd.xml DESTINATION ${INDI_DATA_DIR})
