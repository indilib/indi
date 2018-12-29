cmake_minimum_required(VERSION 2.7)
PROJECT(indi_dreamfocuser_focus C CXX)

set (DREAMFOCUSER_VERSION_MAJOR 1)
set (DREAMFOCUSER_VERSION_MINOR 0)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake_modules/")
set(BIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/bin")

find_package(INDI REQUIRED)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_dreamfocuser_focus.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_dreamfocuser_focus.xml)

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})

add_executable(indi_dreamfocuser_focus dreamfocuser.cpp)
target_link_libraries(indi_dreamfocuser_focus ${INDI_LIBRARIES})
install(TARGETS indi_dreamfocuser_focus RUNTIME DESTINATION bin )
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_dreamfocuser_focus.xml DESTINATION ${INDI_DATA_DIR})
