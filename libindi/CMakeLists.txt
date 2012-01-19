cmake_minimum_required(VERSION 2.4.7)
PROJECT(libindi C CXX)

##################  INDI version  ################################
set(INDI_SOVERSION "0")
set(CMAKE_INDI_VERSION_MAJOR 0)
set(CMAKE_INDI_VERSION_MINOR 9)
set(CMAKE_INDI_VERSION_RELEASE 0)
set(CMAKE_INDI_VERSION_STRING "${CMAKE_INDI_VERSION_MAJOR}.${CMAKE_INDI_VERSION_MINOR}.${CMAKE_INDI_VERSION_RELEASE}")

##################  Paths  ################################
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake_modules/")
set(DATA_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/share/indi/")
set(BIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/bin")
set(INCLUDE_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/include")

##################  setup install directories  ################################
set (LIB_SUFFIX "" CACHE STRING "Define suffix of directory name (32/64)" )
set (LIB_DESTINATION "${CMAKE_INSTALL_PREFIX}/lib${LIB_SUFFIX}" CACHE STRING "Library directory name")
## the following are directories where stuff will be installed to
set(INCLUDE_INSTALL_DIR      "${CMAKE_INSTALL_PREFIX}/include/" CACHE PATH "The subdirectory to the header prefix")
set(PKGCONFIG_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}/lib${LIB_SUFFIX}/pkgconfig/" CACHE STRING "Base directory for pkgconfig files")

##################  Includes  ################################
Include (CheckCXXSourceCompiles)
include (MacroOptionalFindPackage)
include (MacroLogFeature)
include (MacroBoolTo01)
include (CheckIncludeFiles)

FIND_PACKAGE(ZLIB REQUIRED)
FIND_PACKAGE(USB REQUIRED)
FIND_PACKAGE(CFITSIO REQUIRED)

if (NOT CFITSIO_FOUND OR CFITSIO_VERSION_MAJOR LESS 3)
  message(FATAL_ERROR "CFITSIO version too old, Please install cfitsio 3.x and try again. http://heasarc.gsfc.nasa.gov/fitsio/fitsio.html")
endif (NOT CFITSIO_FOUND OR CFITSIO_VERSION_MAJOR LESS 3)

macro_bool_to_01(CFITSIO_FOUND HAVE_CFITSIO_H)
macro_log_feature(CFITSIO_FOUND "libcfitsio" "A library for reading and writing data files in FITS (Flexible Image Transport System) data format" "http://heasarc.gsfc.nasa.gov/fitsio/fitsio.html" FALSE "3.03" "Provides INDI with FITS I/O support.")

macro_optional_find_package(Nova)
macro_bool_to_01(NOVA_FOUND HAVE_NOVA_H)
macro_log_feature(NOVA_FOUND "libnova" "A general purpose, double precision, Celestial Mechanics, Astrometry and Astrodynamics library" "http://libnova.sourceforge.net" FALSE "0.12.1" "Provides INDI with astrodynamics library.")

check_include_files(linux/videodev2.h HAVE_LINUX_VIDEODEV2_H)
check_include_files(termios.h TERMIOS_FOUND)
macro_bool_to_01(TERMIOS_FOUND HAVE_TERMIOS_H)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )

if(COMMAND cmake_policy)
 cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)

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
        ${CMAKE_SOURCE_DIR}/indidriver.c
	${CMAKE_SOURCE_DIR}/indidrivermain.c
	${CMAKE_SOURCE_DIR}/eventloop.c
    )

set (indiclient_SRCS
        ${CMAKE_SOURCE_DIR}/libs/indibase/basedriver.cpp
        ${CMAKE_SOURCE_DIR}/libs/indibase/baseclient.cpp
    )

