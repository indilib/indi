########### NexStar Evolution INDI driver ##############
PROJECT(indi-nexstarevo C CXX)
cmake_minimum_required(VERSION 2.8)

include(GNUInstallDirs)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake_modules/")

find_package(INDI REQUIRED)
find_package(Nova REQUIRED)
find_package(ZLIB REQUIRED)

set(NSEVO_VERSION_MAJOR 0)
set(NSEVO_VERSION_MINOR 3)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_nexstarevo.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_nexstarevo.xml )

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${NOVA_INCLUDE_DIR})
include_directories( ${EV_INCLUDE_DIR})

IF ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++11")
ELSE ()
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
ENDIF ()

add_executable(indi_nexstarevo_telescope nexstarevo.cpp NexStarAUXScope.cpp)
target_link_libraries(indi_nexstarevo_telescope ${INDI_LIBRARIES} ${NOVA_LIBRARIES})
install(TARGETS indi_nexstarevo_telescope RUNTIME DESTINATION bin )

install( FILES  ${CMAKE_CURRENT_BINARY_DIR}/indi_nexstarevo.xml DESTINATION ${INDI_DATA_DIR})
