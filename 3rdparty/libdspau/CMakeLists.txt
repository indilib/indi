project(libdspau C)
cmake_minimum_required(VERSION 2.4.7)

set (DSPAU_VERSION_MAJOR 2)
set (DSPAU_VERSION_MINOR 0)
set (DSPAU_VERSION_REVISION 2)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake_modules/")
include(GNUInstallDirs)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-error -std=gnu99")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error")
set(DSPAU_VERSION_STRING "${DSPAU_VERSION_MAJOR}.${DSPAU_VERSION_MINOR}.${DSPAU_VERSION_REVISION}")

find_package(FFTW3 REQUIRED)
find_package(Threads REQUIRED)

add_definitions(-Wall -O2)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/include/libdspau.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/libdspau.h )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories( ${CMAKE_CURRENT_BINARY_DIR})

# This is for craft builds
include_directories("${CMAKE_INSTALL_PREFIX}/include")

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
   ${CMAKE_CURRENT_SOURCE_DIR}/src/libdspau-time.c
)

#build the shared library
add_library(dspau SHARED ${dspau_lib_srcs})

set_target_properties(dspau PROPERTIES VERSION ${DSPAU_VERSION_STRING} SOVERSION ${DSPAU_VERSION_MAJOR})

#need to link to some other libraries ? just add them here
target_link_libraries(dspau ${M_LIB} ${CMAKE_THREAD_LIBS_INIT} ${FFTW3_LIBRARIES})

#add an install target here
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libdspau.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS dspau LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

