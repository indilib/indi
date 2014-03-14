cmake_minimum_required(VERSION 2.4.7)
PROJECT(indi_asicam0 CXX C)

set(CMAKE_MODULE_PATH "${CMAKE_INDI_SOURCE_DIR}/cmake_modules/")
set(BIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/bin")

find_package(CFITSIO REQUIRED)
find_package(INDI REQUIRED)
find_package(ZLIB REQUIRED)
find_package(USB-1 REQUIRED)

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

set(indi_asicam0_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/indi_asicam.cpp
   )

add_executable(indi_asicam0 ${indi_asicam0_SRCS})

target_link_libraries(indi_asicam0 ${ASILIB} ${INDI_DRIVER_LIBRARIES} ${CFITSIO_LIBRARIES} ${LIBUSB_1_LIBRARIES} )

install(TARGETS indi_asicam0 RUNTIME DESTINATION bin )

install(FILES indi_asicam.xml DESTINATION ${INDI_DATA_DIR})
