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
find_package(USB-1 REQUIRED)
find_package(INDI REQUIRED)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_SOURCE_DIR})
include_directories(${INDI_INCLUDE_DIR})

set(indisxccd_SRCS
   ${CMAKE_SOURCE_DIR}/sxccd.cpp
   ${CMAKE_SOURCE_DIR}/sxccdusb.cpp
   )

add_executable(indi_sx_ccd ${indisxccd_SRCS})
target_link_libraries(indi_sx_ccd ${INDI_DRIVER_LIBRARIES} ${LIBUSB_LIBRARIES})

IF (APPLE)
set(indisxwheel_SRCS
  ${CMAKE_SOURCE_DIR}/sxwheel.cpp
  ${CMAKE_SOURCE_DIR}/hid_mac.c
  )
ELSEIF (WIN32)
set(indisxwheel_SRCS
  ${CMAKE_SOURCE_DIR}/sxwheel.cpp
  ${CMAKE_SOURCE_DIR}/hid_win.c
  )
ELSE ()
include_directories(${LIBUSB_1_INCLUDE_DIRS})
set(indisxwheel_SRCS
  ${CMAKE_SOURCE_DIR}/sxwheel.cpp
  ${CMAKE_SOURCE_DIR}/hid_libusb.c
  )
ENDIF()

add_executable(indi_sx_wheel ${indisxwheel_SRCS})
IF (APPLE)
set(CMAKE_EXE_LINKER_FLAGS "-framework IOKit -framework CoreFoundation")
target_link_libraries(indi_sx_wheel ${INDI_DRIVER_LIBRARIES} ${LIBUSB_LIBRARIES})
ELSEIF (WIN32)
ELSE ()
target_link_libraries(indi_sx_wheel ${INDI_DRIVER_LIBRARIES} ${LIBUSB_1_LIBRARIES})
ENDIF()

set(indisxao_SRCS
   ${CMAKE_SOURCE_DIR}/sxao.cpp
   )

add_executable(indi_sx_ao ${indisxao_SRCS})
target_link_libraries(indi_sx_ao ${INDI_DRIVER_LIBRARIES})


install(TARGETS indi_sx_ccd RUNTIME DESTINATION bin)
install(TARGETS indi_sx_wheel RUNTIME DESTINATION bin)
install(FILES indi_sx.xml DESTINATION ${INDI_DATA_DIR})

IF(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  install(FILES 99-sx.rules DESTINATION ${RULES_INSTALL_DIR})
ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
