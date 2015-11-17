cmake_minimum_required(VERSION 2.6)
PROJECT(libindi C CXX)

cmake_policy(SET CMP0003 NEW)
#FIXME the following requires CMake v3.0
#cmake_policy(SET CMP0042 OLD)
set(CMAKE_CXX_FLAGS "-std=c++0x ${CMAKE_CXX_FLAGS}")
##################  INDI version  ################################
set(INDI_SOVERSION "1")
set(CMAKE_INDI_VERSION_MAJOR 1)
set(CMAKE_INDI_VERSION_MINOR 1)
set(CMAKE_INDI_VERSION_RELEASE 0)
set(CMAKE_INDI_VERSION_STRING "${CMAKE_INDI_VERSION_MAJOR}.${CMAKE_INDI_VERSION_MINOR}.${CMAKE_INDI_VERSION_RELEASE}")

##################  Paths  ################################
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake_modules/")
set(DATA_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/share/indi/")
set(BIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/bin")
set(INCLUDE_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/include")

IF(APPLE)
set(CMAKE_SHARED_LINKER_FLAGS "-undefined dynamic_lookup")
ENDIF(APPLE)

##################  setup install directories  ################################
set (LIB_SUFFIX "" CACHE STRING "Define suffix of directory name (32/64)" )
set (LIB_DESTINATION "${CMAKE_INSTALL_PREFIX}/lib${LIB_SUFFIX}")
## the following are directories where stuff will be installed to
set(INCLUDE_INSTALL_DIR      "${CMAKE_INSTALL_PREFIX}/include/")
set(PKGCONFIG_INSTALL_PREFIX "${LIB_DESTINATION}/pkgconfig/")
set(UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")

##################  Includes  ################################
Include (CheckCXXSourceCompiles)
include (MacroOptionalFindPackage)
include (MacroLogFeature)
include (MacroBoolTo01)
include (CheckIncludeFiles)

FIND_LIBRARY(M_LIB m)
FIND_PACKAGE(ZLIB REQUIRED)
FIND_PACKAGE(USB-1 REQUIRED)
find_package(CURL REQUIRED)
FIND_PACKAGE(CFITSIO REQUIRED)
FIND_PACKAGE(Nova REQUIRED)
FIND_PACKAGE(Threads REQUIRED)
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  FIND_PACKAGE(JPEG REQUIRED)
endif ()

if (NOT CFITSIO_FOUND OR CFITSIO_VERSION_MAJOR LESS 3)
  message(FATAL_ERROR "CFITSIO version too old, Please install cfitsio 3.x and try again. http://heasarc.gsfc.nasa.gov/fitsio/fitsio.html")
endif (NOT CFITSIO_FOUND OR CFITSIO_VERSION_MAJOR LESS 3)

macro_bool_to_01(CFITSIO_FOUND HAVE_CFITSIO_H)
macro_log_feature(CFITSIO_FOUND "libcfitsio" "A library for reading and writing data files in FITS (Flexible Image Transport System) data format" "http://heasarc.gsfc.nasa.gov/fitsio/fitsio.html" FALSE "3.03" "Provides INDI with FITS I/O support.")

macro_bool_to_01(NOVA_FOUND HAVE_NOVA_H)
macro_log_feature(NOVA_FOUND "libnova" "A general purpose, double precision, Celestial Mechanics, Astrometry and Astrodynamics library" "http://libnova.sourceforge.net" FALSE "0.12.1" "Provides INDI with astrodynamics library.")

check_include_files(linux/videodev2.h HAVE_LINUX_VIDEODEV2_H)
check_include_files(termios.h TERMIOS_FOUND)
macro_bool_to_01(TERMIOS_FOUND HAVE_TERMIOS_H)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_SOURCE_DIR})
include_directories( ${CMAKE_SOURCE_DIR}/libs)
include_directories( ${CMAKE_SOURCE_DIR}/libs/indibase)

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
include_directories( ${CMAKE_SOURCE_DIR}/libs/webcam)
  include_directories( ${JPEG_INCLUDE_DIR} )
endif()

if (CFITSIO_FOUND)
  include_directories(${CFITSIO_INCLUDE_DIR})
endif (CFITSIO_FOUND)

include_directories(${NOVA_INCLUDE_DIR})

set(liblilxml_SRCS  ${CMAKE_SOURCE_DIR}/libs/lilxml.c )

set(libindicom_SRCS
	${CMAKE_SOURCE_DIR}/libs/indicom.c
	${CMAKE_SOURCE_DIR}/base64.c
	)

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
set(libwebcam_SRCS
	${CMAKE_SOURCE_DIR}/libs/webcam/v4l2_base.cpp
	${CMAKE_SOURCE_DIR}/libs/webcam/v4l2_colorspace.c
	${CMAKE_SOURCE_DIR}/libs/webcam/ccvt_c2.c
	${CMAKE_SOURCE_DIR}/libs/webcam/ccvt_misc.c
        ${CMAKE_SOURCE_DIR}/libs/webcam/jpegutils.c
	${CMAKE_SOURCE_DIR}/libs/webcam/v4l2_decode/v4l2_decode.cpp
	${CMAKE_SOURCE_DIR}/libs/webcam/v4l2_decode/v4l2_builtin_decoder.cpp
	${CMAKE_SOURCE_DIR}/libs/webcam/v4l2_record/v4l2_record.cpp
	${CMAKE_SOURCE_DIR}/libs/webcam/v4l2_record/ser_recorder.cpp
        ${CMAKE_SOURCE_DIR}/libs/webcam/v4l2_record/stream_recorder.cpp
	)
endif()

set (indimain_SRCS
        ${CMAKE_SOURCE_DIR}/indidriver.c
	${CMAKE_SOURCE_DIR}/indidrivermain.c
	${CMAKE_SOURCE_DIR}/eventloop.c
    )

set (indiclient_SRCS
        ${CMAKE_SOURCE_DIR}/libs/indibase/basedevice.cpp
        ${CMAKE_SOURCE_DIR}/libs/indibase/baseclient.cpp
        ${CMAKE_SOURCE_DIR}/libs/indibase/indiproperty.cpp
    )

set (indidriver_SRCS
        ${CMAKE_SOURCE_DIR}/libs/indibase/basedevice.cpp
        ${CMAKE_SOURCE_DIR}/libs/indibase/defaultdevice.cpp
        ${CMAKE_SOURCE_DIR}/libs/indibase/indiproperty.cpp
        ${CMAKE_SOURCE_DIR}/libs/indibase/indiccd.cpp
        ${CMAKE_SOURCE_DIR}/libs/indibase/inditelescope.cpp
        ${CMAKE_SOURCE_DIR}/libs/indibase/indifilterwheel.cpp
        ${CMAKE_SOURCE_DIR}/libs/indibase/indifocuserinterface.cpp
        ${CMAKE_SOURCE_DIR}/libs/indibase/indifocuser.cpp
        ${CMAKE_SOURCE_DIR}/libs/indibase/indiusbdevice.cpp
        ${CMAKE_SOURCE_DIR}/libs/indibase/indiguiderinterface.cpp
        ${CMAKE_SOURCE_DIR}/libs/indibase/indifilterinterface.cpp
        ${CMAKE_SOURCE_DIR}/libs/indibase/indidome.cpp
        ${CMAKE_SOURCE_DIR}/libs/indibase/indigps.cpp
        ${CMAKE_SOURCE_DIR}/libs/indibase/indiweather.cpp
        ${CMAKE_SOURCE_DIR}/libs/indibase/indilogger.cpp
        ${CMAKE_SOURCE_DIR}/libs/indibase/indicontroller.cpp
    )

set (lx_SRCS
     ${CMAKE_SOURCE_DIR}/libs/lx/Lx.cpp
)

IF (APPLE)
set(hidapi_SRCS ${CMAKE_SOURCE_DIR}/libs/indibase/hid_mac.c)
ELSEIF (WIN32)
set(hidapi_SRCS ${CMAKE_SOURCE_DIR}/libs/indibase/hid_win.c)
ELSE ()
set(hidapi_SRCS ${CMAKE_SOURCE_DIR}/libs/indibase/hid_libusb.c)
ENDIF()

######################################
########### INDI SERVER ##############
######################################
set(indiserver_SRCS indiserver.c fq.c)

add_executable(indiserver ${indiserver_SRCS} ${liblilxml_SRCS})

target_link_libraries(indiserver ${CMAKE_THREAD_LIBS_INIT})

install(TARGETS indiserver RUNTIME DESTINATION bin)

#################################################
############# INDI Shared Library ###############
# To offer lilxml and communination routines    #
# Mostly used by generic clients                #
#################################################
add_library(indi SHARED ${libindicom_SRCS} ${liblilxml_SRCS})
target_link_libraries(indi ${NOVA_LIBRARIES} ${M_LIB} ${ZLIB_LIBRARY} ${CFITSIO_LIBRARIES})

install(TARGETS indi LIBRARY DESTINATION ${LIB_DESTINATION})
set_target_properties(indi PROPERTIES VERSION ${CMAKE_INDI_VERSION_STRING} SOVERSION ${INDI_SOVERSION})

##################################################
########## INDI Default Driver Library ###########
# To link with main() and indibase classes  ######
##################################################
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
add_library(indidriver SHARED ${libindicom_SRCS} ${liblilxml_SRCS} ${indimain_SRCS} ${indidriver_SRCS} ${libwebcam_SRCS} ${hidapi_SRCS})
target_link_libraries(indidriver ${LIBUSB_1_LIBRARIES} ${NOVA_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${CFITSIO_LIBRARIES} ${M_LIB} ${ZLIB_LIBRARY} ${JPEG_LIBRARY})
add_library(indidriverstatic STATIC ${libindicom_SRCS} ${liblilxml_SRCS} ${indimain_SRCS} ${indidriver_SRCS} ${libwebcam_SRCS})
target_link_libraries(indidriverstatic ${LIBUSB_1_LIBRARIES} ${NOVA_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${CFITSIO_LIBRARIES} ${M_LIB} ${ZLIB_LIBRARY} ${JPEG_LIBRARY})
else()
add_library(indidriver SHARED ${libindicom_SRCS} ${liblilxml_SRCS} ${indimain_SRCS} ${indidriver_SRCS} ${hidapi_SRCS})
target_link_libraries(indidriver ${LIBUSB_1_LIBRARIES} ${NOVA_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${CFITSIO_LIBRARIES} ${M_LIB} ${ZLIB_LIBRARY})
add_library(indidriverstatic STATIC ${libindicom_SRCS} ${liblilxml_SRCS} ${indimain_SRCS} ${indidriver_SRCS})
target_link_libraries(indidriverstatic ${LIBUSB_1_LIBRARIES} ${NOVA_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${CFITSIO_LIBRARIES} ${M_LIB} ${ZLIB_LIBRARY})
endif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
set_target_properties(indidriver indidriverstatic PROPERTIES VERSION ${CMAKE_INDI_VERSION_STRING} SOVERSION ${INDI_SOVERSION} OUTPUT_NAME indidriver)
install(TARGETS indidriver LIBRARY DESTINATION ${LIB_DESTINATION})
install(TARGETS indidriverstatic ARCHIVE DESTINATION ${LIB_DESTINATION})

##################################################
########### INDI Client Static Library ###########
##################################################
add_library(indiclient STATIC ${indiclient_SRCS})
#-----------------------------------------------------------------------------
#
# To fix compilation problem: relocation R_X86_64_32 against `a local symbol' can not be
# used when making a shared object; recompile with -fPIC
# See http://www.cmake.org/pipermail/cmake/2007-May/014350.html
#
IF( CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64" )
  SET_TARGET_PROPERTIES(indiclient PROPERTIES COMPILE_FLAGS "-fPIC")
ENDIF( CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64" )

target_link_libraries(indiclient indi ${CMAKE_THREAD_LIBS_INIT})
install(TARGETS indiclient ARCHIVE DESTINATION ${LIB_DESTINATION})

##################################################
########### INDI Alignment Subsystem #############
##################################################
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
add_subdirectory(${CMAKE_SOURCE_DIR}/libs/indibase/alignment)
endif()

#####################################
######## AGENT GROUP #########
#####################################

########### Imager ##############
set(imager_SRCS
        ${CMAKE_SOURCE_DIR}/drivers/agent/agent_imager.cpp
   )

add_executable(indi_imager_agent ${imager_SRCS})
target_link_libraries(indi_imager_agent indidriver indiclient)
install(TARGETS indi_imager_agent RUNTIME DESTINATION bin )

#################################################################################

#####################################
########## TELESCOPE GROUP ##########
#####################################

########### LX200 Basic #############
set(lx200basic_SRCS
   ${indimain_SRCS}
   ${CMAKE_SOURCE_DIR}/drivers/telescope/lx200driver.cpp
   ${CMAKE_SOURCE_DIR}/drivers/telescope/lx200basic.cpp )

add_executable(indi_lx200basic ${lx200basic_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_lx200basic indidriver)

install(TARGETS indi_lx200basic RUNTIME DESTINATION bin )

#################################################################################

########### LX200 Generic ###########
set(lx200generic_SRCS
   ${CMAKE_SOURCE_DIR}/drivers/telescope/lx200driver.cpp
   ${CMAKE_SOURCE_DIR}/drivers/telescope/lx200autostar.cpp
   ${CMAKE_SOURCE_DIR}/drivers/telescope/lx200_16.cpp
   ${CMAKE_SOURCE_DIR}/drivers/telescope/lx200gps.cpp
   ${CMAKE_SOURCE_DIR}/drivers/telescope/lx200generic.cpp
   ${CMAKE_SOURCE_DIR}/drivers/telescope/lx200classic.cpp
   ${CMAKE_SOURCE_DIR}/drivers/telescope/lx200apdriver.cpp
   ${CMAKE_SOURCE_DIR}/drivers/telescope/lx200gemini.cpp
   ${CMAKE_SOURCE_DIR}/drivers/telescope/lx200zeq25.cpp
   ${CMAKE_SOURCE_DIR}/drivers/telescope/lx200ap.cpp
   ${CMAKE_SOURCE_DIR}/drivers/telescope/lx200fs2.cpp

)

add_executable(indi_lx200generic ${lx200generic_SRCS})

target_link_libraries(indi_lx200generic indidriver)

install(TARGETS indi_lx200generic RUNTIME DESTINATION bin )

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/make_lx200generic_symlink.cmake
"exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink ${BIN_INSTALL_DIR}/indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200classic)\n
exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink ${BIN_INSTALL_DIR}/indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200autostar)\n
exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink ${BIN_INSTALL_DIR}/indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200_16)\n
exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink ${BIN_INSTALL_DIR}/indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200gps)\n
exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink ${BIN_INSTALL_DIR}/indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200ap)\n
exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink ${BIN_INSTALL_DIR}/indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200gemini)\n
exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink ${BIN_INSTALL_DIR}/indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200zeq25)\n
exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink ${BIN_INSTALL_DIR}/indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200fs2)\n
")
set_target_properties(indi_lx200generic PROPERTIES POST_INSTALL_SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/make_lx200generic_symlink.cmake)
#################################################################################

########### Celestron GPS ############
set(celestrongps_SRCS
   ${CMAKE_SOURCE_DIR}/drivers/telescope/celestrondriver.cpp
   ${CMAKE_SOURCE_DIR}/drivers/telescope/celestrongps.cpp )

add_executable(indi_celestron_gps ${celestrongps_SRCS})

target_link_libraries(indi_celestron_gps indidriver)

install(TARGETS indi_celestron_gps RUNTIME DESTINATION bin )

#################################################################################

########### Takahashi Temma ##########
set(temma_SRCS
   ${indimain_SRCS}
   ${CMAKE_SOURCE_DIR}/drivers/telescope/temmadriver.c )

add_executable(indi_temma ${temma_SRCS}  ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_temma  ${NOVA_LIBRARIES} ${M_LIB} )

install(TARGETS indi_temma RUNTIME DESTINATION bin )
#################################################################################

########### Sky Commander #############
set(skycommander_SRCS
   ${indimain_SRCS}
   ${CMAKE_SOURCE_DIR}/drivers/telescope/skycommander.c )

add_executable(indi_skycommander ${skycommander_SRCS}  ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_skycommander  ${NOVA_LIBRARIES} ${M_LIB} )

install(TARGETS indi_skycommander  RUNTIME DESTINATION bin )

#################################################################################

########### Intelliscope ###############
set(intelliscope_SRCS
   ${indimain_SRCS}
   ${CMAKE_SOURCE_DIR}/drivers/telescope/intelliscope.c )

add_executable(indi_intelliscope ${intelliscope_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_intelliscope ${NOVA_LIBRARIES} ${M_LIB} )

install(TARGETS indi_intelliscope RUNTIME DESTINATION bin )

########### Syncscan ###############
set(synscan_SRCS
   ${CMAKE_SOURCE_DIR}/drivers/telescope/synscanmount.cpp )

add_executable(indi_synscan ${synscan_SRCS})
target_link_libraries(indi_synscan indidriver)

install(TARGETS indi_synscan RUNTIME DESTINATION bin )

########### Magellan I #############
set(magellan_SRCS
   ${indimain_SRCS}
   ${CMAKE_SOURCE_DIR}/drivers/telescope/magellandriver.c
   ${CMAKE_SOURCE_DIR}/drivers/telescope/magellan1.cpp )

add_executable(indi_magellan1 ${magellan_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_magellan1 ${NOVA_LIBRARIES})

install(TARGETS indi_magellan1 RUNTIME DESTINATION bin )

########### IEQ Pro / CEM60 #############
set(ieq_SRCS
   ${indimain_SRCS}
   ${CMAKE_SOURCE_DIR}/drivers/telescope/ieqprodriver.cpp
   ${CMAKE_SOURCE_DIR}/drivers/telescope/ieqpro.cpp )

add_executable(indi_ieq_telescope ${ieq_SRCS})

target_link_libraries(indi_ieq_telescope indidriver)

install(TARGETS indi_ieq_telescope RUNTIME DESTINATION bin )

########### Telescope Simulator ##############
set(telescopesimulator_SRCS
        ${CMAKE_SOURCE_DIR}/drivers/telescope/telescope_simulator.cpp
   )

add_executable(indi_simulator_telescope ${telescopesimulator_SRCS})
target_link_libraries(indi_simulator_telescope indidriver)
install(TARGETS indi_simulator_telescope RUNTIME DESTINATION bin )

########### CCD Simulator ##############
if (CFITSIO_FOUND)

set(ccdsimulator_SRCS
        ${CMAKE_SOURCE_DIR}/drivers/ccd/ccd_simulator.cpp
   )

add_executable(indi_simulator_ccd ${ccdsimulator_SRCS})
target_link_libraries(indi_simulator_ccd indidriver)
install(TARGETS indi_simulator_ccd RUNTIME DESTINATION bin )

endif (CFITSIO_FOUND)


#####################################
########## FOCUSER GROUP ############
#####################################

#################################################################################

################ Focuser Simulator ################

set(focussimulator_SRCS
        ${CMAKE_SOURCE_DIR}/drivers/focuser/focus_simulator.cpp
   )

add_executable(indi_simulator_focus ${focussimulator_SRCS})
target_link_libraries(indi_simulator_focus indidriver)
install(TARGETS indi_simulator_focus RUNTIME DESTINATION bin )

################ Robo Focuser ################

set(robofocus_SRCS
        ${CMAKE_SOURCE_DIR}/drivers/focuser/robofocus.cpp
   )

add_executable(indi_robo_focus ${robofocus_SRCS})
target_link_libraries(indi_robo_focus indidriver)
install(TARGETS indi_robo_focus RUNTIME DESTINATION bin )


################ Rigelsys NFocus Focuser ################

set(nfocus_SRCS
        ${CMAKE_SOURCE_DIR}/drivers/focuser/nfocus.cpp
   )

add_executable(indi_nfocus ${nfocus_SRCS})
target_link_libraries(indi_nfocus indidriver)
install(TARGETS indi_nfocus RUNTIME DESTINATION bin )


################ Moonlite Focuser ################

set(moonlite_SRCS
        ${CMAKE_SOURCE_DIR}/drivers/focuser/moonlite.cpp
   )

add_executable(indi_moonlite_focus ${moonlite_SRCS})
target_link_libraries(indi_moonlite_focus indidriver)
install(TARGETS indi_moonlite_focus RUNTIME DESTINATION bin )

################ Baader SteelDrive Focuser ################

set(steeldrive_SRCS
        ${CMAKE_SOURCE_DIR}/drivers/focuser/steeldrive.cpp
   )

add_executable(indi_steeldrive_focus ${steeldrive_SRCS})
target_link_libraries(indi_steeldrive_focus indidriver)
install(TARGETS indi_steeldrive_focus RUNTIME DESTINATION bin )

################ FocusLynx Focuser ################

set(focuslynx_SRCS
        ${CMAKE_SOURCE_DIR}/drivers/focuser/focuslynxbase.cpp
        ${CMAKE_SOURCE_DIR}/drivers/focuser/focuslynx.cpp
   )

add_executable(indi_lynx_focus ${focuslynx_SRCS})
target_link_libraries(indi_lynx_focus indidriver)
install(TARGETS indi_lynx_focus RUNTIME DESTINATION bin )

################ PerfectStar Focuser ################

set(perfectstar_SRCS
        ${CMAKE_SOURCE_DIR}/drivers/focuser/perfectstar.cpp
   )

add_executable(indi_perfectstar_focus ${perfectstar_SRCS})
target_link_libraries(indi_perfectstar_focus indidriver)
install(TARGETS indi_perfectstar_focus RUNTIME DESTINATION bin )
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
install(FILES ${CMAKE_SOURCE_DIR}/drivers/focuser/99-perfectstar.rules DESTINATION ${UDEVRULES_INSTALL_DIR})
endif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")

################ Optec TCF-S ################

set(tcfs_SRCS
        ${CMAKE_SOURCE_DIR}/drivers/focuser/tcfs.cpp
   )

add_executable(indi_tcfs_focus ${tcfs_SRCS})

target_link_libraries(indi_tcfs_focus indidriver)
install(TARGETS indi_tcfs_focus RUNTIME DESTINATION bin )

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/make_tcfs_symlink.cmake
"exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink ${BIN_INSTALL_DIR}/indi_tcfs_focus \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_tcfs3_focus)\n")
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

target_link_libraries(indi_trutech_wheel ${NOVA_LIBRARIES} ${M_LIB} ${ZLIB_LIBRARY})

install(TARGETS indi_trutech_wheel RUNTIME DESTINATION bin )

########### Filter Simulator ##############
set(xagylwheel_SRCS
        ${CMAKE_SOURCE_DIR}/drivers/filter_wheel/xagyl_wheel.cpp
   )

add_executable(indi_xagyl_wheel ${xagylwheel_SRCS})
target_link_libraries(indi_xagyl_wheel indidriver)
install(TARGETS indi_xagyl_wheel RUNTIME DESTINATION bin )

########### Filter Simulator ##############
set(filtersimulator_SRCS
        ${CMAKE_SOURCE_DIR}/drivers/filter_wheel/filter_simulator.cpp
   )

add_executable(indi_simulator_wheel ${filtersimulator_SRCS})
target_link_libraries(indi_simulator_wheel indidriver)
install(TARGETS indi_simulator_wheel RUNTIME DESTINATION bin )

#################################################################################

#####################################
########## DOME GROUP ############
#####################################

################ Dome Simulator ################

set(domesimulator_SRCS
        ${CMAKE_SOURCE_DIR}/drivers/dome/dome_simulator.cpp
   )

add_executable(indi_simulator_dome ${domesimulator_SRCS})
target_link_libraries(indi_simulator_dome indidriver)
install(TARGETS indi_simulator_dome RUNTIME DESTINATION bin )

################ Roll Off ################

set(rolloff_SRCS
        ${CMAKE_SOURCE_DIR}/drivers/dome/roll_off.cpp
   )

add_executable(indi_rolloff_dome ${rolloff_SRCS})
target_link_libraries(indi_rolloff_dome indidriver)
install(TARGETS indi_rolloff_dome RUNTIME DESTINATION bin )


################ Baader Dome ################

set(baaderdome_SRCS
        ${CMAKE_SOURCE_DIR}/drivers/dome/baader_dome.cpp
   )

add_executable(indi_baader_dome ${baaderdome_SRCS})
target_link_libraries(indi_baader_dome indidriver)
install(TARGETS indi_baader_dome RUNTIME DESTINATION bin )
#################################################################################

#########################################
########### VIDEO GROUP   ###############
#########################################

########### STV #######################
if (CFITSIO_FOUND)

set(stv_SRCS
   ${indimain_SRCS}
   ${CMAKE_SOURCE_DIR}/drivers/video/stvdriver.c
   ${CMAKE_SOURCE_DIR}/drivers/video/stv.c )

add_executable(indi_sbig_stv ${stv_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_sbig_stv ${NOVA_LIBRARIES} ${CFITSIO_LIBRARIES} ${M_LIB} ${ZLIB_LIBRARY} ${CMAKE_THREAD_LIBS_INIT})

install(TARGETS indi_sbig_stv RUNTIME DESTINATION bin )

endif(CFITSIO_FOUND)

#################################################################################

########### INDI::CCD V4L Driver ###############
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
if (CFITSIO_FOUND)

set(v4l2driverccd_SRCS
        ${lx_SRCS}
        ${CMAKE_SOURCE_DIR}/drivers/video/v4l2driver.cpp
        ${CMAKE_SOURCE_DIR}/drivers/video/indi_v4l2driver.cpp)

add_executable(indi_v4l2_ccd ${v4l2driverccd_SRCS} ${libwebcam_SRCS} ${lx_SRCS})

target_link_libraries(indi_v4l2_ccd ${JPEG_LIBRARY} indidriver)

install(TARGETS indi_v4l2_ccd RUNTIME DESTINATION bin )


endif (CFITSIO_FOUND)
endif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")

#################################################################################

#####################################
############ AUX GROUP ##############
#####################################

########### Watch dog ###############

set(watchdog_SRCS
        ${CMAKE_SOURCE_DIR}/drivers/auxiliary/watchdog.cpp
        ${CMAKE_SOURCE_DIR}/drivers/auxiliary/watchdogclient.cpp
   )

add_executable(indi_watchdog ${watchdog_SRCS})

target_link_libraries(indi_watchdog indidriver indiclient)
install(TARGETS indi_watchdog RUNTIME DESTINATION bin )

########### Flip Flat & Flip Man Driver ###############

set(flipflat_SRCS
        ${CMAKE_SOURCE_DIR}/drivers/auxiliary/flip_flat.cpp
   )

add_executable(indi_flipflat ${flipflat_SRCS})

target_link_libraries(indi_flipflat indidriver)
install(TARGETS indi_flipflat RUNTIME DESTINATION bin )

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
install(FILES ${CMAKE_SOURCE_DIR}/drivers/auxiliary/99-flipflat.rules DESTINATION ${UDEVRULES_INSTALL_DIR})
endif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")


########### STAR2000 Driver ###############

set(STAR2000_SRCS
        ${CMAKE_SOURCE_DIR}/drivers/auxiliary/STAR2kdriver.c
        ${CMAKE_SOURCE_DIR}/drivers/auxiliary/STAR2000.cpp
   )

add_executable(indi_star2000 ${STAR2000_SRCS})

target_link_libraries(indi_star2000 indidriver)
install(TARGETS indi_star2000 RUNTIME DESTINATION bin )

########### GPUSB Driver ###############

set(gpusb_SRCS
        ${CMAKE_SOURCE_DIR}/drivers/auxiliary/gpdriver.cpp
        ${CMAKE_SOURCE_DIR}/drivers/auxiliary/gpusb.cpp
   )

add_executable(indi_gpusb ${gpusb_SRCS})

target_link_libraries(indi_gpusb indidriver)
install(TARGETS indi_gpusb RUNTIME DESTINATION bin )
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
install(FILES ${CMAKE_SOURCE_DIR}/drivers/auxiliary/99-gpusb.rules DESTINATION ${UDEVRULES_INSTALL_DIR})
endif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")

########### Joystick Driver ###############

set(joystick_SRCS
        ${CMAKE_SOURCE_DIR}/drivers/auxiliary/joystickdriver.cpp
        ${CMAKE_SOURCE_DIR}/drivers/auxiliary/joystick.cpp
   )

add_executable(indi_joystick ${joystick_SRCS})

target_link_libraries(indi_joystick indidriver)
install(TARGETS indi_joystick RUNTIME DESTINATION bin )


endif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")

########### GPS Simulator Driver ###############

set(gpssimulator_SRCS
        ${CMAKE_SOURCE_DIR}/drivers/auxiliary/gps_simulator.cpp
   )

add_executable(indi_simulator_gps ${gpssimulator_SRCS})

target_link_libraries(indi_simulator_gps indidriver)
install(TARGETS indi_simulator_gps RUNTIME DESTINATION bin )

#####################################
############ AUX GROUP ##############
#####################################

########### Vantage Driver ###############

set(vantage_SRCS
        ${CMAKE_SOURCE_DIR}/drivers/weather/vantage.cpp
   )

add_executable(indi_vantage_weather ${vantage_SRCS})

target_link_libraries(indi_vantage_weather indidriver)
install(TARGETS indi_vantage_weather RUNTIME DESTINATION bin )
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
install(FILES ${CMAKE_SOURCE_DIR}/drivers/weather/99-vantage.rules DESTINATION ${UDEVRULES_INSTALL_DIR})
endif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")

########### WunderGround Driver ###############

set(WunderGround_SRCS
        ${CMAKE_SOURCE_DIR}/drivers/weather/gason.cpp
        ${CMAKE_SOURCE_DIR}/drivers/weather/wunderground.cpp
   )

add_executable(indi_wunderground_weather ${WunderGround_SRCS})

target_link_libraries(indi_wunderground_weather indidriver ${CURL_LIBRARIES})
install(TARGETS indi_wunderground_weather RUNTIME DESTINATION bin )

#####################################
############ INDI TOOLS #############
#####################################


########### getINDI ##############
set(getindi_SRCS
	${CMAKE_SOURCE_DIR}/eventloop.c
	${CMAKE_SOURCE_DIR}/tools/getINDIproperty.c
   )

add_executable(indi_getprop ${getindi_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_getprop ${NOVA_LIBRARIES} ${M_LIB} ${ZLIB_LIBRARY})

install(TARGETS indi_getprop RUNTIME DESTINATION bin )

#################################################################################

########### setINDI ##############
set(setindi_SRCS
	${CMAKE_SOURCE_DIR}/eventloop.c
	${CMAKE_SOURCE_DIR}/tools/setINDIproperty.c
   )

add_executable(indi_setprop ${setindi_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_setprop ${NOVA_LIBRARIES} ${M_LIB} ${ZLIB_LIBRARY})

install(TARGETS indi_setprop RUNTIME DESTINATION bin )

#################################################################################

########### evelINDI ##############
set(evalindi_SRCS
	${CMAKE_SOURCE_DIR}/eventloop.c
	${CMAKE_SOURCE_DIR}/tools/compiler.c
	${CMAKE_SOURCE_DIR}/tools/evalINDI.c
   )

add_executable(indi_eval ${evalindi_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_eval ${NOVA_LIBRARIES} ${M_LIB} ${ZLIB_LIBRARY})

install(TARGETS indi_eval RUNTIME DESTINATION bin )

#################################################################################
## Build Examples. Not installation

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
add_subdirectory(${CMAKE_SOURCE_DIR}/examples)
endif()

#################################################################################

install( FILES drivers.xml ${CMAKE_SOURCE_DIR}/drivers/focuser/indi_tcfs_sk.xml DESTINATION ${DATA_INSTALL_DIR})

install( FILES indiapi.h indidevapi.h base64.h eventloop.h indidriver.h ${CMAKE_SOURCE_DIR}/libs/lilxml.h ${CMAKE_SOURCE_DIR}/libs/indibase/indibase.h
${CMAKE_SOURCE_DIR}/libs/indibase/indibasetypes.h ${CMAKE_SOURCE_DIR}/libs/indibase/basedevice.h  ${CMAKE_SOURCE_DIR}/libs/indibase/defaultdevice.h
${CMAKE_SOURCE_DIR}/libs/indibase/indiccd.h  ${CMAKE_SOURCE_DIR}/libs/indibase/indifilterwheel.h
${CMAKE_SOURCE_DIR}/libs/indibase/indifocuserinterface.h  ${CMAKE_SOURCE_DIR}/libs/indibase/indifocuser.h
${CMAKE_SOURCE_DIR}/libs/indibase/inditelescope.h ${CMAKE_SOURCE_DIR}/libs/indibase/baseclient.h ${CMAKE_SOURCE_DIR}/libs/indibase/indiguiderinterface.h
${CMAKE_SOURCE_DIR}/libs/indibase/indifilterinterface.h ${CMAKE_SOURCE_DIR}/libs/indibase/indiproperty.h
${CMAKE_SOURCE_DIR}/libs/indibase/indidome.h ${CMAKE_SOURCE_DIR}/libs/indibase/indigps.h
${CMAKE_SOURCE_DIR}/libs/indibase/indiweather.h ${CMAKE_SOURCE_DIR}/libs/indicom.h ${CMAKE_SOURCE_DIR}/libs/indibase/indiusbdevice.h
${CMAKE_SOURCE_DIR}/libs/indibase/indilogger.h ${CMAKE_SOURCE_DIR}/libs/indibase/indicontroller.h ${CMAKE_SOURCE_DIR}/libs/indibase/hidapi.h
${CMAKE_SOURCE_DIR}/libs/webcam/ccvt.h ${CMAKE_SOURCE_DIR}/libs/webcam/ccvt_types.h
${CMAKE_SOURCE_DIR}/libs/webcam/v4l2_record/v4l2_record.h ${CMAKE_SOURCE_DIR}/libs/webcam/v4l2_record/ser_recorder.h
${CMAKE_SOURCE_DIR}/libs/webcam/v4l2_decode/v4l2_decode.h ${CMAKE_SOURCE_DIR}/libs/webcam/v4l2_decode/v4l2_builtin_decoder.h
${CMAKE_SOURCE_DIR}/libs/webcam/v4l2_record/stream_recorder.h ${CMAKE_SOURCE_DIR}/libs/webcam/ccvt_types.h ${CMAKE_SOURCE_DIR}/libs/webcam/v4l2_colorspace.h
${CMAKE_SOURCE_DIR}/libs/webcam/jpegutils.h
 DESTINATION ${INCLUDE_INSTALL_DIR}/libindi COMPONENT Devel)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/libindi.pc.cmake ${CMAKE_CURRENT_BINARY_DIR}/libindi.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libindi.pc DESTINATION ${PKGCONFIG_INSTALL_PREFIX})