set (indidriver_SRCS
        ${CMAKE_SOURCE_DIR}/libs/indibase/basedriver.cpp
        ${CMAKE_SOURCE_DIR}/libs/indibase/defaultdriver.cpp
        ${CMAKE_SOURCE_DIR}/libs/indibase/indiccd.cpp
        ${CMAKE_SOURCE_DIR}/libs/indibase/inditelescope.cpp
        ${CMAKE_SOURCE_DIR}/libs/indibase/indifilterwheel.cpp
        ${CMAKE_SOURCE_DIR}/libs/indibase/indifocuser.cpp
        ${CMAKE_SOURCE_DIR}/libs/indibase/indiusbdevice.cpp
        ${CMAKE_SOURCE_DIR}/libs/indibase/indiguiderinterface.cpp
        ${CMAKE_SOURCE_DIR}/libs/indibase/indifilterinterface.cpp

    )


######################################
########### INDI SERVER ##############
######################################

set(indiserver_SRCS indiserver.c fq.c)

add_executable(indiserver ${indiserver_SRCS} ${liblilxml_SRCS})

target_link_libraries(indiserver pthread)

if (NOVA_FOUND)
  target_link_libraries(indiserver ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

install(TARGETS indiserver RUNTIME DESTINATION bin)

#################################################
############# INDI Shared Library ###############
# To offer lilxml and communination routines    #
# Mostly used by generic clients                #
#################################################
add_library(indi SHARED ${libindicom_SRCS} ${liblilxml_SRCS})
target_link_libraries(indi m z)

if (NOVA_FOUND)
target_link_libraries(indi ${NOVA_LIBRARIES})
endif(NOVA_FOUND)
if (CFITSIO_FOUND)
target_link_libraries(indi ${CFITSIO_LIBRARIES})
endif(CFITSIO_FOUND)

install(TARGETS indi LIBRARY DESTINATION ${LIB_DESTINATION})
set_target_properties(indi PROPERTIES VERSION ${CMAKE_INDI_VERSION_STRING} SOVERSION ${INDI_SOVERSION})

##################################################
############ INDI Main Static Library ############
# To link with main() for 3rd party drivers      #
##################################################
add_library(indimain STATIC ${indimain_SRCS})
install(TARGETS indimain ARCHIVE DESTINATION ${LIB_DESTINATION})

##################################################
###### INDI Default Driver Static Library ########
# To link with main() and indibase classes       #
##################################################
add_library(indidriver STATIC ${indimain_SRCS} ${indidriver_SRCS})
install(TARGETS indidriver ARCHIVE DESTINATION ${LIB_DESTINATION})

##################################################
########### INDI Client Static Library ###########
##################################################
add_library(indiclient STATIC ${indiclient_SRCS})
target_link_libraries(indiclient indi pthread)
install(TARGETS indiclient ARCHIVE DESTINATION ${LIB_DESTINATION})

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
   ${CMAKE_SOURCE_DIR}/drivers/telescope/lx200ap.cpp 
   ${CMAKE_SOURCE_DIR}/drivers/telescope/lx200fs2.cpp)

add_executable(indi_lx200generic ${lx200generic_SRCS}  ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_lx200generic  m )

