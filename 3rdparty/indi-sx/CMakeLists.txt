cmake_minimum_required(VERSION 2.4.7)
PROJECT(sxccd CXX C)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake_modules/")
set(RULES_INSTALL_DIR "/etc/udev/rules.d")

set (VERSION_MAJOR 0)
set (VERSION_MINOR 1)
 
configure_file (
  "${CMAKE_SOURCE_DIR}/sxconfig.h.in"
  "${CMAKE_SOURCE_DIR}/sxconfig.h"
  )

find_package(USB REQUIRED)
find_package(INDI REQUIRED)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_SOURCE_DIR})
include_directories(${INDI_INCLUDE_DIR})

set(indisxccd_SRCS
   ${CMAKE_SOURCE_DIR}/sxccd.cpp
   ${CMAKE_SOURCE_DIR}/sxccdusb.cpp
   )

add_executable(indi_sx_ccd ${indisxccd_SRCS})
target_link_libraries(indi_sx_ccd ${INDI_DRIVER_LIBRARIES})

set(test_SRCS
   ${CMAKE_SOURCE_DIR}/test.cpp
   ${CMAKE_SOURCE_DIR}/sxccdusb.cpp
   )

add_executable(test ${CMAKE_SOURCE_DIR}/test.cpp ${CMAKE_SOURCE_DIR}/sxccdusb.cpp)
target_link_libraries(test ${LIBUSB_LIBRARIES})

install(TARGETS indi_sx_ccd RUNTIME DESTINATION bin)
install(FILES indi_sx.xml DESTINATION ${INDI_DATA_DIR})

IF(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  install(FILES 99-sx.rules DESTINATION ${RULES_INSTALL_DIR})
ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "Linux")