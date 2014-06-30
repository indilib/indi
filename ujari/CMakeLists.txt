PROJECT(indi-ujari C CXX)
cmake_minimum_required(VERSION 2.4.7)

set(UJARI_VERSION_MAJOR 0)
set(UJARI_VERSION_MINOR 1)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
set(BIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/bin")

Include (CheckCXXSourceCompiles)

find_package(INDI REQUIRED)
find_package(Nova REQUIRED)
find_package(ZLIB REQUIRED)
find_package(MODBUS REQUIRED)
find_package(AIOUSB REQUIRED)
find_package(Threads REQUIRED)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )
set(INDI_DATA_DIR "${CMAKE_INSTALL_PREFIX}/share/indi")

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${NOVA_INCLUDE_DIR})
include_directories( ${MODBUS_INCLUDE_DIR})
include_directories( ${AIOUSB_INCLUDE_DIR})

########### Ujari ###############
set(ujari_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/ujari.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/ujarierror.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/forkmount.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/encoder.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/inverter.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/amccontroller.cpp
   )

add_executable(indi_ujari_telescope ${ujari_SRCS})

target_link_libraries(indi_ujari_telescope ${INDI_LIBRARIES} ${INDI_DRIVER_LIBRARIES} ${NOVA_LIBRARIES} ${AIOUSB_LIBRARIES} ${MODBUS_LIBRARIES})

install(TARGETS indi_ujari_telescope RUNTIME DESTINATION bin )

install( FILES  indi_ujari.xml indi_ujari_sk.xml DESTINATION ${INDI_DATA_DIR})
