cmake_minimum_required(VERSION 2.8)
PROJECT(indi_avalon)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake_modules/")
include(GNUInstallDirs)

find_package(INDI REQUIRED)
find_package(Nova REQUIRED)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../libindi)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../libindi/drivers/telescope)
include_directories(${INDI_INCLUDE_DIR})
include_directories(${NOVA_INCLUDE_DIR})

include(CMakeCommon)

IF ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 6.3.9)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wformat-overflow=2")
ENDIF ()

########### LX200 StarGO ###########
SET(lx200stargo_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libindi/drivers/telescope/lx200telescope.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libindi/drivers/telescope/lx200driver.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/lx200stargofocuser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/lx200stargo.cpp
    )


add_executable(indi_lx200stargo ${lx200stargo_SRCS})
target_link_libraries(indi_lx200stargo ${INDI_LIBRARIES} ${NOVA_LIBRARIES})

install(TARGETS indi_lx200stargo RUNTIME DESTINATION bin )

