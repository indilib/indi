cmake_minimum_required(VERSION 3.0)
PROJECT(indi_toupbase CXX C)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake_modules/")
include(GNUInstallDirs)

set(UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")

find_package(CFITSIO REQUIRED)
find_package(INDI REQUIRED)
find_package(ZLIB REQUIRED)
find_package(USB1 REQUIRED)
find_package(Threads REQUIRED)
find_package(TOUPCAM REQUIRED)
find_package(ALTAIRCAM REQUIRED)
find_package(STARSHOOTG REQUIRED)
find_package(NNCAM REQUIRED)

set(TOUPBASE_VERSION_MAJOR 0)
set(TOUPBASE_VERSION_MINOR 2)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_toupbase.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_toupbase.xml)

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${CFITSIO_INCLUDE_DIR})
include_directories( ${TOUPCAM_INCLUDE_DIR})
include_directories( ${ALTAIRCAM_INCLUDE_DIR})
include_directories( ${STARSHOOTG_INCLUDE_DIR})
include_directories( ${NNCAM_INCLUDE_DIR})

include(CMakeCommon)

set(indi_toupbase_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/indi_toupbase.cpp)

########### indi_toupcam_ccd ###########
add_executable(indi_toupcam_ccd ${indi_toupbase_SRCS})
target_compile_definitions(indi_toupcam_ccd PRIVATE "-DBUILD_TOUPCAM")
target_link_libraries(indi_toupcam_ccd ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${TOUPCAM_LIBRARIES} ${USB1_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})

########### indi_altair_ccd ###########
add_executable(indi_altair_ccd ${indi_toupbase_SRCS})
target_compile_definitions(indi_altair_ccd PRIVATE "-DBUILD_ALTAIRCAM")
target_link_libraries(indi_altair_ccd ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${ALTAIRCAM_LIBRARIES} ${USB1_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})

########### indi_starshootg_ccd ###########
add_executable(indi_starshootg_ccd ${indi_toupbase_SRCS})
target_compile_definitions(indi_starshootg_ccd PRIVATE "-DBUILD_STARSHOOTG")
target_link_libraries(indi_starshootg_ccd ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${STARSHOOTG_LIBRARIES} ${USB1_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})

########### indi_nncam_ccd ###########
add_executable(indi_nncam_ccd ${indi_toupbase_SRCS})
target_compile_definitions(indi_nncam_ccd PRIVATE "-DBUILD_NNCAM")
target_link_libraries(indi_nncam_ccd ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${NNCAM_LIBRARIES} ${USB1_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})

#####################################

if (CMAKE_SYSTEM_PROCESSOR MATCHES "arm*")
target_link_libraries(indi_toupcam_ccd rt)
target_link_libraries(indi_altair_ccd rt)
target_link_libraries(indi_starshootg_ccd rt)
target_link_libraries(indi_nncam_ccd rt)
endif (CMAKE_SYSTEM_PROCESSOR MATCHES "arm*")

install(TARGETS
    indi_toupcam_ccd
    indi_altair_ccd
    indi_starshootg_ccd
    indi_nncam_ccd
    RUNTIME DESTINATION bin)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_toupbase.xml DESTINATION ${INDI_DATA_DIR})
