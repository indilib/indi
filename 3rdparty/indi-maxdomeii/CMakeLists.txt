cmake_minimum_required(VERSION 2.8)
project(indi_maxdomeii C CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake_modules/")
include(GNUInstallDirs)

set(INDI_MAXDOMEII_VERSION_MAJOR 1)
set(INDI_MAXDOMEII_VERSION_MINOR 3)

find_package(INDI REQUIRED)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/config.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_maxdomeii.xml.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/indi_maxdomeii.xml)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${INDI_INCLUDE_DIR})

include(CMakeCommon)

###############
## MaxDome II
###############

set(indimaxdomeii_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/maxdomeiidriver.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/maxdomeii.cpp
)

add_executable(indi_maxdomeii ${indimaxdomeii_SRCS})

target_link_libraries(indi_maxdomeii ${INDI_LIBRARIES})

install(TARGETS indi_maxdomeii RUNTIME DESTINATION bin )
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_maxdomeii.xml DESTINATION ${INDI_DATA_DIR})

##############
# Testing
##############

if (INDI_BUILD_UNITTESTS)
    # Workaround for fixing a linking error caused by "-pie" flag in CMakeCommon
    set(CMAKE_EXE_LINKER_FLAGS "-Wl,-z,nodump -Wl,-z,noexecstack -Wl,-z,relro -Wl,-z,now")

    enable_testing()

    find_package(GTest REQUIRED)

    include_directories (${GTEST_INCLUDE_DIRS})

    add_executable(test-maxdomeii test_maxdomeii.cpp ${indimaxdomeii_SRCS})

    target_link_libraries(test-maxdomeii
        ${INDI_LIBRARIES} ${GTEST_BOTH_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT}
    )

    add_test(run-tests test-maxdomeii)
endif()
