cmake_minimum_required(VERSION 2.8)
PROJECT(indi-dsi CXX C)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake_modules/")
include(GNUInstallDirs)

IF(APPLE)
set(FIRMWARE_INSTALL_DIR "/usr/local/lib/firmware")
ELSE(APPLE)
set(RULES_INSTALL_DIR "/etc/udev/rules.d")
set(FIRMWARE_INSTALL_DIR "/lib/firmware")
ENDIF()
set (DSI_VERSION_MAJOR 0)
set (DSI_VERSION_MINOR 3)

find_package(CFITSIO REQUIRED)
find_package(INDI REQUIRED)
find_package(ZLIB REQUIRED)
find_package(USB1 REQUIRED)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_dsi.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_dsi.xml)

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${CFITSIO_INCLUDE_DIR})

include(CMakeCommon)

########### DSI ###########
set(indidsi_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/dsi_ccd.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/DsiDevice.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/DsiDeviceFactory.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/DsiPro.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/DsiColor.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/DsiProII.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/DsiProIII.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/DsiColorII.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/DsiColorIII.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/DsiTypes.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/Util.cpp   
   )

if (APPLE)
    set(indidsi_SRCS
        ${indidsi_SRCS}
        ${CMAKE_CURRENT_SOURCE_DIR}/download_fx2.cpp)
endif()

IF (UNITY_BUILD)
    ENABLE_UNITY_BUILD(indidsi indidsi_SRCS 10 cpp)
ENDIF ()

add_executable(indi_dsi_ccd ${indidsi_SRCS})

target_link_libraries(indi_dsi_ccd ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${USB1_LIBRARIES} )

install(TARGETS indi_dsi_ccd RUNTIME DESTINATION bin )

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_dsi.xml DESTINATION ${INDI_DATA_DIR})

IF (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
install(FILES 99-meadedsi.rules DESTINATION ${RULES_INSTALL_DIR})
ENDIF()

install(FILES meade-deepskyimager.hex DESTINATION ${FIRMWARE_INSTALL_DIR})
