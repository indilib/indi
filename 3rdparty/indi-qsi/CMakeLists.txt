cmake_minimum_required(VERSION 2.4.7)
PROJECT(qsiccd CXX C)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake_modules/")
set(BIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/bin")
set(RULES_INSTALL_DIR "/etc/udev/rules.d")

find_package(CFITSIO REQUIRED)
find_package(INDI REQUIRED)
find_package(QSI REQUIRED)
find_package(ZLIB REQUIRED)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${CFITSIO_INCLUDE_DIR})
include_directories( ${QSI_INCLUDE_DIR})

########### QSI ###########
set(indiqsi_SRCS
   ${CMAKE_SOURCE_DIR}/qsi_ccd.cpp
   )

add_executable(indi_qsi_ccd ${indiqsi_SRCS})

target_link_libraries(indi_qsi_ccd ${INDI_DRIVER_LIBRARIES} ${CFITSIO_LIBRARIES} ${QSI_LIBRARIES} )

install(TARGETS indi_qsi_ccd RUNTIME DESTINATION bin )

install(FILES indi_qsi.xml DESTINATION ${INDI_DATA_DIR})
install(FILES 99-qsi.rules DESTINATION ${RULES_INSTALL_DIR})

