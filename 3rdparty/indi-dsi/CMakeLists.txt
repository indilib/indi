cmake_minimum_required(VERSION 2.4.7)
PROJECT(indi-dsi CXX C)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
set(BIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/bin")
set(RULES_INSTALL_DIR "/etc/udev/rules.d")
set(FIRMWARE_INSTALL_DIR "/lib/firmware")

find_package(CFITSIO REQUIRED)
find_package(INDI REQUIRED)
find_package(ZLIB REQUIRED)
find_package(USB-1 REQUIRED)

#configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${CFITSIO_INCLUDE_DIR})

########### DSI ###########
set(indidsi_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/dsi_ccd.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/DsiDevice.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/DsiDeviceFactory.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/DsiPro.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/DsiColor.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/DsiProII.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/DsiColorII.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/DsiTypes.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/Util.cpp
   )

add_executable(indi_dsi_ccd ${indidsi_SRCS})

target_link_libraries(indi_dsi_ccd ${INDI_DRIVER_LIBRARIES} ${CFITSIO_LIBRARIES} ${LIBUSB_1_LIBRARIES} )

install(TARGETS indi_dsi_ccd RUNTIME DESTINATION bin )

install(FILES indi_dsi.xml DESTINATION ${INDI_DATA_DIR})
install(FILES 99-meadedsi.rules DESTINATION ${RULES_INSTALL_DIR})
install(FILES meade-deepskyimager.hex  DESTINATION ${FIRMWARE_INSTALL_DIR})
