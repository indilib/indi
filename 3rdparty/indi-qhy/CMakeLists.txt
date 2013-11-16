cmake_minimum_required(VERSION 2.4.7)
PROJECT(indi_qhy_ccd CXX)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
set(FIRMWARE_INSTALL_DIR "/lib/firmware")
set(UDEVRULES_INSTALL_DIR "/etc/udev/rules.d")
SET(CMAKE_C_FLAGS "-Wall" )
SET(CMAKE_C_FLAGS_DEBUG "-Werror" )

find_package(CFITSIO REQUIRED)
find_package(USB-1 REQUIRED)
find_package(INDI REQUIRED)
find_package(ZLIB REQUIRED)

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${CFITSIO_INCLUDE_DIR})

########### QHY ###########
set(indiqhy_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/qhy_ccd.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/qhy5_driver.cpp
   )

add_executable(indi_qhy_ccd ${indiqhy_SRCS})

target_link_libraries(indi_qhy_ccd ${INDI_DRIVER_LIBRARIES} ${CFITSIO_LIBRARIES} ${ZLIB_LIBRARIES} ${LIBUSB_1_LIBRARIES})

install(TARGETS indi_qhy_ccd RUNTIME DESTINATION bin )

install(FILES indi_qhy.xml DESTINATION ${INDI_DATA_DIR} )

install(FILES firmware/qhy5.hex DESTINATION ${FIRMWARE_INSTALL_DIR})
install(FILES firmware/qhy5loader.hex DESTINATION ${FIRMWARE_INSTALL_DIR})
install(FILES firmware/85-qhy.rules DESTINATION ${UDEVRULES_INSTALL_DIR})
