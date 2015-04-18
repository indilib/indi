########### NexStar Evolution INDI driver ##############
PROJECT(indi-nexstarevo C CXX)
cmake_minimum_required(VERSION 2.4.7)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
set(BIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/bin")
set(DATA_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/share/indi/")
set(CMAKE_CXX_FLAGS "-std=c++0x ${CMAKE_CXX_FLAGS}")

find_package(INDI REQUIRED)
find_package(Nova REQUIRED)
find_package(ZLIB REQUIRED)

set(NSEVO_VERSION_MAJOR 0)
set(NSEVO_VERSION_MINOR 2)

set(INDI_DATA_DIR "${CMAKE_INSTALL_PREFIX}/share/indi")

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${NOVA_INCLUDE_DIR})
include_directories( ${EV_INCLUDE_DIR})

add_executable(indi_nexstarevo_telescope nexstarevo.cpp NexStarAUXScope.cpp)
target_link_libraries(indi_nexstarevo_telescope indiAlignmentDriver ${INDI_LIBRARIES} ${INDI_DRIVER_LIBRARIES} ${NOVA_LIBRARIES})
install(TARGETS indi_nexstarevo_telescope RUNTIME DESTINATION bin )

install( FILES  indi_nexstarevo.xml DESTINATION ${INDI_DATA_DIR})
