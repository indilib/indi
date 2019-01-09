cmake_minimum_required(VERSION 3.0)
PROJECT(indi-avalon C CXX)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake_modules/")
include(GNUInstallDirs)

find_package(INDI COMPONENTS driver lx200 REQUIRED)
find_package(Nova REQUIRED)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${INDI_INCLUDE_DIR})
include_directories(${NOVA_INCLUDE_DIR})

include(CMakeCommon)

set(AVALON_VERSION_MAJOR 1)
set(AVALON_VERSION_MINOR 0)

set(INDI_DATA_DIR "${CMAKE_INSTALL_PREFIX}/share/indi")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_avalon.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_avalon.xml )

IF ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 6.3.9)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wformat-overflow=2")
ENDIF ()

########### LX200 StarGO ###########
SET(lx200stargo_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/lx200stargofocuser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/lx200stargo.cpp
    )

add_executable(indi_lx200stargo ${lx200stargo_SRCS})
target_link_libraries(indi_lx200stargo ${INDI_LIBRARIES} ${NOVA_LIBRARIES})

install(TARGETS indi_lx200stargo RUNTIME DESTINATION bin )

install( FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_avalon.xml DESTINATION ${INDI_DATA_DIR})
