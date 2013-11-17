cmake_minimum_required(VERSION 2.4.7)
PROJECT(qhyccd CXX)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake_modules/")
set(FIRMWARE_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/lib/firmware")
set(UDEVRULES_INSTALL_DIR "/etc/udev/rules.d")

set (VERSION_MAJOR 0)
set (VERSION_MINOR 1)

configure_file (
  "${CMAKE_SOURCE_DIR}/qhyconfig.h.in"
  "${CMAKE_SOURCE_DIR}/qhyconfig.h"
  )

find_package(CFITSIO REQUIRED)
find_package(INDI REQUIRED)
find_package(ZLIB REQUIRED)
find_package(USB-1 REQUIRED)


include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_SOURCE_DIR})
include_directories(${USB_1_INCLUDE_DIR})
include_directories(${INDI_INCLUDE_DIR})
include_directories(${CFITSIO_INCLUDE_DIR})

set(indiqhy_SRCS
   ${CMAKE_SOURCE_DIR}/qhyccd.cpp
   ${CMAKE_SOURCE_DIR}/qhygeneric.cpp
   ${CMAKE_SOURCE_DIR}/qhyspecific.cpp
   )

set(qhytest_SRCS
   ${CMAKE_SOURCE_DIR}/qhytest.cpp
   ${CMAKE_SOURCE_DIR}/qhygeneric.cpp
   ${CMAKE_SOURCE_DIR}/qhyspecific.cpp
   )

add_executable(indi_qhy_ccd ${indiqhy_SRCS})
add_executable(qhy_test ${qhytest_SRCS})


target_link_libraries(indi_qhy_ccd ${INDI_DRIVER_LIBRARIES} ${CFITSIO_LIBRARIES} ${ZLIB_LIBRARIES} ${LIBUSB_1_LIBRARIES})
target_link_libraries(qhy_test ${LIBUSB_1_LIBRARIES})

install(TARGETS indi_qhy_ccd RUNTIME DESTINATION bin )

install(FILES indi_qhy.xml DESTINATION ${INDI_DATA_DIR} )

file(GLOB FIRMWARE "firmware/*.HEX")
install(FILES ${FIRMWARE} DESTINATION ${FIRMWARE_INSTALL_DIR})

set (CPACK_PACKAGE_NAME "qhyccd")
set (CPACK_PACKAGE_VENDOR "CloudMakers, s. r. o.")
set (CPACK_PACKAGE_VERSION "${VERSION_MAJOR}.${VERSION_MINOR}")
set (CPACK_PACKAGE_CONTACT "Peter Polakovic <peter.polakovic@cloudmakers.eu>")
set (CPACK_PACKAGE_DESCRIPTION_SUMMARY "INDI Driver for SX CCD.")
set (CPACK_PACKAGE_DESCRIPTION "This driver for INDI 0.9.7 supports QHY CCD cameras with full cooler and guider port control.")

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  file (WRITE "README.txt" "${CPACK_PACKAGE_DESCRIPTION_SUMMARY}\nVersion ${CPACK_PACKAGE_VERSION}\n\n${CPACK_PACKAGE_DESCRIPTION}\n\nVisit http://www.cloudmakers.eu/indi to learn more.")
  set (CPACK_GENERATOR "PackageMaker")
  set (CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/COPYRIGHT.txt")
  set (CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.txt")
  set (CPACK_PACKAGING_INSTALL_PREFIX "/usr/local")
  set (CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}")
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	install(FILES 85-qhyccd.rules DESTINATION ${UDEVRULES_INSTALL_DIR})
  set (CPACK_DEBIAN_PACKAGE_DESCRIPTION "${CPACK_PACKAGE_DESCRIPTION_SUMMARY}\n  ${CPACK_PACKAGE_DESCRIPTION}")
  set (CPACK_RPM_PACKAGE_DESCRIPTION ${CPACK_PACKAGE_DESCRIPTION})
  set (CPACK_GENERATOR "DEB;RPM")
  set (CPACK_DEBIAN_PACKAGE_DEPENDS "indi-bin (>= 0.9.7)")
  set (CPACK_DEBIAN_PACKAGE_SECTION "Science")
  set (CPACK_RPM_PACKAGE_DEPENDS "indi-bin >= 0.9.7")
  set (CPACK_RPM_PACKAGE_GROUP "Applications/Science")
  set (CPACK_PACKAGING_INSTALL_PREFIX "/usr")
  if (CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "x86_64")
    set (CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
    set (CPACK_RPM_PACKAGE_ARCHITECTURE "x86_64")
    set (CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-amd64")
  else (CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "x86_64")
    set (CPACK_DEBIAN_PACKAGE_ARCHITECTURE "i386")
    set (CPACK_RPM_PACKAGE_ARCHITECTURE "i386")
    set (CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-i386")
  endif ()
endif ()

include (CPack)