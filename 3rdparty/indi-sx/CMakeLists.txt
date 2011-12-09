cmake_minimum_required(VERSION 2.4.7)
PROJECT(sxccd CXX C)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake_modules/")

Include (CheckCSourceCompiles)
include (MacroOptionalFindPackage)
include (MacroLogFeature)
include (MacroBoolTo01)
include (CheckIncludeFiles)

find_package(USB REQUIRED)
find_package(CFITSIO REQUIRED)
find_package(INDI REQUIRED)
find_package(ZLIB REQUIRED)


include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${CFITSIO_INCLUDE_DIR})

if (NOVA_FOUND)
    include_directories(${NOVA_INCLUDE_DIR})
endif (NOVA_FOUND)

########### SX CCD ###########
set(indisxccd_SRCS
   ${CMAKE_SOURCE_DIR}/sxcam.cpp
   )

add_executable(indi_sx_ccd ${indisxccd_SRCS})

target_link_libraries(indi_sx_ccd ${INDI_LIBRARIES} ${INDI_DRIVER_LIBRARIES} ${CFITSIO_LIBRARIES} ${ZLIB_LIBRARIES} ${LIBUSB_LIBRARIES})

if (NOVA_FOUND)
  target_link_libraries(indi_sx_ccd ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

install(TARGETS indi_sx_ccd RUNTIME DESTINATION bin )


########### SX Wheel ###########
set(indisxwheel_SRCS
   ${CMAKE_SOURCE_DIR}/sxwheel.cpp
   )

add_executable(indi_sx_wheel ${indisxwheel_SRCS})

target_link_libraries(indi_sx_wheel ${INDI_LIBRARIES} ${INDI_DRIVER_LIBRARIES} ${ZLIB_LIBRARIES} )

if (NOVA_FOUND)
  target_link_libraries(indi_sx_wheel ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

install(TARGETS indi_sx_wheel RUNTIME DESTINATION bin )

install(FILES indi_sx.xml DESTINATION ${INDI_DATA_DIR} )

