cmake_minimum_required(VERSION 2.4.7)
PROJECT(indi_asicam CXX C)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
set(BIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/bin")

cmake_policy(SET CMP0003 NEW)

find_package(CFITSIO REQUIRED)
find_package(INDI REQUIRED)
find_package(ZLIB REQUIRED)
find_package(USB-1 REQUIRED)
find_package(Threads REQUIRED)

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${CFITSIO_INCLUDE_DIR})

########### indi_asicam ###########

if (CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "arm.*")
set(ASILIB "${CMAKE_CURRENT_SOURCE_DIR}/SDK/armel/libASICamera.a")
elseif (CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "x86_64")
set(ASILIB "${CMAKE_CURRENT_SOURCE_DIR}/SDK/amd64/libASICamera.a")
elseif (CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "i[3-6]86")
set(ASILIB "${CMAKE_CURRENT_SOURCE_DIR}/SDK/i386/libASICamera.a")
endif ()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/SDK)
add_definitions(-D_LIN)

# Launchpad only works if we copy the static library to the binary dir
exec_program(cp ARGS ${ASILIB} ${CMAKE_BINARY_DIR})

set(indi_asicam_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/indi_asicam.cpp
   )

add_executable(indi_asicam ${indi_asicam_SRCS})

target_link_libraries(indi_asicam ${CMAKE_BINARY_DIR}/libASICamera.a ${INDI_DRIVER_LIBRARIES} ${CFITSIO_LIBRARIES} ${LIBUSB_1_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})

install(TARGETS indi_asicam RUNTIME DESTINATION bin )

install(FILES indi_asicam.xml DESTINATION ${INDI_DATA_DIR})
