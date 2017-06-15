cmake_minimum_required(VERSION 2.4.7)
PROJECT(indi_mgenautoguider C CXX)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake_modules/")
include(GNUInstallDirs)

set(INDI_MGENAUTOGUIDER_VERSION_MAJOR 0)
set(INDI_MGENAUTOGUIDER_VERSION_MINOR 1)

find_package(INDI REQUIRED)
find_package(ZLIB REQUIRED)
find_package(FTDI1 REQUIRED)
find_package(USB-1 REQUIRED)
find_package(Threads REQUIRED)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_mgenautoguider.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_mgenautoguider.xml )

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${INDI_INCLUDE_DIR})
include_directories(${FTDI1_INCLUDE_DIR})
include_directories(${LIBUSB_1_INCLUDE_DIRS})

IF ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++11")
ELSE ()
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
ENDIF ()

########### MGenAutoguider ###########
set(indimgenautoguider_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/mgenautoguider.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/mgen_device.cpp
   )

add_executable(indi_mgenautoguider ${indimgenautoguider_SRCS})

target_link_libraries(indi_mgenautoguider ${INDI_DRIVER_LIBRARIES} ${FTDI1_LIBRARIES} ${LIBUSB_1_LIBRARIES} pthread)

install(TARGETS indi_mgenautoguider RUNTIME DESTINATION bin )

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_mgenautoguider.xml DESTINATION ${INDI_DATA_DIR})

