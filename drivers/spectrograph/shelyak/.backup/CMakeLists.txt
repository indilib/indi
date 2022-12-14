cmake_minimum_required(VERSION 3.0)
project(indi_shelyak_usis CXX C)

set(CMAKE_CXX_FLAGS "-g -std=c++0x ${CMAKE_CXX_FLAGS}")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include(GNUInstallDirs)

find_package(INDI REQUIRED)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${INDI_INCLUDE_DIR})

set (SHELYAK_USIS_VERSION_MAJOR 1)
set (SHELYAK_USIS_VERSION_MINOR 0)
set (JSMN_PARENT_LINKS 1)
set (JSMN_STRICT 1)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_shelyak_usis.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_shelyak_usis.xml )

set( shelyak_usis_SRCS
  ${CMAKE_CURRENT_SOURCE_DIR}/indi_shelyak_usis.cpp ${CMAKE_CURRENT_SOURCE_DIR}/jsmn.c
)

add_executable(indi_shelyak_usis ${shelyak_usis_SRCS} )
target_link_libraries(indi_shelyak_usis ${INDI_DRIVER_LIBRARIES})

install(TARGETS indi_shelyak_usis RUNTIME DESTINATION bin)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_shelyak_usis.xml DESTINATION ${INDI_DATA_DIR})