if (NOVA_FOUND)
  target_link_libraries(indi_lx200generic ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

install(TARGETS indi_lx200generic RUNTIME DESTINATION bin )

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/make_lx200generic_symlink.cmake
"exec_program(${CMAKE_COMMAND} ARGS -E create_symlink ${BIN_INSTALL_DIR}/indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200classic)\n
exec_program(${CMAKE_COMMAND} ARGS -E create_symlink ${BIN_INSTALL_DIR}/indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200autostar)\n
exec_program(${CMAKE_COMMAND} ARGS -E create_symlink ${BIN_INSTALL_DIR}/indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200_16)\n
exec_program(${CMAKE_COMMAND} ARGS -E create_symlink ${BIN_INSTALL_DIR}/indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200gps)\n
exec_program(${CMAKE_COMMAND} ARGS -E create_symlink ${BIN_INSTALL_DIR}/indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200ap)\n
exec_program(${CMAKE_COMMAND} ARGS -E create_symlink ${BIN_INSTALL_DIR}/indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200fs2)\n
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

########### Syncscan ###############
set(synscan_SRCS
   ${indimain_SRCS}
   ${CMAKE_SOURCE_DIR}/drivers/telescope/synscanmount.cpp )

add_executable(indi_synscan ${synscan_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_synscan indidriver m z)

if (NOVA_FOUND)
  target_link_libraries(indi_synscan ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

install(TARGETS indi_synscan RUNTIME DESTINATION bin )

########### Magellan I #############
set(magellan_SRCS
   ${indimain_SRCS}
   ${CMAKE_SOURCE_DIR}/drivers/telescope/magellandriver.c
   ${CMAKE_SOURCE_DIR}/drivers/telescope/magellan1.cpp )

add_executable(indi_magellan1 ${magellan_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

if (NOVA_FOUND)
  target_link_libraries(indi_magellan1 ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

install(TARGETS indi_magellan1 RUNTIME DESTINATION bin )

########### IEQ45 #############
set(ieq45_SRCS
   ${indimain_SRCS}
   ${CMAKE_SOURCE_DIR}/drivers/telescope/ieq45driver.c
   ${CMAKE_SOURCE_DIR}/drivers/telescope/ieq45.cpp )

add_executable(indi_ieq45 ${ieq45_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

if (NOVA_FOUND)
  target_link_libraries(indi_ieq45 ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

install(TARGETS indi_ieq45 RUNTIME DESTINATION bin )

########### Telescope Simulator ##############
set(telescopesimulator_SRCS
        ${indimain_SRCS}
        ${CMAKE_SOURCE_DIR}/drivers/telescope/telescope_simulator.cpp
   )

add_executable(indi_simulator_telescope ${telescopesimulator_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_simulator_telescope indidriver ${CFITSIO_LIBRARIES} m z pthread)

if (NOVA_FOUND)
  target_link_libraries(indi_simulator_telescope ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

install(TARGETS indi_simulator_telescope RUNTIME DESTINATION bin )

########### CCD Simulator ##############
if (CFITSIO_FOUND)

set(ccdsimulator_SRCS
        ${indimain_SRCS}
        ${CMAKE_SOURCE_DIR}/drivers/ccd/ccd_simulator.cpp
   )

add_executable(indi_simulator_ccd ${ccdsimulator_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_simulator_ccd indidriver ${CFITSIO_LIBRARIES} m z pthread)

if (NOVA_FOUND)
  target_link_libraries(indi_simulator_ccd ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

install(TARGETS indi_simulator_ccd RUNTIME DESTINATION bin )

endif (CFITSIO_FOUND)


#####################################
########## FOCUSER GROUP ############
#####################################

#################################################################################

################ Robo Focuser ################

set(robofocus_SRCS
	${indimain_SRCS}
        ${CMAKE_SOURCE_DIR}/drivers/focuser/robofocus.cpp
   )

add_executable(indi_robo_focus ${robofocus_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_robo_focus indidriver m z)

if (NOVA_FOUND)
  target_link_libraries(indi_robo_focus ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

install(TARGETS indi_robo_focus RUNTIME DESTINATION bin )

################ Optec TCF-S ################

set(tcfs_SRCS
        ${indimain_SRCS}
        ${CMAKE_SOURCE_DIR}/drivers/focuser/tcfs.cpp
   )

add_executable(indi_tcfs_focus ${tcfs_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_tcfs_focus indidriver m z)

if (NOVA_FOUND)
  target_link_libraries(indi_tcfs_focus ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

install(TARGETS indi_tcfs_focus RUNTIME DESTINATION bin )

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/make_tcfs_symlink.cmake
"exec_program(${CMAKE_COMMAND} ARGS -E create_symlink ${BIN_INSTALL_DIR}/indi_tcfs_focus \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_tcfs3_focus)\n")
set_target_properties(indi_tcfs_focus PROPERTIES POST_INSTALL_SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/make_tcfs_symlink.cmake)

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

########### Filter Simulator ##############
set(filtersimulator_SRCS
        ${indimain_SRCS}
        ${CMAKE_SOURCE_DIR}/drivers/filter_wheel/filter_simulator.cpp
   )

add_executable(indi_simulator_wheel ${filtersimulator_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_simulator_wheel indidriver ${CFITSIO_LIBRARIES} m z pthread)

if (NOVA_FOUND)
  target_link_libraries(indi_simulator_wheel ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

install(TARGETS indi_simulator_wheel RUNTIME DESTINATION bin )

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

target_link_libraries(indi_sbig_stv z m ${NOVA_LIBRARIES} ${CFITSIO_LIBRARIES} pthread)

install(TARGETS indi_sbig_stv RUNTIME DESTINATION bin )

endif (NOVA_FOUND)
endif(CFITSIO_FOUND)

#################################################################################

### Meade Lunar Planetary Imager ########
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
if (CFITSIO_FOUND)

ADD_DEFINITIONS(-DHAVE_LINUX_VIDEODEV2_H)

set(meade_lpi_SRCS
	${indimain_SRCS}
	${CMAKE_SOURCE_DIR}/drivers/video/v4ldriver.cpp
	${CMAKE_SOURCE_DIR}/drivers/video/indi_lpi.cpp
   )

add_executable(indi_meade_lpi ${meade_lpi_SRCS} ${libwebcam_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_meade_lpi z ${CFITSIO_LIBRARIES} pthread)

if (NOVA_FOUND)
  target_link_libraries(indi_meade_lpi ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

install(TARGETS indi_meade_lpi RUNTIME DESTINATION bin )

endif (CFITSIO_FOUND)
endif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")

#################################################################################

########### V4L Philips ##############
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
if (CFITSIO_FOUND)

set(v4lphilips_SRCS
	${indimain_SRCS}
	${CMAKE_SOURCE_DIR}/drivers/video/v4ldriver.cpp
	${CMAKE_SOURCE_DIR}/drivers/video/v4lphilips.cpp
	${CMAKE_SOURCE_DIR}/drivers/video/indi_philips.cpp
)

add_executable(indi_v4l_philips ${v4lphilips_SRCS} ${libwebcam_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_v4l_philips m z ${CFITSIO_LIBRARIES} pthread)

if (NOVA_FOUND)
  target_link_libraries(indi_v4l_philips ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

install(TARGETS indi_v4l_philips RUNTIME DESTINATION bin )

endif (CFITSIO_FOUND)
endif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")

#################################################################################

########### Generic V4L Driver ###############
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
if (CFITSIO_FOUND)

set(v4ldriver_SRCS
	${indimain_SRCS}
	${CMAKE_SOURCE_DIR}/drivers/video/v4ldriver.cpp
	${CMAKE_SOURCE_DIR}/drivers/video/indi_v4l.cpp
   )

add_executable(indi_v4l_generic ${v4ldriver_SRCS} ${libwebcam_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_v4l_generic m z ${CFITSIO_LIBRARIES} pthread)

if (NOVA_FOUND)
  target_link_libraries(indi_v4l_generic ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

install(TARGETS indi_v4l_generic RUNTIME DESTINATION bin )

endif (CFITSIO_FOUND)
endif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")

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
## Build Examples. Not installation
########### Tutorial one ##############
set(tutorialone_SRCS
	${indimain_SRCS}
	${CMAKE_SOURCE_DIR}/examples/tutorial_one.c
   )

add_executable(tutorial_one ${tutorialone_SRCS}  ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(tutorial_one  m)

if (NOVA_FOUND)
  target_link_libraries(tutorial_one ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

########### Tutorial two ##############
set(tutorialtwo_SRCS
	${indimain_SRCS}
	${CMAKE_SOURCE_DIR}/examples/tutorial_two.c
   )

add_executable(tutorial_two ${tutorialtwo_SRCS}  ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(tutorial_two  m)

if (NOVA_FOUND)
  target_link_libraries(tutorial_two ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

########### Tutorial three ##############
set(tutorialthree_SRCS
	${indimain_SRCS}
	${CMAKE_SOURCE_DIR}/examples/tutorial_three.c
   )

add_executable(tutorial_three ${tutorialthree_SRCS}  ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(tutorial_three m z)

if (NOVA_FOUND)
  target_link_libraries(tutorial_three ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

########### Tutorial four ##############
if (CFITSIO_FOUND)

set(tutorialfour_SRCS
        ${indimain_SRCS}
        ${CMAKE_SOURCE_DIR}/examples/tutorial_four.cpp
   )

add_executable(tutorial_four ${tutorialfour_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(tutorial_four indidriver ${CFITSIO_LIBRARIES} m z pthread)

if (NOVA_FOUND)
  target_link_libraries(tutorial_four ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

endif (CFITSIO_FOUND)

########### Tutorial dome ##############
set(tutorialdome_SRCS
	${indimain_SRCS}
	${CMAKE_SOURCE_DIR}/examples/tutorial_dome.c
   )

add_executable(tutorial_dome ${tutorialdome_SRCS}  ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(tutorial_dome  m)

if (NOVA_FOUND)
  target_link_libraries(tutorial_dome ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

########### Tutorial rain ##############
set(tutorialrain_SRCS
	${indimain_SRCS}
	${CMAKE_SOURCE_DIR}/examples/tutorial_rain.c
   )

add_executable(tutorial_rain ${tutorialrain_SRCS}  ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(tutorial_rain  m)

if (NOVA_FOUND)
  target_link_libraries(tutorial_rain ${NOVA_LIBRARIES})
endif (NOVA_FOUND)


########### Client Tutorial ##############
set(tutorialclient_SRCS
        ${CMAKE_SOURCE_DIR}/examples/tutorial_client.cpp
   )

add_executable(tutorial_client ${tutorialclient_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(tutorial_client indiclient m)

if (NOVA_FOUND)
  target_link_libraries(tutorial_client ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

#################################################################################

install( FILES drivers.xml ${CMAKE_SOURCE_DIR}/drivers/focuser/indi_tcfs_sk.xml DESTINATION ${DATA_INSTALL_DIR})

install( FILES indiapi.h indidevapi.h base64.h eventloop.h indidriver.h ${CMAKE_SOURCE_DIR}/libs/lilxml.h ${CMAKE_SOURCE_DIR}/libs/indibase/indibase.h
${CMAKE_SOURCE_DIR}/libs/indibase/basedriver.h  ${CMAKE_SOURCE_DIR}/libs/indibase/defaultdriver.h
${CMAKE_SOURCE_DIR}/libs/indibase/indiccd.h  ${CMAKE_SOURCE_DIR}/libs/indibase/indifilterwheel.h
${CMAKE_SOURCE_DIR}/libs/indibase/indifocuser.h  ${CMAKE_SOURCE_DIR}/libs/indibase/inditelescope.h
${CMAKE_SOURCE_DIR}/libs/indibase/baseclient.h ${CMAKE_SOURCE_DIR}/libs/indibase/indiguiderinterface.h
${CMAKE_SOURCE_DIR}/libs/indibase/indifilterinterface.h
${CMAKE_SOURCE_DIR}/libs/indicom.h ${CMAKE_SOURCE_DIR}/libs/indibase/indiusbdevice.h
 DESTINATION ${INCLUDE_INSTALL_DIR}/libindi COMPONENT Devel)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/libindi.pc.cmake ${CMAKE_CURRENT_BINARY_DIR}/libindi.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libindi.pc DESTINATION ${PKGCONFIG_INSTALL_PREFIX})
