cmake_minimum_required(VERSION 2.4.7)
PROJECT(aagcloudwatcher CXX C)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")

set (VERSION_MAJOR 1)
set (VERSION_MINOR 2)

find_package(INDI REQUIRED)
find_package(Threads REQUIRED)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${INDI_INCLUDE_DIR})

set(indiaag_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/indi_aagcloudwatcher.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/CloudWatcherController.cpp
   )

add_executable(indi_aagcloudwatcher ${indiaag_SRCS})
target_link_libraries(indi_aagcloudwatcher ${INDI_DRIVER_LIBRARIES})

set(test_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/CloudWatcherController.cpp
   )

add_executable(aagcloudwatcher_test ${test_SRCS})

install(TARGETS indi_aagcloudwatcher RUNTIME DESTINATION bin)
install(TARGETS aagcloudwatcher_test RUNTIME DESTINATION bin)
install(FILES indi_aagcloudwatcher.xml DESTINATION ${INDI_DATA_DIR})
install(FILES indi_aagcloudwatcher_sk.xml DESTINATION ${INDI_DATA_DIR})
