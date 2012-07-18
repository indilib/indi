cmake_minimum_required(VERSION 2.4.7)
PROJECT(indi_maxdomeii C CXX)

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

find_package(INDI REQUIRED)
find_package(ZLIB REQUIRED)

#configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})

########### MaxDome II ###########
set(indimaxdomeii_SRCS
   ${CMAKE_SOURCE_DIR}/maxdomeiidriver.c
   ${CMAKE_SOURCE_DIR}/maxdomeii.cpp
   )

if(COMMAND cmake_policy)
  cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)

add_executable(indi_maxdomeii ${indimaxdomeii_SRCS})

target_link_libraries(indi_maxdomeii ${INDI_LIBRARIES} ${INDI_DRIVER_LIBRARIES})

install(TARGETS indi_maxdomeii RUNTIME DESTINATION bin )

install(FILES indi_maxdomeii.xml DESTINATION ${INDI_DATA_DIR})
