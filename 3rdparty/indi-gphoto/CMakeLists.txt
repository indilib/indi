cmake_minimum_required(VERSION 2.4.7)
#PROJECT(indi_gphoto_ccd C)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake_modules/")

SET(CMAKE_CXX_FLAGS "-Wall" )
SET(CMAKE_CXX_FLAGS_DEBUG "-Werror" )
SET(CMAKE_C_FLAGS "-Wall" )
SET(CMAKE_C_FLAGS_DEBUG "-Werror" )

Include (CheckCXXSourceCompiles)
Include (CheckCSourceCompiles)
include (MacroOptionalFindPackage)
include (MacroLogFeature)
include (MacroBoolTo01)
include (CheckIncludeFiles)

find_package(USB REQUIRED)
find_package(CFITSIO REQUIRED)
find_package(INDI REQUIRED)
find_package(ZLIB REQUIRED)
find_package(GPHOTO2 REQUIRED)
find_package(Threads REQUIRED)
find_package(JPEG REQUIRED)
#configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${CFITSIO_INCLUDE_DIR})
include_directories( ${LIBGPHOTO2_INCLUDE_DIR})

########### Gphoto ###########
set(indigphoto_SRCS
   ${CMAKE_SOURCE_DIR}/gphoto_ccd.cpp
   ${CMAKE_SOURCE_DIR}/gphoto_driver.c
   ${CMAKE_SOURCE_DIR}/gphoto_readimage.c
   )

if(COMMAND cmake_policy)
  cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)

add_executable(indi_gphoto_ccd ${indigphoto_SRCS})

target_link_libraries(indi_gphoto_ccd ${INDI_LIBRARIES} ${INDI_MAIN_LIBRARIES} ${CFITSIO_LIBRARIES} ${LIBGPHOTO2_LIBRARIES})
target_link_libraries(indi_gphoto_ccd ${CMAKE_THREAD_LIBS_INIT} ${JPEG_LIBRARIES})
install(TARGETS indi_gphoto_ccd RUNTIME DESTINATION bin )

install(FILES indi_gphoto.xml DESTINATION ${INDI_DATA_DIR})
