cmake_minimum_required(VERSION 2.7)
PROJECT(indi_sbig CXX C)

set (SBIG_VERSION_MAJOR 2)
set (SBIG_VERSION_MINOR 1)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake_modules/")
set(BIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/bin")

find_package(CFITSIO REQUIRED)
find_package(INDI REQUIRED)
find_package(ZLIB REQUIRED)
find_package(SBIG REQUIRED)
find_package(Threads REQUIRED)
find_package(USB1 REQUIRED)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_sbig.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_sbig.xml )

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${SBIG_INCLUDE_DIR})
include_directories( ${CFITSIO_INCLUDE_DIR})

include(CMakeCommon)

############# SBIG CCD ###############
if (CFITSIO_FOUND)

set(sbigccd_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/sbig_ccd.cpp
)

if (APPLE)
    set(sbigccd_SRCS
        ${sbigccd_SRCS}
        ${CMAKE_CURRENT_SOURCE_DIR}/ezusb.c)
endif()

add_executable(indi_sbig_ccd ${sbigccd_SRCS})

target_link_libraries(indi_sbig_ccd ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${SBIG_LIBRARIES} ${M_LIB} ${ZLIB_LIBRARY} ${CMAKE_THREAD_LIBS_INIT} ${USB1_LIBRARIES})

install(TARGETS indi_sbig_ccd RUNTIME DESTINATION bin)

endif (CFITSIO_FOUND)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_sbig.xml DESTINATION ${INDI_DATA_DIR})
