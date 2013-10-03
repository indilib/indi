cmake_minimum_required(VERSION 2.4.7)
PROJECT(sxccd CXX C)

cmake_policy(SET CMP0003 OLD)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake_modules/")
set(RULES_INSTALL_DIR "/etc/udev/rules.d")

set (VERSION_MAJOR 1)
set (VERSION_MINOR 2)
 
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
target_link_libraries(indi_sx_wheel ${INDI_DRIVER_LIBRARIES} ${LIBUSB_1_LIBRARIES} pthread)
ENDIF()

set(indisxao_SRCS
   ${CMAKE_SOURCE_DIR}/sxao.cpp
   )

add_executable(indi_sx_ao ${indisxao_SRCS})
target_link_libraries(indi_sx_ao ${INDI_DRIVER_LIBRARIES})

set(test_SRCS
   ${CMAKE_SOURCE_DIR}/test.cpp
   ${CMAKE_SOURCE_DIR}/sxccdusb.cpp
   )

add_executable(test ${test_SRCS})
target_link_libraries(test ${LIBUSB_LIBRARIES})

install(TARGETS indi_sx_ccd RUNTIME DESTINATION bin)
install(TARGETS indi_sx_wheel RUNTIME DESTINATION bin)
install(TARGETS indi_sx_ao RUNTIME DESTINATION bin)
install(FILES indi_sx.xml DESTINATION ${INDI_DATA_DIR})

set (CPACK_PACKAGE_NAME "sxccd")
set (CPACK_PACKAGE_VENDOR "CloudMakers, s. r. o.")
set (CPACK_PACKAGE_VERSION "${VERSION_MAJOR}.${VERSION_MINOR}")
set (CPACK_PACKAGE_CONTACT "Peter Polakovic <peter.polakovic@cloudmakers.eu>")
set (CPACK_PACKAGE_DESCRIPTION_SUMMARY "INDI Driver for SX CCD, FW wheel and AO")
set (CPACK_PACKAGE_DESCRIPTION "This driver for INDI 0.9.6 supports SXVF-xxx, SXVR-xxx, LodeStar, CoStar and SuperStar cameras with full cooler and guider port control, USB Filter Wheel and Active Optics device.")

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  file (WRITE "README.txt" "${CPACK_PACKAGE_DESCRIPTION_SUMMARY}\nVersion ${CPACK_PACKAGE_VERSION}\n\n${CPACK_PACKAGE_DESCRIPTION}\n\nVisit http://www.cloudmakers.eu/indi to learn more.")
  set (CPACK_GENERATOR "PackageMaker")
  set (CPACK_RESOURCE_FILE_LICENSE "COPYRIGHT.txt")
  set (CPACK_RESOURCE_FILE_README "README.txt")
  set (CPACK_PACKAGING_INSTALL_PREFIX "/usr/local")
  set (CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}")
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  install(FILES 99-sx.rules DESTINATION ${RULES_INSTALL_DIR})
  set (CPACK_DEBIAN_PACKAGE_DESCRIPTION "${CPACK_PACKAGE_DESCRIPTION_SUMMARY}\n  ${CPACK_PACKAGE_DESCRIPTION}")
  set (CPACK_RPM_PACKAGE_DESCRIPTION ${CPACK_PACKAGE_DESCRIPTION})
  set (CPACK_GENERATOR "DEB;RPM")
  set (CPACK_DEBIAN_PACKAGE_DEPENDS "indi-bin (>= 0.9.6)")
  set (CPACK_DEBIAN_PACKAGE_SECTION "Science")
  set (CPACK_RPM_PACKAGE_DEPENDS "indi-bin >= 0.9.6")
  set (CPACK_RPM_PACKAGE_GROUP "Applications/Science")
  set (CPACK_PACKAGING_INSTALL_PREFIX "/usr")
  if (CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "arm.*")
    set (CPACK_GENERATOR "DEB")
    set (CPACK_DEBIAN_PACKAGE_ARCHITECTURE "arm")
    set (CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}")
  elseif (CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "x86_64")
    set (CPACK_GENERATOR "DEB;RPM")
    set (CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
    set (CPACK_RPM_PACKAGE_ARCHITECTURE "x86_64")
    set (CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}")
  elseif (CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "i[3-6]86")
    set (CPACK_GENERATOR "DEB;RPM")
    set (CPACK_DEBIAN_PACKAGE_ARCHITECTURE "i386")
    set (CPACK_RPM_PACKAGE_ARCHITECTURE "i386")
    set (CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}")
  endif ()
endif ()

include (CPack)
