cmake_minimum_required(VERSION 2.4.7)
PROJECT(indi_tess CXX C)
Include (CheckCXXSourceCompiles)
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake_modules/")
set (VERSION_MAJOR 0)
set (VERSION_MINOR 1)
 
find_package(INDI REQUIRED)
find_package(Nova REQUIRED)

configure_file (
  "${CMAKE_SOURCE_DIR}/duconfig.h.in"
  "${CMAKE_SOURCE_DIR}/duconfig.h"
  )

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_SOURCE_DIR})
include_directories(${INDI_INCLUDE_DIR})
include_directories( ${NOVA_INCLUDE_DIR})


##################### indi arduino #####################
set(inditess_SRCS
   ${CMAKE_SOURCE_DIR}/tess.cpp
   ${CMAKE_SOURCE_DIR}/tess_algebra.cpp
   )

add_executable(indi_tess ${inditess_SRCS})
target_link_libraries(indi_tess ${INDI_DRIVER_LIBRARIES} ${NOVA_LIBRARIES})

install(TARGETS indi_tess RUNTIME DESTINATION bin)
install(FILES indi_tess.xml DESTINATION ${INDI_DATA_DIR})
install(FILES tess_sk.xml DESTINATION ${INDI_DATA_DIR})

