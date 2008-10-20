cmake_minimum_required(VERSION 2.4.7)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake_modules/")
set(DATA_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/share/indi/")
set(BIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/bin")
set(INCLUDE_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/include")

MESSAGE( STATUS "BIN_INSTALL_DIR: " ${BIN_INSTALL_DIR} )

IF(UNIX AND NOT WIN32 AND NOT APPLE)
 IF(CMAKE_SIZEOF_VOID_P MATCHES "8")
     SET(LIB_POSTFIX "64" CACHE STRING "suffix for 32/64 dir placement")
     MARK_AS_ADVANCED(LIB_POSTFIX)
 ENDIF(CMAKE_SIZEOF_VOID_P MATCHES "8")
ENDIF(UNIX AND NOT WIN32 AND NOT APPLE)
IF(NOT DEFINED LIB_POSTFIX)
   SET(LIB_POSTFIX "")
ENDIF(NOT DEFINED LIB_POSTFIX)

Include (CheckCXXSourceCompiles)
include (MacroOptionalFindPackage)
include (MacroLogFeature)
include (MacroBoolTo01)
include (CheckIncludeFiles)

find_package(ZLIB REQUIRED)

macro_optional_find_package(USB)
macro_log_feature(LIBUSB_FOUND "libusb" "User level access to USB devices" "http://indi.sf.net" FALSE "" "Provides support for USB based drivers in INDI.")

macro_optional_find_package(CFITSIO)
macro_bool_to_01(CFITSIO_FOUND HAVE_CFITSIO_H)
macro_log_feature(CFITSIO_FOUND "libcfitsio" "A library for reading and writing data files in FITS (Flexible Image Transport System) data format" "http://indi.sf.net" FALSE "3.03" "Provides INDI with FITS I/O support.")


macro_optional_find_package(FLI)
macro_log_feature(FLI_FOUND "libfli" "Finger Lakes Instrument Library" "http://indi.sourceforge.net/" FALSE "1.71" "Provides INDI with support for controlling FLI CCDS & Filter Wheels.")

macro_optional_find_package(Nova)
macro_bool_to_01(NOVA_FOUND HAVE_NOVA_H)
macro_log_feature(NOVA_FOUND "libnova" "A general purpose, double precision, Celestial Mechanics, Astrometry and Astrodynamics library" "http://indi.sf.net" FALSE "0.12.1" "Provides INDI with astrodynamics library.")

check_include_files(linux/videodev2.h HAVE_LINUX_VIDEODEV2_H)
check_include_files(termios.h TERMIOS_FOUND)
macro_bool_to_01(TERMIOS_FOUND HAVE_TERMIOS_H)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_SOURCE_DIR})
include_directories( ${CMAKE_SOURCE_DIR}/libs)
include_directories( ${CMAKE_SOURCE_DIR}/libs/webcam)

if (CFITSIO_FOUND)
  include_directories(${CFITSIO_INCLUDE_DIR})
endif (CFITSIO_FOUND)

if (NOVA_FOUND)
    include_directories(${NOVA_INCLUDE_DIR})
endif (NOVA_FOUND)

if (FLI_FOUND)
    include_directories(${FLI_INCLUDE_DIR})
endif (FLI_FOUND)

set(liblilxml_SRCS  ${CMAKE_SOURCE_DIR}/libs/lilxml.c )

set(libindicom_SRCS
	${CMAKE_SOURCE_DIR}/libs/indicom.c
	${CMAKE_SOURCE_DIR}/base64.c
	)

set(libwebcam_SRCS
	${CMAKE_SOURCE_DIR}/libs/webcam/PPort.cpp
	${CMAKE_SOURCE_DIR}/libs/webcam/port.cpp
	${CMAKE_SOURCE_DIR}/libs/webcam/v4l2_base.cpp
	${CMAKE_SOURCE_DIR}/libs/webcam/ccvt_c2.c
	${CMAKE_SOURCE_DIR}/libs/webcam/ccvt_misc.c
	)

set (indimain_SRCS
	${CMAKE_SOURCE_DIR}/indidrivermain.c
	${CMAKE_SOURCE_DIR}/eventloop.c
    )

######################################
########### INDI SERVER ##############
######################################

set(indiserver_SRCS indiserver.c fq.c)

add_executable(indiserver ${indiserver_SRCS}  ${liblilxml_SRCS})

target_link_libraries(indiserver  pthread )

if (NOVA_FOUND)
  target_link_libraries(indiserver ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

install(TARGETS indiserver RUNTIME DESTINATION bin )

######################################
########### INDI Static Library ######
######################################
add_library(indi STATIC ${indimain_SRCS} ${libindicom_SRCS} ${liblilxml_SRCS})
install(TARGETS indi ARCHIVE DESTINATION lib${LIB_POSTFIX})

#####################################
########## TELESCOPE GROUP ##########
#####################################

########### LX200 Basic #############
set(lx200basic_SRCS
   ${indimain_SRCS}
   ${CMAKE_SOURCE_DIR}/drivers/telescope/lx200driver.c
   ${CMAKE_SOURCE_DIR}/drivers/telescope/lx200basic.cpp )

add_executable(indi_lx200basic ${lx200basic_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

if (NOVA_FOUND)
  target_link_libraries(indi_lx200basic ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

install(TARGETS indi_lx200basic RUNTIME DESTINATION bin )

#################################################################################

########### LX200 Generic ###########
set(lx200generic_SRCS
   ${indimain_SRCS}
   ${CMAKE_SOURCE_DIR}/drivers/telescope/lx200driver.c
   ${CMAKE_SOURCE_DIR}/drivers/telescope/lx200autostar.cpp
   ${CMAKE_SOURCE_DIR}/drivers/telescope/lx200_16.cpp
   ${CMAKE_SOURCE_DIR}/drivers/telescope/lx200gps.cpp
   ${CMAKE_SOURCE_DIR}/drivers/telescope/lx200generic.cpp
   ${CMAKE_SOURCE_DIR}/drivers/telescope/lx200classic.cpp
   ${CMAKE_SOURCE_DIR}/drivers/telescope/lx200apdriver.c 
   ${CMAKE_SOURCE_DIR}/drivers/telescope/lx200ap.cpp )

add_executable(indi_lx200generic ${lx200generic_SRCS}  ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_lx200generic  m )

if (NOVA_FOUND)
  target_link_libraries(indi_lx200generic ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

install(TARGETS indi_lx200generic RUNTIME DESTINATION bin )

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/make_lx200generic_symlink.cmake 
"exec_program(${CMAKE_COMMAND} ARGS -E create_symlink ${BIN_INSTALL_DIR}/indi_lx200generic ${BUILD_ROOT}${BIN_INSTALL_DIR}/indi_lx200classic)\n
exec_program(${CMAKE_COMMAND} ARGS -E create_symlink ${BIN_INSTALL_DIR}/indi_lx200generic ${BUILD_ROOT}${BIN_INSTALL_DIR}/indi_lx200autostar)\n 
exec_program(${CMAKE_COMMAND} ARGS -E create_symlink ${BIN_INSTALL_DIR}/indi_lx200generic ${BUILD_ROOT}${BIN_INSTALL_DIR}/indi_lx200_16)\n
exec_program(${CMAKE_COMMAND} ARGS -E create_symlink ${BIN_INSTALL_DIR}/indi_lx200generic ${BUILD_ROOT}${BIN_INSTALL_DIR}/indi_lx200gps)\n
exec_program(${CMAKE_COMMAND} ARGS -E create_symlink ${BIN_INSTALL_DIR}/indi_lx200generic ${BUILD_ROOT}${BIN_INSTALL_DIR}/indi_lx200ap)\n
")
set_target_properties(indi_lx200generic PROPERTIES POST_INSTALL_SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/make_lx200generic_symlink.cmake)
#################################################################################

########### Celestron GPS ############
set(celestrongps_SRCS
   ${indimain_SRCS}
   ${CMAKE_SOURCE_DIR}/drivers/telescope/celestronprotocol.c
   ${CMAKE_SOURCE_DIR}/drivers/telescope/celestrongps.cpp )

add_executable(indi_celestron_gps ${celestrongps_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_celestron_gps m )

if (NOVA_FOUND)
  target_link_libraries(indi_celestron_gps ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

install(TARGETS indi_celestron_gps RUNTIME DESTINATION bin )

#################################################################################

########### Orion Atlas #############
set(orionatlas_SRCS
   ${indimain_SRCS}
   ${CMAKE_SOURCE_DIR}/drivers/telescope/orionatlas.cpp )

add_executable(indi_orion_atlas ${orionatlas_SRCS}  ${liblilxml_SRCS} ${libindicom_SRCS})

#target_link_libraries(indi_orion_atlas ${KDE4_KDECORE_LIBS})

if (NOVA_FOUND)
  target_link_libraries(indi_orion_atlas ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

install(TARGETS indi_orion_atlas RUNTIME DESTINATION bin )

#################################################################################

########### Takahashi Temma ##########
if (NOVA_FOUND)

set(temma_SRCS
   ${indimain_SRCS}
   ${CMAKE_SOURCE_DIR}/drivers/telescope/temmadriver.c )

add_executable(indi_temma ${temma_SRCS}  ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_temma  ${NOVA_LIBRARIES} m )

install(TARGETS indi_temma RUNTIME DESTINATION bin )

endif (NOVA_FOUND)
#################################################################################

########### Sky Commander #############
set(skycommander_SRCS
   ${indimain_SRCS}
   ${CMAKE_SOURCE_DIR}/drivers/telescope/lx200driver.c
   ${CMAKE_SOURCE_DIR}/drivers/telescope/skycommander.c )

add_executable(indi_skycommander ${skycommander_SRCS}  ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_skycommander  m )

if (NOVA_FOUND)
  target_link_libraries(indi_skycommander ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

install(TARGETS indi_skycommander  RUNTIME DESTINATION bin )

#################################################################################

########### Intelliscope ###############
set(intelliscope_SRCS
   ${indimain_SRCS}
   ${CMAKE_SOURCE_DIR}/drivers/telescope/lx200driver.c
   ${CMAKE_SOURCE_DIR}/drivers/telescope/intelliscope.c )

add_executable(indi_intelliscope ${intelliscope_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_intelliscope  m )

if (NOVA_FOUND)
  target_link_libraries(indi_intelliscope ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

install(TARGETS indi_intelliscope RUNTIME DESTINATION bin )

#####################################
########## FOCUSER GROUP ############
#####################################

###### FLI Precision Digital Focuser ######
if (FLI_FOUND)
set(flipdf_SRCS
	${indimain_SRCS}
	${CMAKE_SOURCE_DIR}/drivers/focuser/fli_pdf.c
)

add_executable(indi_fli_pdf ${flipdf_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_fli_pdf m ${FLI_LIBRARIES})

if (NOVA_FOUND)
  target_link_libraries(indi_fli_pdf ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

install(TARGETS indi_fli_pdf RUNTIME DESTINATION bin )
endif(FLI_FOUND)
#################################################################################

################ Robo Focuser ################

set(robofocus_SRCS
	${indimain_SRCS}
	${CMAKE_SOURCE_DIR}/drivers/focuser/robofocus.c
	${CMAKE_SOURCE_DIR}/drivers/focuser/robofocusdriver.c
   )

add_executable(indi_robofocus ${robofocus_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_robofocus m)

if (NOVA_FOUND)
  target_link_libraries(indi_robofocus ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

install(TARGETS indi_robofocus RUNTIME DESTINATION bin )

#################################################################################

#####################################
######## FILTER WHEEL GROUP #########
#####################################

########## True Technology Wheel ############
set(trutechwheel_SRCS
	${indimain_SRCS}
	${CMAKE_SOURCE_DIR}/drivers/filter_wheel/trutech_wheel.c
   )

add_executable(indi_trutech_wheel ${trutechwheel_SRCS}  ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_trutech_wheel  m)

if (NOVA_FOUND)
  target_link_libraries(indi_trutech_wheel ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

install(TARGETS indi_trutech_wheel RUNTIME DESTINATION bin )

#################################################################################

########## FLI Filter Wheel ############
if (FLI_FOUND)
set(fliwheel_SRCS
	${indimain_SRCS}
	${CMAKE_SOURCE_DIR}/drivers/filter_wheel/fli_wheel.c
   )

add_executable(indi_fli_wheel ${fliwheel_SRCS}  ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_fli_wheel ${FLI_LIBRARIES} m)

if (NOVA_FOUND)
  target_link_libraries(indi_fli_wheel ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

install(TARGETS indi_fli_wheel RUNTIME DESTINATION bin )
endif (FLI_FOUND)
#################################################################################

#######################################
############# CCD GROUP ###############
#######################################

############# FLI CCD ###############
if (CFITSIO_FOUND AND FLI_FOUND)

set(fliccd_SRCS
	${indimain_SRCS}
	${CMAKE_SOURCE_DIR}/drivers/ccd/fli_ccd.c
)

add_executable(indi_fli_ccd ${fliccd_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_fli_ccd ${FLI_LIBRARIES} ${CFITSIO_LIBRARIES} m z)

if (NOVA_FOUND)
  target_link_libraries(indi_fli_ccd ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

install(TARGETS indi_fli_ccd RUNTIME DESTINATION bin )

endif (CFITSIO_FOUND AND FLI_FOUND)
#################################################################################

#########################################
########### VIDEO GROUP   ###############
#########################################

########### STV #######################
if (CFITSIO_FOUND)
if (NOVA_FOUND)

set(stv_SRCS
   ${indimain_SRCS}
   ${CMAKE_SOURCE_DIR}/drivers/video/stvdriver.c
   ${CMAKE_SOURCE_DIR}/drivers/video/stv.c )

add_executable(indi_sbig_stv ${stv_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_sbig_stv z ${NOVA_LIBRARIES} ${CFITSIO_LIBRARIES})

install(TARGETS indi_sbig_stv RUNTIME DESTINATION bin )

endif (NOVA_FOUND)
endif(CFITSIO_FOUND)

#################################################################################

### Meade Lunar Planetary Imager ########
if (CFITSIO_FOUND)

ADD_DEFINITIONS(-DHAVE_LINUX_VIDEODEV2_H)

set(meade_lpi_SRCS
	${indimain_SRCS}
	${CMAKE_SOURCE_DIR}/drivers/video/v4ldriver.cpp
	${CMAKE_SOURCE_DIR}/drivers/video/indi_lpi.cpp
   )

add_executable(indi_meade_lpi ${meade_lpi_SRCS} ${libwebcam_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_meade_lpi z ${CFITSIO_LIBRARIES})

if (NOVA_FOUND)
  target_link_libraries(indi_meade_lpi ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

install(TARGETS indi_meade_lpi RUNTIME DESTINATION bin )

endif (CFITSIO_FOUND)

#################################################################################

########### V4L Philips ##############
if (CFITSIO_FOUND)

set(v4lphilips_SRCS
	${indimain_SRCS}
	${CMAKE_SOURCE_DIR}/drivers/video/v4ldriver.cpp
	${CMAKE_SOURCE_DIR}/drivers/video/v4lphilips.cpp
	${CMAKE_SOURCE_DIR}/drivers/video/indi_philips.cpp
)

add_executable(indi_v4l_philips ${v4lphilips_SRCS} ${libwebcam_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_v4l_philips m z ${CFITSIO_LIBRARIES})

if (NOVA_FOUND)
  target_link_libraries(indi_v4l_philips ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

install(TARGETS indi_v4l_philips RUNTIME DESTINATION bin )

endif (CFITSIO_FOUND)

#################################################################################

########### Generic V4L Driver ###############
if (CFITSIO_FOUND)

set(v4ldriver_SRCS
	${indimain_SRCS}
	${CMAKE_SOURCE_DIR}/drivers/video/v4ldriver.cpp
	${CMAKE_SOURCE_DIR}/drivers/video/indi_v4l.cpp
   )

add_executable(indi_v4l_generic ${v4ldriver_SRCS} ${libwebcam_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_v4l_generic m z ${CFITSIO_LIBRARIES})

if (NOVA_FOUND)
  target_link_libraries(indi_v4l_generic ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

install(TARGETS indi_v4l_generic RUNTIME DESTINATION bin )

endif (CFITSIO_FOUND)

#################################################################################

########### getINDI ##############
set(getindi_SRCS
	${CMAKE_SOURCE_DIR}/eventloop.c
	${CMAKE_SOURCE_DIR}/tools/getINDIproperty.c
   )

add_executable(indi_getprop ${getindi_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_getprop m z)

if (NOVA_FOUND)
  target_link_libraries(indi_getprop ${NOVA_LIBRARIES})
endif (NOVA_FOUND)


install(TARGETS indi_getprop RUNTIME DESTINATION bin )

#################################################################################

########### setINDI ##############
set(setindi_SRCS
	${CMAKE_SOURCE_DIR}/eventloop.c
	${CMAKE_SOURCE_DIR}/tools/setINDIproperty.c
   )

add_executable(indi_setprop ${setindi_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_setprop m z)

if (NOVA_FOUND)
  target_link_libraries(indi_setprop ${NOVA_LIBRARIES})
endif (NOVA_FOUND)


install(TARGETS indi_setprop RUNTIME DESTINATION bin )

#################################################################################

########### evelINDI ##############
set(evalindi_SRCS
	${CMAKE_SOURCE_DIR}/eventloop.c
	${CMAKE_SOURCE_DIR}/tools/compiler.c
	${CMAKE_SOURCE_DIR}/tools/evalINDI.c
   )

add_executable(indi_eval ${evalindi_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_eval m z)

if (NOVA_FOUND)
  target_link_libraries(indi_eval ${NOVA_LIBRARIES})
endif (NOVA_FOUND)


install(TARGETS indi_eval RUNTIME DESTINATION bin )

#################################################################################

install( FILES drivers.xml DESTINATION ${DATA_INSTALL_DIR})

install( FILES indiapi.h indidevapi.h base64.h eventloop.h ${CMAKE_SOURCE_DIR}/libs/lilxml.h
 ${CMAKE_SOURCE_DIR}/libs/indicom.h DESTINATION ${INCLUDE_INSTALL_DIR}/libindi COMPONENT Devel)
 