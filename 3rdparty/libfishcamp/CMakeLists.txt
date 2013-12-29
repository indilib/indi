cmake_minimum_required(VERSION 2.4.7)
PROJECT(libfishcamp C CXX)

#***********************************************************
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake_modules/")
set(FIRMWARE_INSTALL_DIR "/lib/firmware")
set(LIB_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/lib")
set(UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")
#***********************************************************

find_package(USB-1 REQUIRED)

ADD_DEFINITIONS(-Wno-multichar)

set(LIBFISHCAMP_VERSION "1.0")
set(LIBFISHCAMP_SOVERSION "1")

set(fishcamp_LIB_SRCS fishcamp.c)

#build a shared library
ADD_LIBRARY(fishcamp SHARED ${fishcamp_LIB_SRCS})

set_target_properties(fishcamp PROPERTIES VERSION ${LIBFISHCAMP_VERSION} SOVERSION ${LIBFISHCAMP_SOVERSION})

target_link_libraries(fishcamp ${LIBUSB_1_LIBRARIES})

INSTALL(FILES fishcamp.h fishcamp_common.h DESTINATION include/fishcamp)

INSTALL(TARGETS fishcamp LIBRARY DESTINATION lib)

install( FILES gdr_usb.hex Guider_mono_rev16_intel.srec DESTINATION ${FIRMWARE_INSTALL_DIR})

install(FILES 99-fishcamp.rules DESTINATION ${UDEVRULES_INSTALL_DIR})

