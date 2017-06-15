
cmake_minimum_required(VERSION 2.4.7)
project(indi_shelyak CXX C)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake_modules/")
include(GNUInstallDirs)

find_package(INDI REQUIRED)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${INDI_INCLUDE_DIR})

set (SHELYAK_VERSION_MAJOR 1)
set (SHELYAK_VERSION_MINOR 0)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_shelyak.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_shelyak.xml )

IF ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++11")
ELSE ()
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
ENDIF ()

set(shelyak_SRCS
  ${CMAKE_CURRENT_SOURCE_DIR}/indi_shelyakeshel_spectrograph.cpp
)


add_executable(indi_shelyakeshel_spectrograph ${shelyak_SRCS})

target_link_libraries(indi_shelyakeshel_spectrograph ${INDI_LIBRARIES})

install(TARGETS indi_shelyakeshel_spectrograph RUNTIME DESTINATION bin)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_shelyak.xml DESTINATION ${INDI_DATA_DIR})
