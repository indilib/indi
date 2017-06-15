cmake_minimum_required(VERSION 2.4.7)
PROJECT(indi_qsi CXX C)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake_modules/")
include(GNUInstallDirs)
set(RULES_INSTALL_DIR "/lib/udev/rules.d/")

find_package(CFITSIO REQUIRED)
find_package(INDI REQUIRED)
find_package(QSI REQUIRED)
find_package(ZLIB REQUIRED)

set (QSI_VERSION_MAJOR 0)
set (QSI_VERSION_MINOR 5)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_qsi.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_qsi.xml )

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${CFITSIO_INCLUDE_DIR})
include_directories( ${QSI_INCLUDE_DIR})

IF ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++11")
ELSE ()
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
ENDIF ()

########### QSI ###########
set(indiqsi_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/qsi_ccd.cpp
   )

add_executable(indi_qsi_ccd ${indiqsi_SRCS})

target_link_libraries(indi_qsi_ccd ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${QSI_LIBRARIES} )

install(TARGETS indi_qsi_ccd RUNTIME DESTINATION bin )

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_qsi.xml DESTINATION ${INDI_DATA_DIR})

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
install(FILES 99-qsi.rules DESTINATION ${RULES_INSTALL_DIR})
endif()
