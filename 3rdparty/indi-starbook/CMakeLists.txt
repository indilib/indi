cmake_minimum_required(VERSION 2.8)
PROJECT(indi_starbook CXX C)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake_modules/")
include(GNUInstallDirs)

set(BIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/bin")

find_package(INDI REQUIRED)
find_package(CURL REQUIRED)


set(STARBOOK_VERSION_MAJOR 0)
set(STARBOOK_VERSION_MINOR 1)


set(indi_starbook_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/indi_starbook.cpp
        )

add_executable(indi_starbook ${indi_starbook_SRCS})