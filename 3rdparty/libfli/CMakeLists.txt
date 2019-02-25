cmake_minimum_required(VERSION 3.0)
PROJECT(libfli CXX C)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake_modules/")
include(GNUInstallDirs)

set(UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")

ADD_DEFINITIONS(-Wall -O2 -D__LIBUSB__)

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-error")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error")

find_package(USB1 REQUIRED)

#SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-error")
#SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error")

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR})
INCLUDE_DIRECTORIES(unix)

## Some files like libnova.h and libusb.h are in in subdirectories of the include directory
## For the CMAKE Modules, they find the subdirectory, so then something like ln_types.h should be #include ln_types.h
## But some packages and drivers write their header files like this: #include libnova/ln_types.h
## On Linux, this is fine since the top include directory such as /usr/include is already included and therefore
## <libnova/ln_types.h> is resolved. But on Mac it its not already in the path and has to be explicitly added.

if (APPLE)
    ##This one is needed for homebrew
include_directories( "/usr/local/include")
    ## This one is needed for Craft
include_directories("${CMAKE_INSTALL_PREFIX}/include")
endif(APPLE)

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
   
   #unix/linux/libfli-usb-sys.c
   
   # LIBUSB support
   unix/libusb/libfli-usb-sys.c
)

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
list(APPEND fli_LIB_SRCS unix/linux/libfli-parport.c)
endif()

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

