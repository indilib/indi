cmake_minimum_required(VERSION 3.0)
PROJECT(indi_toupcam CXX C)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake_modules/")
include(GNUInstallDirs)

set(UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")

find_package(CFITSIO REQUIRED)
find_package(INDI REQUIRED)
find_package(ZLIB REQUIRED)
find_package(USB1 REQUIRED)
find_package(Threads REQUIRED)
find_package(TOUPCAM REQUIRED)

# Using ToupCamera SDK Version: 30.12802.2018.0829
set(TOUPCAM_VERSION_MAJOR 0)
set(TOUPCAM_VERSION_MINOR 1)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_toupcam.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_toupcam.xml)

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${CFITSIO_INCLUDE_DIR})
include_directories( ${TOUPCAM_INCLUDE_DIR})

include(CMakeCommon)

########### indi_toupcam_ccd ###########
set(indi_toupcam_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/indi_toupcam.cpp
   )

add_executable(indi_toupcam_ccd ${indi_toupcam_SRCS})

target_link_libraries(indi_toupcam_ccd ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${TOUPCAM_LIBRARIES} ${USB1_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})


#####################################

if (CMAKE_SYSTEM_PROCESSOR MATCHES "arm*")
target_link_libraries(indi_toupcam_ccd rt)
endif (CMAKE_SYSTEM_PROCESSOR MATCHES "arm*")

install(TARGETS indi_toupcam_ccd RUNTIME DESTINATION bin)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_toupcam.xml DESTINATION ${INDI_DATA_DIR})
