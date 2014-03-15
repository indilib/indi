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
set(ASILIB "${CMAKE_CURRENT_SOURCE_DIR}/SDK/armel/libASICamera.bin")
elseif (CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "x86_64")
set(ASILIB "${CMAKE_CURRENT_SOURCE_DIR}/SDK/amd64/libASICamera.bin")
elseif (CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "i[3-6]86")
set(ASILIB "${CMAKE_CURRENT_SOURCE_DIR}/SDK/i386/libASICamera.bin")
endif ()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/SDK)
add_definitions(-D_LIN)

set(indi_asicam_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/indi_asicam.cpp
   )

add_executable(indi_asi_ccd ${indi_asicam_SRCS})

target_link_libraries(indi_asi_ccd ${ASILIB} ${INDI_DRIVER_LIBRARIES} ${CFITSIO_LIBRARIES} ${LIBUSB_1_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})

install(TARGETS indi_asi_ccd RUNTIME DESTINATION bin )

install(FILES indi_asiccd.xml DESTINATION ${INDI_DATA_DIR})
