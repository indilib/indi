cmake_minimum_required(VERSION 3.0)
PROJECT(indi_altaircam CXX C)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake_modules/")
include(GNUInstallDirs)

find_package(CFITSIO REQUIRED)
find_package(INDI REQUIRED)
find_package(ZLIB REQUIRED)
find_package(USB1 REQUIRED)
find_package(Threads REQUIRED)
find_package(ALTAIRCAM REQUIRED)

# Using Altair Library
set(ALTAIRCAM_VERSION_MAJOR 0)
set(ALTAIRCAM_VERSION_MINOR 1)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_altaircam.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_altaircam.xml)

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${CFITSIO_INCLUDE_DIR})
include_directories( ${ALTAIRCAM_INCLUDE_DIR})

include(CMakeCommon)
########### indi_altair_ccd ###########
set(indi_altair_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/indi_altair.cpp
   )

add_executable(indi_altair_ccd ${indi_altair_SRCS})

target_link_libraries(indi_altair_ccd ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${ALTAIRCAM_LIBRARIES} ${USB1_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})


#####################################

if (CMAKE_SYSTEM_PROCESSOR MATCHES "arm*")
target_link_libraries(indi_altair_ccd rt)
endif (CMAKE_SYSTEM_PROCESSOR MATCHES "arm*")

install(TARGETS indi_altair_ccd RUNTIME DESTINATION bin)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_altaircam.xml DESTINATION ${INDI_DATA_DIR})
