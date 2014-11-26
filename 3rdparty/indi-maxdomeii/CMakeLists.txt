cmake_minimum_required(VERSION 2.4.7)
PROJECT(indi_maxdomeii C CXX)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
set(CMAKE_CXX_FLAGS "-std=c++0x ${CMAKE_CXX_FLAGS}")

Include (CheckCXXSourceCompiles)
Include (CheckCSourceCompiles)
include (MacroOptionalFindPackage)
include (MacroLogFeature)
include (MacroBoolTo01)
include (CheckIncludeFiles)

find_package(INDI REQUIRED)
find_package(ZLIB REQUIRED)
FIND_PACKAGE(Nova REQUIRED)

#configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories(${NOVA_INCLUDE_DIR})

########### MaxDome II ###########
set(indimaxdomeii_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/maxdomeiidriver.c
   ${CMAKE_CURRENT_SOURCE_DIR}/maxdomeii.cpp
   )

add_executable(indi_maxdomeii ${indimaxdomeii_SRCS})

target_link_libraries(indi_maxdomeii ${INDI_DRIVER_LIBRARIES} ${NOVA_LIBRARIES})

install(TARGETS indi_maxdomeii RUNTIME DESTINATION bin )

install(FILES indi_maxdomeii.xml DESTINATION ${INDI_DATA_DIR})
