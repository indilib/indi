cmake_minimum_required(VERSION 2.8)
PROJECT(indi_asi CXX C)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake_modules/")
include(GNUInstallDirs)

find_package(CFITSIO REQUIRED)
find_package(INDI REQUIRED)
find_package(ZLIB REQUIRED)
find_package(USB1 REQUIRED)
find_package(Threads REQUIRED)

# Using ASI Camera SDK Version 1.14.0715 updated on 2019-07-22
# Using ASI EFW SDK Version 0.3.1205 updated on 2017-12-05
# Using ASI ST4 SDK Version 1.0 updated on 2018-07-23
# Using ASI EAF SDK Version 0.1.0524 updated on 2019-06-20
set(ASI_VERSION_MAJOR 1)
set(ASI_VERSION_MINOR 3)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_asi.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_asi.xml)

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${CFITSIO_INCLUDE_DIR})

include(CMakeCommon)

if (INDI_WEBSOCKET)
    find_package(websocketpp REQUIRED)
    find_package(Boost COMPONENTS system thread)
    include_directories (${Boost_INCLUDE_DIRS})
    add_definitions(-DHAVE_WEBSOCKET)
    SET(HAVE_WEBSOCKET 1)
endif()

if (APPLE)
    set(ASICAMLIB ${CMAKE_CURRENT_SOURCE_DIR}/mac/libASICamera2.bin)
    set(ASIFWLIB ${CMAKE_CURRENT_SOURCE_DIR}/mac/libEFWFilter.bin)
    set(ASIEAFLIB ${CMAKE_CURRENT_SOURCE_DIR}/mac/libEAFFocuser.bin)
    set(ASIST4LIB ${CMAKE_CURRENT_SOURCE_DIR}/mac/libUSB2ST4Conv.bin)
    message("-- Using Mac static ASI library")
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "armv+")
    set(ASICAMLIB ${CMAKE_CURRENT_SOURCE_DIR}/armv6/libASICamera2.bin)
    set(ASIFWLIB ${CMAKE_CURRENT_SOURCE_DIR}/armv6/libEFWFilter.bin)
    set(ASIEAFLIB ${CMAKE_CURRENT_SOURCE_DIR}/armv6/libEAFFocuser.bin)
    set(ASIST4LIB ${CMAKE_CURRENT_SOURCE_DIR}/armv6/libUSB2ST4Conv.bin)
    add_definitions(-DLOW_USB_BANDWIDTH)
    message("-- Using armv6 static ASI library")
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    set(ASICAMLIB ${CMAKE_CURRENT_SOURCE_DIR}/armv8/libASICamera2.bin)
    set(ASIFWLIB ${CMAKE_CURRENT_SOURCE_DIR}/armv8/libEFWFilter.bin)
    set(ASIEAFLIB ${CMAKE_CURRENT_SOURCE_DIR}/armv8/libEAFFocuser.bin)
    set(ASIST4LIB ${CMAKE_CURRENT_SOURCE_DIR}/armv8/libUSB2ST4Conv.bin)
    add_definitions(-DLOW_USB_BANDWIDTH)
    message("-- Using armv8 static ASI library")
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
    set(ASICAMLIB ${CMAKE_CURRENT_SOURCE_DIR}/x64/libASICamera2.bin)
    set(ASIFWLIB ${CMAKE_CURRENT_SOURCE_DIR}/x64/libEFWFilter.bin)
    set(ASIEAFLIB ${CMAKE_CURRENT_SOURCE_DIR}/x64/libEAFFocuser.bin)
    set(ASIST4LIB ${CMAKE_CURRENT_SOURCE_DIR}/x64/libUSB2ST4Conv.bin)
    message("-- Using 64 bit static ASI library")
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "i[3-6]86")
    set(ASICAMLIB ${CMAKE_CURRENT_SOURCE_DIR}/x86/libASICamera2.bin)
    set(ASIFWLIB ${CMAKE_CURRENT_SOURCE_DIR}/x86/libEFWFilter.bin)
    set(ASIEAFLIB ${CMAKE_CURRENT_SOURCE_DIR}/x86/libEAFFocuser.bin)
    set(ASIST4LIB ${CMAKE_CURRENT_SOURCE_DIR}/x86/libUSB2ST4Conv.bin)
    message("-- Using 32 bit static ASI library")
endif ()

########### indi_asi_ccd ###########
set(indi_asi_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/asi_ccd.cpp
   )

add_executable(indi_asi_ccd ${indi_asi_SRCS})
target_link_libraries(indi_asi_ccd ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${ASICAMLIB} ${USB1_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_THREAD_LIBS_INIT})
if (HAVE_WEBSOCKET)
    target_link_libraries(indi_asi_ccd ${Boost_LIBRARIES})
endif()



########### indi_asi_wheel ###########
add_executable(indi_asi_wheel ${CMAKE_CURRENT_SOURCE_DIR}/asi_wheel.cpp)
IF (APPLE)
set(CMAKE_EXE_LINKER_FLAGS "-framework IOKit -framework CoreFoundation")
target_link_libraries(indi_asi_wheel ${INDI_LIBRARIES} ${ASIFWLIB} ${LIBUSB_LIBRARIES})
ELSE()
target_link_libraries(indi_asi_wheel ${INDI_LIBRARIES} ${ASIFWLIB} ${USB1_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
ENDIF()

########### indi_asi_st4 ###########
add_executable(indi_asi_st4 ${CMAKE_CURRENT_SOURCE_DIR}/asi_st4.cpp)
IF (APPLE)
set(CMAKE_EXE_LINKER_FLAGS "-framework IOKit -framework CoreFoundation")
target_link_libraries(indi_asi_st4 ${INDI_LIBRARIES} ${ASIST4LIB} ${LIBUSB_LIBRARIES})
ELSE()
target_link_libraries(indi_asi_st4 ${INDI_LIBRARIES} ${ASIST4LIB} ${USB1_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
ENDIF()

########### indi_asi_focuser ###########
add_executable(indi_asi_focuser ${CMAKE_CURRENT_SOURCE_DIR}/asi_focuser.cpp)
IF (APPLE)
set(CMAKE_EXE_LINKER_FLAGS "-framework IOKit -framework CoreFoundation")
target_link_libraries(indi_asi_focuser ${INDI_LIBRARIES} ${ASIEAFLIB} ${LIBUSB_LIBRARIES})
ELSE()
target_link_libraries(indi_asi_focuser ${INDI_LIBRARIES} ${ASIEAFLIB} ${USB1_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
ENDIF()

#####################################

if (CMAKE_SYSTEM_PROCESSOR MATCHES "arm*")
target_link_libraries(indi_asi_ccd rt)
target_link_libraries(indi_asi_wheel rt)
target_link_libraries(indi_asi_st4 rt)
target_link_libraries(indi_asi_focuser rt)
endif (CMAKE_SYSTEM_PROCESSOR MATCHES "arm*")

install(TARGETS indi_asi_ccd RUNTIME DESTINATION bin)
install(TARGETS indi_asi_wheel RUNTIME DESTINATION bin)
install(TARGETS indi_asi_st4 RUNTIME DESTINATION bin)
install(TARGETS indi_asi_focuser RUNTIME DESTINATION bin)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_asi.xml DESTINATION ${INDI_DATA_DIR})
