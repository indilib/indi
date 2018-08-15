PROJECT(libdspau C CXX)
cmake_minimum_required(VERSION 2.4.7)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake_modules/")
include(GNUInstallDirs)

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-error -std=c99")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error")

FIND_PACKAGE(FFTW3 REQUIRED)

SET(PACKAGE_VERSION "1.0.0")

ADD_DEFINITIONS(-Wall -O2)

include_directories( ${CMAKE_CURRENT_SOURCE_DIR})

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

set(dspau_LIB_SRCS
   libdspau-fft.c
   libdspau-filters.c
   libdspau-correlations.c
   libdspau-stats.c
   libdspau-buffer.c
   libdspau-convert.c
)

#build a shared library
ADD_LIBRARY(dspau SHARED ${dspau_LIB_SRCS})

set_target_properties(dspau PROPERTIES VERSION 1.0.0 SOVERSION 1)

#need to link to some other libraries ? just add them here
TARGET_LINK_LIBRARIES(dspau ${FFTW3_LIBRARIES} ${M_LIB})

#add an install target here
INSTALL(FILES libdspau.h DESTINATION include)

INSTALL(TARGETS dspau LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
