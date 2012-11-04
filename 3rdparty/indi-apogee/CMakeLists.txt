cmake_minimum_required(VERSION 2.4.7)
PROJECT(indi_apogee C CXX)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake_modules/")
set(BIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/bin")

find_package(CFITSIO REQUIRED)
find_package(INDI REQUIRED)
find_package(APOGEE REQUIRED)
find_package(ZLIB REQUIRED)

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${CFITSIO_INCLUDE_DIR})
include_directories( ${APOGEE_INCLUDE_DIR})

########### Apogee ###########
set(indiapogee_SRCS
   ${CMAKE_SOURCE_DIR}/indialta.c
   )

add_executable(indi_apogeeu_ccd ${indiapogee_SRCS})
add_executable(indi_apogeee_ccd ${indiapogee_SRCS})

target_link_libraries(indi_apogeeu_ccd ${INDI_MAIN_LIBRARIES} ${CFITSIO_LIBRARIES} ${APOGEEU_LIBRARIES} z m)
target_link_libraries(indi_apogeee_ccd ${INDI_MAIN_LIBRARIES} ${CFITSIO_LIBRARIES} ${APOGEEE_LIBRARIES} z m)

install(TARGETS indi_apogeeu_ccd RUNTIME DESTINATION bin )
install(TARGETS indi_apogeee_ccd RUNTIME DESTINATION bin )

install(FILES indi_apogee.xml DESTINATION ${INDI_DATA_DIR})
