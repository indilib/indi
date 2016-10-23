cmake_minimum_required(VERSION 2.4.7)
PROJECT(indi_optec_wheel CXX C)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake_modules/")
include(GNUInstallDirs)

set(CMAKE_CXX_FLAGS "-std=c++0x ${CMAKE_CXX_FLAGS}")
set(BIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/bin")
set(RULES_INSTALL_DIR "/lib/udev/rules.d/")

find_package(INDI REQUIRED)

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})

########### indi optec wheel ###########
set(indioptecwheel_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/ifwoptec.cpp
   )

add_executable(indi_optec_wheel ${indioptecwheel_SRCS})

target_link_libraries(indi_optec_wheel ${INDI_DRIVER_LIBRARIES} )

install(TARGETS indi_optec_wheel RUNTIME DESTINATION bin )

