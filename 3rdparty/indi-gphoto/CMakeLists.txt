cmake_minimum_required(VERSION 2.4.7)
PROJECT(indi_gphoto C CXX)

set(INDI_GPHOTO_VERSION_MAJOR 1)
set(INDI_GPHOTO_VERSION_MINOR 5)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall" )
SET(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -DDEBUG_FITS" )
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall" )
SET(CMAKE_C_FLAGS_DEBUG "-O0 -g -DDEBUG_FITS" )

find_package(USB REQUIRED)
find_package(CFITSIO REQUIRED)
find_package(INDI REQUIRED)
find_package(ZLIB REQUIRED)
find_package(GPHOTO2 REQUIRED)
find_package(Threads REQUIRED)
find_package(JPEG REQUIRED)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )

find_program(DCRAW_EXECUTABLE NAMES dcraw
    PATHS
    /usr/bin
    /usr/local/bin)

if (NOT DCRAW_EXECUTABLE)
    message(FATAL_ERROR "dcraw not found. Please install dcraw and try again.")
endif (NOT DCRAW_EXECUTABLE)

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${CFITSIO_INCLUDE_DIR})
include_directories( ${GPHOTO2_INCLUDE_DIR})

#if (CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "arm*")
#add_definitions(-DUSE_THUMB_SIZE)
#endif (CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "arm*")

########### Gphoto ###########
set(indigphoto_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/gphoto_ccd.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/gphoto_driver.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/gphoto_readimage.cpp
   )

add_executable(indi_gphoto_ccd ${indigphoto_SRCS})

target_link_libraries(indi_gphoto_ccd ${INDI_DRIVER_LIBRARIES} ${CFITSIO_LIBRARIES} ${GPHOTO2_LIBRARY} ${GPHOTO2_PORT_LIBRARY} ${CMAKE_THREAD_LIBS_INIT} ${JPEG_LIBRARIES} ${ZLIB_LIBRARIES})
install(TARGETS indi_gphoto_ccd RUNTIME DESTINATION bin )

install(FILES indi_gphoto.xml DESTINATION ${INDI_DATA_DIR})
