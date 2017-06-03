cmake_minimum_required(VERSION 2.4.7)
PROJECT(indi-focuser-server CXX C)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake_modules/")
include(GNUInstallDirs)

find_package(INDI REQUIRED)

set (FS_VERSION_MAJOR 0)
set (FS_VERSION_MINOR 1)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_focuser-server.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_focuser-server.xml )

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})

set(indifs_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/focuserserver.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/focuserserverclient.cpp
   )

add_executable(indi_focuser_server ${indifs_SRCS})

target_link_libraries(indi_focuser_server ${INDI_LIBRARIES} )

install(TARGETS indi_focuser_server RUNTIME DESTINATION bin )

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_focuser-server.xml DESTINATION ${INDI_DATA_DIR})

