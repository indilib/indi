cmake_minimum_required(VERSION 3.0)
PROJECT(indi_atik CXX C)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake_modules/")
include(GNUInstallDirs)

find_package(CFITSIO REQUIRED)
find_package(INDI REQUIRED)
find_package(ZLIB REQUIRED)
find_package(USB1 REQUIRED)
find_package(Threads REQUIRED)
find_package(ATIK REQUIRED)
FIND_LIBRARY(M_LIB m)

set(ATIK_VERSION_MAJOR 2)
set(ATIK_VERSION_MINOR 4)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_atik.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_atik.xml)

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${CFITSIO_INCLUDE_DIR})
include_directories( ${ATIK_INCLUDE_DIR})

include(CMakeCommon)

########### indi_atik_ccd ###########
set(indi_atik_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/atik_ccd.cpp
   )

add_executable(indi_atik_ccd ${indi_atik_SRCS})

target_link_libraries(indi_atik_ccd ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${ATIK_LIBRARIES} ${USB1_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_THREAD_LIBS_INIT} ${M_LIB})


########## indi_atik_wheel ###########
add_executable(indi_atik_wheel ${CMAKE_CURRENT_SOURCE_DIR}/atik_wheel.cpp)
IF (APPLE)
set(CMAKE_EXE_LINKER_FLAGS "-framework IOKit -framework CoreFoundation")
target_link_libraries(indi_atik_wheel ${INDI_LIBRARIES} ${ATIK_LIBRARIES} ${LIBUSB_LIBRARIES})
ELSE()
target_link_libraries(indi_atik_wheel ${INDI_LIBRARIES} ${ATIK_LIBRARIES} ${USB1_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
ENDIF()

#####################################

if (CMAKE_SYSTEM_PROCESSOR MATCHES "arm*")
target_link_libraries(indi_atik_ccd rt)
target_link_libraries(indi_atik_wheel rt)
endif (CMAKE_SYSTEM_PROCESSOR MATCHES "arm*")

install(TARGETS indi_atik_ccd RUNTIME DESTINATION bin)
install(TARGETS indi_atik_wheel RUNTIME DESTINATION bin)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_atik.xml DESTINATION ${INDI_DATA_DIR})
