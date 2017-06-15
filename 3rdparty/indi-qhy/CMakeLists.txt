cmake_minimum_required(VERSION 2.8)
PROJECT(indi_qhy CXX C)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake_modules/")
include(GNUInstallDirs)

find_package(CFITSIO REQUIRED)
find_package(INDI REQUIRED)
find_package(QHY REQUIRED)
find_package(ZLIB REQUIRED)
find_package(USB-1 REQUIRED)
find_package(Threads REQUIRED)

set(INDI_QHY_VERSION_MAJOR 1)
set(INDI_QHY_VERSION_MINOR 6)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_qhy.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_qhy.xml )

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${CFITSIO_INCLUDE_DIR})
include_directories( ${QHY_INCLUDE_DIR})

IF ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++11")
ELSE ()
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
ENDIF ()

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

target_link_libraries(indi_qhy_ccd ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${QHY_LIBRARIES} ${LIBUSB_1_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})

if (CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "arm*")
target_link_libraries(indi_qhy_ccd rt)
endif (CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "arm*")

install(TARGETS indi_qhy_ccd RUNTIME DESTINATION bin )

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_qhy.xml DESTINATION ${INDI_DATA_DIR})

