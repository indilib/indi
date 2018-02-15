cmake_minimum_required(VERSION 3.0)
PROJECT(libfli CXX C)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake_modules/")
include(GNUInstallDirs)

set(UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")

ADD_DEFINITIONS(-Wall -O2 -D__LIBUSB__)

find_package(USB1 REQUIRED)

#SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-error")
#SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error")

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR})
INCLUDE_DIRECTORIES(unix)

set(fli_LIB_SRCS
   libfli.c   
   libfli-camera.c   
   libfli-camera-parport.c   
   libfli-camera-usb.c   
   libfli-filter-focuser.c   
   libfli-mem.c
   libfli-raw.c
      
   unix/libfli-usb.c
   unix/libfli-debug.c
   unix/libfli-serial.c
   unix/libfli-sys.c
   
   unix/linux/libfli-parport.c
   #unix/linux/libfli-usb-sys.c
   
   # LIBUSB support
   unix/libusb/libfli-usb-sys.c
)

#build a shared library
ADD_LIBRARY(fli SHARED ${fli_LIB_SRCS})

set_target_properties(fli PROPERTIES VERSION 2.0 SOVERSION 2)

#need to link to some other libraries ? just add them here
TARGET_LINK_LIBRARIES(fli ${USB1_LIBRARIES} -lm)

#add an install target here
INSTALL(FILES libfli.h DESTINATION include)

INSTALL(TARGETS fli LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/99-fli.rules DESTINATION ${UDEVRULES_INSTALL_DIR})
endif()

