cmake_minimum_required(VERSION 2.4.7)
PROJECT(libdspau CXX C)

include(GNUInstallDirs)
ADD_DEFINITIONS(-Wall -O2)

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-error")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error")

set(dspau_LIB_SRCS
   libdspau-fft.c
   libdspau-filters.c
)

#build a shared library
ADD_LIBRARY(dspau SHARED ${dspau_LIB_SRCS})

set_target_properties(dspau PROPERTIES VERSION 1.0 SOVERSION 1)

#need to link to some other libraries ? just add them here
TARGET_LINK_LIBRARIES(dspau -lfftw3 -lm)

#add an install target here
INSTALL(FILES libdspau.h DESTINATION include)

INSTALL(TARGETS dspau LIBRARY DESTINATION lib)
