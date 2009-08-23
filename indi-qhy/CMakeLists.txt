cmake_minimum_required(VERSION 2.4.7)
PROJECT(indi_qhy_ccd C)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake_modules/")
set(DATA_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/share/indi/")
set(BIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/bin")

Include (CheckCSourceCompiles)
include (MacroOptionalFindPackage)
include (MacroLogFeature)
include (MacroBoolTo01)
include (CheckIncludeFiles)

find_package(USB REQUIRED)
find_package(CFITSIO REQUIRED)
find_package(INDI REQUIRED)
find_package(ZLIB REQUIRED)

#configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${CFITSIO_INCLUDE_DIR})

if (NOVA_FOUND)
    include_directories(${NOVA_INCLUDE_DIR})
endif (NOVA_FOUND)

########### QHY ###########
set(indiqhy_SRCS
   ${CMAKE_SOURCE_DIR}/qhy_ccd.c
   ${CMAKE_SOURCE_DIR}/qhy5_driver.c
   )

add_executable(indi_qhy_ccd ${indiqhy_SRCS})

target_link_libraries(indi_qhy_ccd ${INDI_LIBRARIES} ${INDI_DRIVER_LIBRARIES} ${CFITSIO_LIBRARIES} ${QHY_LIBRARIES} ${LIBUSB_LIBRARIES})

if (NOVA_FOUND)
  target_link_libraries(indi_qhy_ccd ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

install(TARGETS indi_qhy_ccd RUNTIME DESTINATION bin )

install(FILES indi_qhy.xml DESTINATION ${DATA_INSTALL_DIR})
