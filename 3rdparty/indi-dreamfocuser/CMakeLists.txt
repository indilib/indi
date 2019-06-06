cmake_minimum_required(VERSION 2.7)
PROJECT(indi_dreamfocuser_focus C CXX)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake_modules/")
include(GNUInstallDirs)

set (DREAMFOCUSER_VERSION_MAJOR 2)
set (DREAMFOCUSER_VERSION_MINOR 1)

find_package(INDI REQUIRED)
find_package(Threads REQUIRED)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_dreamfocuser_focus.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_dreamfocuser_focus.xml)

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})

include(CMakeCommon)

########### DreamFocuser Focuser ###########
set(indidreamfocuser_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/dreamfocuser.cpp
   )

IF (UNITY_BUILD)
    ENABLE_UNITY_BUILD(indidreamfocuser indidreamfocuser_SRCS 10 cpp)
ENDIF ()

add_executable(indi_dreamfocuser_focus ${indidreamfocuser_SRCS})

target_link_libraries(indi_dreamfocuser_focus ${INDI_DRIVER_LIBRARIES} pthread)

install(TARGETS indi_dreamfocuser_focus RUNTIME DESTINATION bin )

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_dreamfocuser_focus.xml DESTINATION ${INDI_DATA_DIR})
