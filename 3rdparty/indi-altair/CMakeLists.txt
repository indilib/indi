cmake_minimum_required(VERSION 3.0)
PROJECT(indi_altair CXX C)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake_modules/")
include(GNUInstallDirs)

set(UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")

find_package(CFITSIO REQUIRED)
find_package(INDI REQUIRED)
find_package(ZLIB REQUIRED)
find_package(USB1 REQUIRED)
find_package(Threads REQUIRED)

# Using Altair SDK Version: ALTAIR DRIVERS20181103
set(ALTAIRCAM_VERSION_MAJOR 0)
set(ALTAIRCAM_VERSION_MINOR 1)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_altair.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_altair.xml)

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${CFITSIO_INCLUDE_DIR})

include(CMakeCommon)

if (APPLE)
    set(ALTAIRCAMLIB ${CMAKE_CURRENT_SOURCE_DIR}/mac/libaltaircam.bin)
    message("-- Using Mac static Altair library")
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "armv+")
    set(ALTAIRCAMLIB ${CMAKE_CURRENT_SOURCE_DIR}/armhf/libaltaircam.bin)
    message("-- Using armhf static Altair library")
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    set(ALTAIRCAMLIB ${CMAKE_CURRENT_SOURCE_DIR}/arm64/libaltaircam.bin)
    message("-- Using arm64 static Altair library")
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
    set(ALTAIRCAMLIB ${CMAKE_CURRENT_SOURCE_DIR}/x64/libaltaircam.bin)    
    message("-- Using 64 bit static Altair library")
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "i[3-6]86")
    set(ALTAIRCAMLIB ${CMAKE_CURRENT_SOURCE_DIR}/x86/libaltaircam.bin)    
    message("-- Using 32 bit static Altair library")
endif ()

########### indi_altair_ccd ###########
set(indi_altair_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/indi_altair.cpp
   )

add_executable(indi_altair_ccd ${indi_altair_SRCS})

target_link_libraries(indi_altair_ccd ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${ALTAIRCAMLIB} ${USB1_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})


#####################################

if (CMAKE_SYSTEM_PROCESSOR MATCHES "arm*")
target_link_libraries(indi_altair_ccd rt)
endif (CMAKE_SYSTEM_PROCESSOR MATCHES "arm*")

install(TARGETS indi_altair_ccd RUNTIME DESTINATION bin)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_altair.xml DESTINATION ${INDI_DATA_DIR})

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/99-altair.rules DESTINATION ${UDEVRULES_INSTALL_DIR})
endif()
