cmake_minimum_required(VERSION 2.4.7)
PROJECT(raspi_camera CXX C)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake_modules/")

include(GNUInstallDirs)

set (GENERIC_VERSION_MAJOR 1)
set (GENERIC_VERSION_MINOR 0)

find_package(CFITSIO REQUIRED)
find_package(INDI REQUIRED)
find_package(ZLIB REQUIRED)
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/raspicam/")
find_package(raspicam REQUIRED)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/raspi_camera.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/raspi_camera.xml )

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${CFITSIO_INCLUDE_DIR})

include(CMakeCommon)

############# GENERIC CCD ###############
set(genericccd_SRCS
	${CMAKE_CURRENT_SOURCE_DIR}/raspi_camera.cpp
)

add_executable(indi_raspi_camera ${genericccd_SRCS})

target_link_libraries(indi_raspi_camera ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} m ${ZLIB_LIBRARY})
target_link_libraries (indi_raspi_camera ${raspicam_LIBS})

install(TARGETS indi_raspi_camera RUNTIME DESTINATION bin)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/raspi_camera.xml DESTINATION ${INDI_DATA_DIR})
