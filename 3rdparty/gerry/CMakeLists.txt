cmake_minimum_required(VERSION 2.4.7)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
SET(CMAKE_C_FLAGS "-Wall" )
SET(CMAKE_C_FLAGS_DEBUG "-Werror" )

Include (CheckCSourceCompiles)
Include (CheckCXXSourceCompiles)
include (MacroOptionalFindPackage)
include (MacroLogFeature)
include (MacroBoolTo01)
include (CheckIncludeFiles)

find_package(CFITSIO REQUIRED)
find_package(USB REQUIRED)
find_package(INDI REQUIRED)
find_package(ZLIB REQUIRED)

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${CFITSIO_INCLUDE_DIR})


########### drivers ###########

set(indi_ccdsim_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/IndiDevice.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/IndiCcd.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/CcdSim.cpp
   )

add_executable(indi_ccdsim ${indi_ccdsim_SRCS})
target_link_libraries(indi_ccdsim ${INDI_LIBRARIES} ${INDI_BASE_DRIVER_LIBRARIES} ${INDI_DEFAULT_DRIVER_LIBRARIES} ${CFITSIO_LIBRARIES} ${ZLIB_LIBRARIES} )

set(indi_scopesim_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/IndiDevice.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/IndiTelescope.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/ScopeSim.cpp
   )

add_executable(indi_scopesim ${indi_scopesim_SRCS})
target_link_libraries(indi_scopesim ${INDI_LIBRARIES} ${INDI_BASE_DRIVER_LIBRARIES} ${INDI_DEFAULT_DRIVER_LIBRARIES} ${CFITSIO_LIBRARIES} ${ZLIB_LIBRARIES} )

set(indi_filtersim_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/IndiDevice.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/IndiFilterWheel.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/FilterSim.cpp
   )

add_executable(indi_filtersim ${indi_filtersim_SRCS})
target_link_libraries(indi_filtersim ${INDI_LIBRARIES} ${INDI_BASE_DRIVER_LIBRARIES} ${INDI_DEFAULT_DRIVER_LIBRARIES} ${CFITSIO_LIBRARIES} ${ZLIB_LIBRARIES} )

set(indi_synscan_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/IndiDevice.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/IndiTelescope.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/SynscanMount.cpp
   )

add_executable(indi_synscan ${indi_synscan_SRCS})
target_link_libraries(indi_synscan ${INDI_LIBRARIES} ${INDI_BASE_DRIVER_LIBRARIES} ${INDI_DEFAULT_DRIVER_LIBRARIES} ${CFITSIO_LIBRARIES} ${ZLIB_LIBRARIES} ${LIBUSB_LIBRARIES} )

set(indi_fcusb_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/IndiDevice.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/IndiFocusser.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/UsbDevice.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/fcusb.cpp
   )

add_executable(indi_fcusb ${indi_fcusb_SRCS})
target_link_libraries(indi_fcusb ${INDI_LIBRARIES} ${INDI_BASE_DRIVER_LIBRARIES} ${INDI_DEFAULT_DRIVER_LIBRARIES} ${CFITSIO_LIBRARIES} ${ZLIB_LIBRARIES} ${LIBUSB_LIBRARIES} )

set(indi_sxwheel_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/IndiDevice.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/IndiFilterWheel.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/UsbDevice.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/SxWheel.cpp
   )

add_executable(indi_sxwheel ${indi_sxwheel_SRCS})
target_link_libraries(indi_sxwheel ${INDI_LIBRARIES} ${INDI_BASE_DRIVER_LIBRARIES} ${INDI_DEFAULT_DRIVER_LIBRARIES} ${CFITSIO_LIBRARIES} ${ZLIB_LIBRARIES} ${LIBUSB_LIBRARIES} )

set(indi_sxcam_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/IndiDevice.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/IndiCcd.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/UsbDevice.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/SxCam.cpp
   )

add_executable(indi_sxcam ${indi_sxcam_SRCS})
target_link_libraries(indi_sxcam ${INDI_LIBRARIES} ${INDI_BASE_DRIVER_LIBRARIES} ${INDI_DEFAULT_DRIVER_LIBRARIES} ${CFITSIO_LIBRARIES} ${ZLIB_LIBRARIES} ${LIBUSB_LIBRARIES} )

set(testclient_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/testclient/testclient.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/testclient/main.cpp
   )

add_executable(testclient ${testclient_SRCS})
target_link_libraries(testclient ${INDI_LIBRARIES} ${INDI_CLIENT_LIBRARIES} )

#install(TARGETS indi_ccdsim RUNTIME DESTINATION bin )

