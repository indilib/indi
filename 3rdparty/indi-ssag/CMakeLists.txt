cmake_minimum_required(VERSION 2.8)
PROJECT(indi_ssag CXX C)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake_modules/")
include(GNUInstallDirs)

find_package(INDI REQUIRED)
find_package(USB1 REQUIRED)

set(UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")

set(INDI_SSAG_VERSION_MAJOR 1)
set(INDI_SSAG_VERSION_MINOR 0)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_ssag.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_ssag.xml )
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${CFITSIO_INCLUDE_DIR})

include(CMakeCommon)

set(indissag_SRCS
${CMAKE_CURRENT_SOURCE_DIR}/ssagccd.cpp
${CMAKE_CURRENT_SOURCE_DIR}/openssag_loader.cpp
${CMAKE_CURRENT_SOURCE_DIR}/openssag.cpp )

IF (UNITY_BUILD)
    ENABLE_UNITY_BUILD(indissag indissag_SRCS 10 cpp)
ENDIF ()

add_executable(indi_ssag_ccd ${indissag_SRCS})

target_link_libraries(indi_ssag_ccd ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${USB1_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})

if (CMAKE_SYSTEM_PROCESSOR MATCHES "arm*")
target_link_libraries(indi_ssag_ccd rt)
endif (CMAKE_SYSTEM_PROCESSOR MATCHES "arm*")

install(TARGETS indi_ssag_ccd RUNTIME DESTINATION bin )

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_ssag.xml DESTINATION ${INDI_DATA_DIR})
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
install(FILES 95-ssag.rules DESTINATION ${UDEVRULES_INSTALL_DIR})
endif()
