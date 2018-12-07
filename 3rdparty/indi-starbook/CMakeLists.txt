cmake_minimum_required(VERSION 2.8)
PROJECT(indi_starbook CXX C)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake_modules/")

include(GNUInstallDirs)

set(STARBOOK_VERSION_MAJOR 0)
set(STARBOOK_VERSION_MINOR 1)

find_package(INDI REQUIRED)
find_package(CURL REQUIRED)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_starbook_telescope.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_starbook_telescope.xml )

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})

include(CMakeCommon)

############# STARBOOK ###############
set(indi_starbook_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/indi_starbook.cpp)

add_executable(indi_starbook_telescope ${indi_starbook_SRCS})

target_link_libraries(indi_starbook_telescope ${INDI_LIBRARIES})

install(TARGETS indi_starbook_telescope RUNTIME DESTINATION bin)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_starbook_telescope.xml DESTINATION ${INDI_DATA_DIR})
