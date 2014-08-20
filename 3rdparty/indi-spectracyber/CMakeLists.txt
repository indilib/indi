cmake_minimum_required(VERSION 2.4.7)
PROJECT(indi_spectracyber C CXX)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
set(BIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/bin")

find_package(CFITSIO REQUIRED)
find_package(INDI REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)
find_package(Nova REQUIRED)


include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${NOVA_INCLUDE_DIR}/..)

########### SpectraCyber ###########
set(indispectracyber_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/spectracyber.cpp
   )

add_executable(indi_spectracyber ${indispectracyber_SRCS})

target_link_libraries(indi_spectracyber ${INDI_DRIVER_LIBRARIES} ${NOVA_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_THREAD_LIBS_INIT})

install(TARGETS indi_spectracyber RUNTIME DESTINATION bin)

install(FILES indi_spectracyber.xml indi_spectracyber_sk.xml DESTINATION ${INDI_DATA_DIR})

