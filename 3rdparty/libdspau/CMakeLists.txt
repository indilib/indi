project(libdspau C)
cmake_minimum_required(VERSION 2.4.7)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake_modules/")
include(GNUInstallDirs)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-error -std=c99")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error")
set(PACKAGE_VERSION "1.0.0")

find_package(TIFF REQUIRED)
find_package(FFTW3 REQUIRED)
find_package(Threads REQUIRED)

add_definitions(-Wall -O2 -std=gnu99)

include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/include)

set(dspau_lib_srcs
   ${CMAKE_CURRENT_SOURCE_DIR}/src/libdspau-align.c
   ${CMAKE_CURRENT_SOURCE_DIR}/src/libdspau-astro.c
   ${CMAKE_CURRENT_SOURCE_DIR}/src/libdspau-buffer.c
   ${CMAKE_CURRENT_SOURCE_DIR}/src/libdspau-convert.c
   ${CMAKE_CURRENT_SOURCE_DIR}/src/libdspau-interferometry.c
   ${CMAKE_CURRENT_SOURCE_DIR}/src/libdspau-fft.c
   ${CMAKE_CURRENT_SOURCE_DIR}/src/libdspau-filters.c
   ${CMAKE_CURRENT_SOURCE_DIR}/src/libdspau-stats.c
   ${CMAKE_CURRENT_SOURCE_DIR}/src/libdspau-stream.c
)

#build the shared library
add_library(dspau SHARED ${dspau_lib_srcs})

set_target_properties(dspau PROPERTIES VERSION 1.0.1 SOVERSION 1)

#need to link to some other libraries ? just add them here
target_link_libraries(dspau ${M_LIB} ${CMAKE_THREAD_LIBS_INIT} ${FFTW3_LIBRARIES})

#add an install target here
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/include/libdspau.h DESTINATION include)
install(TARGETS dspau LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
