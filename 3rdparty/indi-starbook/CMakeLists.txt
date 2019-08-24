cmake_minimum_required(VERSION 2.8)
PROJECT(indi_starbook CXX C)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake_modules/")

include(GNUInstallDirs)

set(STARBOOK_DRIVER_VERSION_MAJOR 0)
set(STARBOOK_DRIVER_VERSION_MINOR 8)

find_package(INDI REQUIRED)
find_package(CURL REQUIRED)
find_package(Nova REQUIRED)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_starbook_telescope.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_starbook_telescope.xml)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${INDI_INCLUDE_DIR})
include_directories(${NOVA_INCLUDE_DIR})
include_directories(${CURL_INCLUDE_DIR})

include(CMakeCommon)

############# STARBOOK ###############
set(indi_starbook_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/indi_starbook.cpp ${CMAKE_CURRENT_SOURCE_DIR}/starbook_types.cpp)

add_executable(indi_starbook_telescope ${indi_starbook_SRCS} connectioncurl.cpp connectioncurl.h command_interface.cpp command_interface.h)

target_link_libraries(indi_starbook_telescope ${INDI_LIBRARIES} ${NOVA_LIBRARIES} ${CURL_LIBRARIES})

install(TARGETS indi_starbook_telescope RUNTIME DESTINATION bin)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_starbook_telescope.xml DESTINATION ${INDI_DATA_DIR})

#####################################
#OPTION(INDI_BUILD_UNITTESTS "manual switch" on)
if (INDI_BUILD_UNITTESTS)
    # Workaround for fixing a linking error caused by "-pie" flag in CMakeCommon
    set(CMAKE_EXE_LINKER_FLAGS "-Wl,-z,nodump -Wl,-z,noexecstack -Wl,-z,relro -Wl,-z,now")

    enable_testing()

    find_package(GTest REQUIRED)

    include_directories(${GTEST_INCLUDE_DIRS})

    add_executable(test_starbook test_starbook.cpp ${indi_starbook_SRCS} connectioncurl.cpp connectioncurl.h command_interface.cpp command_interface.h)


    #   test shouldn't be so dependent on external libs, but here we are
    target_link_libraries(test_starbook
            ${INDI_LIBRARIES} ${NOVA_LIBRARIES} ${CURL_LIBRARIES} ${GTEST_BOTH_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT}
            )

    add_test(run-tests test_starbook)
endif ()
