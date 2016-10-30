cmake_minimum_required(VERSION 2.8)
PROJECT(libindi C CXX)

# Tell CMake to run moc when necessary:
set(CMAKE_AUTOMOC ON)
# As moc files are generated in the binary dir, tell CMake
# to always look for includes there:
set(CMAKE_INCLUDE_CURRENT_DIR ON)

 cmake_policy(SET CMP0003 NEW)
#FIXME the following requires CMake v3.0
#cmake_policy(SET CMP0042 OLD)
include(GNUInstallDirs)
include(FeatureSummary)

if(ANDROID OR "${CMAKE_SYSTEM_NAME}" STREQUAL "Android")
  set(ANDROID ON)
  add_definitions(-DANDROID)
endif()

if (NOT WIN32 AND NOT ANDROID)
set(CMAKE_CXX_FLAGS "-std=c++0x ${CMAKE_CXX_FLAGS}")
endif(NOT WIN32 AND NOT ANDROID)

#####################################  INDI version  ################################################

set(INDI_SOVERSION "1")
set(CMAKE_INDI_VERSION_MAJOR 1)
set(CMAKE_INDI_VERSION_MINOR 3)
set(CMAKE_INDI_VERSION_RELEASE 0)
set(CMAKE_INDI_VERSION_STRING "${CMAKE_INDI_VERSION_MAJOR}.${CMAKE_INDI_VERSION_MINOR}.${CMAKE_INDI_VERSION_RELEASE}")

########################################  Paths  ###################################################

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
set(DATA_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/share/indi/")
set(BIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/bin")
set(INCLUDE_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/include")

IF(APPLE)
set(CMAKE_SHARED_LINKER_FLAGS "-undefined dynamic_lookup")
ENDIF(APPLE)

##################################  Install Directories  ###########################################
set (LIB_DESTINATION "${CMAKE_INSTALL_LIBDIR}")

## the following are directories where stuff will be installed to
set(INCLUDE_INSTALL_DIR      "${CMAKE_INSTALL_PREFIX}/include/")
set(PKGCONFIG_INSTALL_PREFIX "${LIB_DESTINATION}/pkgconfig/")
set(UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")

#####################################  Build Options  ##############################################

OPTION (INDI_BUILD_UNITTESTS "Build INDI tests" OFF)
OPTION (INDI_FAST_BLOB "Build INDI with Fast BLOB support" ON)
OPTION (INDI_CALCULATE_MINMAX "Calculate and store image minimum and maximum values in FITS header" OFF)

#####################################  Dependencies  ##############################################
# ZLib compression Library
FIND_PACKAGE(ZLIB REQUIRED)

# libcfitsio for FITS IO
FIND_PACKAGE(CFITSIO REQUIRED)

if (NOT CFITSIO_FOUND OR CFITSIO_VERSION_MAJOR LESS 3)
  message(FATAL_ERROR "CFITSIO version too old, Please install cfitsio 3.x and try again. http://heasarc.gsfc.nasa.gov/fitsio/fitsio.html")
endif (NOT CFITSIO_FOUND OR CFITSIO_VERSION_MAJOR LESS 3)

set_package_properties(CFITSIO PROPERTIES DESCRIPTION "A library for reading and writing data files in FITS (Flexible Image Transport System) data format" URL "http://heasarc.gsfc.nasa.gov/fitsio/fitsio.html" TYPE REQUIRED PURPOSE "Provides INDI with FITS I/O support.")

# libnova for astronomical formulae
FIND_PACKAGE(Nova REQUIRED)

set_package_properties(Nova PROPERTIES DESCRIPTION "A general purpose, double precision, Celestial Mechanics, Astrometry and Astrodynamics library" URL "http://libnova.sourceforge.net" TYPE REQUIRED PURPOSE "Provides INDI with astrodynamics library.")

# To build INDI Client Qt5 backend
FIND_PACKAGE(Qt5Network)

if (WIN32)
set(M_LIB "")
else(WIN32)

# Math Library
FIND_LIBRARY(M_LIB m)

if(NOT ANDROID)
# libusb 1.0+ for USB devices
FIND_PACKAGE(USB-1 REQUIRED)

# libcurl to make web requests
FIND_PACKAGE(CURL REQUIRED)

# Phreads for multithreading
FIND_PACKAGE(Threads REQUIRED)

# libjpeg for V4L2
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  FIND_PACKAGE(JPEG REQUIRED)
endif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")

endif(NOT ANDROID)
endif(WIN32)

# Generate config.h from template
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )

##################################  Include Directories  ##########################################

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/libs)
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase)

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/libs/webcam)
  include_directories( ${JPEG_INCLUDE_DIR} )
endif()

include_directories(${CFITSIO_INCLUDE_DIR})
include_directories(${LIBUSB_1_INCLUDE_DIRS})
include_directories(${NOVA_INCLUDE_DIR})

###################################################################################################
########################################  Sources  ################################################
###################################################################################################

set(liblilxml_SRCS  ${CMAKE_CURRENT_SOURCE_DIR}/libs/lilxml.c )

set(libindicom_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indicom.c
        ${CMAKE_CURRENT_SOURCE_DIR}/base64.c
	)

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
set(libwebcam_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/webcam/v4l2_base.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/webcam/v4l2_colorspace.c
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/webcam/ccvt_c2.c
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/webcam/ccvt_misc.c
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/webcam/jpegutils.c
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/webcam/v4l2_decode/v4l2_decode.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/webcam/v4l2_decode/v4l2_builtin_decoder.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/webcam/v4l2_record/v4l2_record.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/webcam/v4l2_record/ser_recorder.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/webcam/v4l2_record/stream_recorder.cpp
	)
endif()

set (indimain_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/indidriver.c
        ${CMAKE_CURRENT_SOURCE_DIR}/indidrivermain.c
        ${CMAKE_CURRENT_SOURCE_DIR}/eventloop.c
    )

set (indiclient_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/basedevice.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/baseclient.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indiproperty.cpp
    )

set (indiclientqt_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/basedevice.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/baseclientqt.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indiproperty.cpp
    )
if(NOT ANDROID)
set (indidriver_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/basedevice.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/defaultdevice.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indiproperty.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indiccd.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/inditelescope.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indifilterwheel.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indifocuserinterface.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indifocuser.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indiusbdevice.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indiguiderinterface.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indifilterinterface.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indidome.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indigps.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indiweather.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indidustcapinterface.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indilightboxinterface.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indilogger.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indicontroller.cpp

    )
endif(NOT ANDROID)

set (lx_SRCS
     ${CMAKE_CURRENT_SOURCE_DIR}/libs/lx/Lx.cpp
)

IF (APPLE)
set(hidapi_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/hid_mac.c)
ELSEIF (WIN32)
set(hidapi_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/hid_win.c)
ELSE ()
set(hidapi_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/hid_libusb.c)
ENDIF()

#################################################
############# INDI Shared Library ###############
# To offer lilxml and communination routines    #
# Mostly used by generic clients                #
#################################################
if (WIN32 OR ANDROID)
    add_library(indi STATIC ${libindicom_SRCS} ${liblilxml_SRCS})
else(WIN32 OR ANDROID)
    add_library(indi SHARED ${libindicom_SRCS} ${liblilxml_SRCS})
endif(WIN32 OR ANDROID)

if (NOT WIN32)
    set_target_properties(indi PROPERTIES COMPILE_FLAGS "-fPIC") #Used for Android and Linux
endif(NOT WIN32)

set_target_properties(indi PROPERTIES VERSION ${CMAKE_INDI_VERSION_STRING} SOVERSION ${INDI_SOVERSION})

target_link_libraries(indi ${NOVA_LIBRARIES} ${M_LIB} ${ZLIB_LIBRARY} ${CFITSIO_LIBRARIES})

if (WIN32 OR ANDROID)
install(TARGETS indi ARCHIVE DESTINATION lib)
else(WIN32 OR ANDROID)
install(TARGETS indi LIBRARY DESTINATION ${LIB_DESTINATION})
endif(WIN32 OR ANDROID)

##################################################
########### INDI Client Static Library ###########
##################################################

if (NOT WIN32 AND NOT ANDROID)
add_library(indiclient STATIC ${indiclient_SRCS})
SET_TARGET_PROPERTIES(indiclient PROPERTIES COMPILE_FLAGS "-fPIC")
target_link_libraries(indiclient indi ${CMAKE_THREAD_LIBS_INIT})
install(TARGETS indiclient ARCHIVE DESTINATION ${LIB_DESTINATION})
endif(NOT WIN32 AND NOT ANDROID)

##################################################
#### INDI Client Static Library Qt5 Backend ######
##################################################
if (Qt5Network_FOUND)
message(STATUS "Building INDI Client with Qt5 support")

add_library(indiclientqt STATIC ${indiclientqt_SRCS})

if (NOT WIN32)
SET_TARGET_PROPERTIES(indiclientqt PROPERTIES COMPILE_FLAGS "-fPIC")
endif(NOT WIN32)

target_link_libraries(indiclientqt indi ${CMAKE_THREAD_LIBS_INIT})

qt5_use_modules(indiclientqt Network)

if (WIN32 OR ANDROID)
install(TARGETS indiclientqt ARCHIVE DESTINATION lib)
else(WIN32 OR ANDROID)
install(TARGETS indiclientqt ARCHIVE DESTINATION ${LIB_DESTINATION})
endif(WIN32 OR ANDROID)

endif(Qt5Network_FOUND)

# Do not build INDI server and drivers for Windows
if (NOT WIN32 AND NOT ANDROID)
######################################
########### INDI SERVER ##############
######################################
set(indiserver_SRCS indiserver.c fq.c)

add_executable(indiserver ${indiserver_SRCS} ${liblilxml_SRCS})

target_link_libraries(indiserver ${CMAKE_THREAD_LIBS_INIT})

install(TARGETS indiserver RUNTIME DESTINATION bin)

##################################################
########## INDI Default Driver Library ###########
# To link with main() and indibase classes  ######
##################################################
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
add_library(indidriver SHARED ${libindicom_SRCS} ${liblilxml_SRCS} ${indimain_SRCS} ${indidriver_SRCS} ${libwebcam_SRCS} ${hidapi_SRCS})
SET_TARGET_PROPERTIES(indidriver PROPERTIES COMPILE_FLAGS "-fPIC")
target_link_libraries(indidriver ${LIBUSB_1_LIBRARIES} ${NOVA_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${CFITSIO_LIBRARIES} ${M_LIB} ${ZLIB_LIBRARY} ${JPEG_LIBRARY})
add_library(indidriverstatic STATIC ${libindicom_SRCS} ${liblilxml_SRCS} ${indimain_SRCS} ${indidriver_SRCS} ${libwebcam_SRCS} ${hidapi_SRCS})
SET_TARGET_PROPERTIES(indidriverstatic PROPERTIES COMPILE_FLAGS "-fPIC")
target_link_libraries(indidriverstatic ${LIBUSB_1_LIBRARIES} ${NOVA_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${CFITSIO_LIBRARIES} ${M_LIB} ${ZLIB_LIBRARY} ${JPEG_LIBRARY})
else()
add_library(indidriver SHARED ${libindicom_SRCS} ${liblilxml_SRCS} ${indimain_SRCS} ${indidriver_SRCS} ${hidapi_SRCS})
target_link_libraries(indidriver ${LIBUSB_1_LIBRARIES} ${NOVA_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${CFITSIO_LIBRARIES} ${M_LIB} ${ZLIB_LIBRARY})
add_library(indidriverstatic STATIC ${libindicom_SRCS} ${liblilxml_SRCS} ${indimain_SRCS} ${indidriver_SRCS} ${hidapi_SRCS})
target_link_libraries(indidriverstatic ${LIBUSB_1_LIBRARIES} ${NOVA_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${CFITSIO_LIBRARIES} ${M_LIB} ${ZLIB_LIBRARY})
endif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")

#if (Qt5Network_FOUND)
#qt5_use_modules(indidriver Network)
#qt5_use_modules(indidriverstatic Network)
#endif(Qt5Network_FOUND)

set_target_properties(indidriver indidriverstatic PROPERTIES VERSION ${CMAKE_INDI_VERSION_STRING} SOVERSION ${INDI_SOVERSION} OUTPUT_NAME indidriver)
install(TARGETS indidriver LIBRARY DESTINATION ${LIB_DESTINATION})
install(TARGETS indidriverstatic ARCHIVE DESTINATION ${LIB_DESTINATION})

##################################################
########### INDI Alignment Subsystem #############
##################################################
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/alignment)

#####################################
######## AGENT GROUP #########
#####################################

########### Imager ##############
set(imager_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/agent/agent_imager.cpp
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
   ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/lx200driver.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/lx200basic.cpp )

add_executable(indi_lx200basic ${lx200basic_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_lx200basic indidriver)

install(TARGETS indi_lx200basic RUNTIME DESTINATION bin )

#################################################################################

########### LX200 Generic ###########
set(lx200generic_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/lx200driver.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/lx200autostar.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/lx200_16.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/lx200gps.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/lx200generic.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/lx200classic.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/lx200apdriver.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/lx200gemini.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/lx200zeq25.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/lx200pulsar2.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/lx200ap.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/lx200fs2.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/lx200ss2000pc.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/lx200_OnStep.cpp
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
exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink ${BIN_INSTALL_DIR}/indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200pulsar2)\n
exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink ${BIN_INSTALL_DIR}/indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200fs2)\n
exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink ${BIN_INSTALL_DIR}/indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200ss2000pc)\n
exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink ${BIN_INSTALL_DIR}/indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200_OnStep)\n
")
set_target_properties(indi_lx200generic PROPERTIES POST_INSTALL_SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/make_lx200generic_symlink.cmake)
#################################################################################

########### Celestron GPS ############
set(celestrongps_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/celestrondriver.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/celestrongps.cpp )

add_executable(indi_celestron_gps ${celestrongps_SRCS})

target_link_libraries(indi_celestron_gps indidriver)

install(TARGETS indi_celestron_gps RUNTIME DESTINATION bin )

#################################################################################


########### Takahashi Temma ##########
set(temma_SRCS
   ${indimain_SRCS}
   ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/temmadriver.cpp )

add_executable(indi_temma ${temma_SRCS})

target_link_libraries(indi_temma indidriver AlignmentDriver )

install(TARGETS indi_temma RUNTIME DESTINATION bin )
#################################################################################

########### Sky Commander #############
set(skycommander_SRCS
   ${indimain_SRCS}
   ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/skycommander.c )

add_executable(indi_skycommander ${skycommander_SRCS}  ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_skycommander  ${NOVA_LIBRARIES} ${M_LIB} )

install(TARGETS indi_skycommander  RUNTIME DESTINATION bin )

#################################################################################

########### Intelliscope ###############
set(intelliscope_SRCS
   ${indimain_SRCS}
   ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/intelliscope.c )

add_executable(indi_intelliscope ${intelliscope_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_intelliscope ${NOVA_LIBRARIES} ${M_LIB} )

install(TARGETS indi_intelliscope RUNTIME DESTINATION bin )

########### Syncscan ###############
set(synscan_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/synscanmount.cpp )

add_executable(indi_synscan ${synscan_SRCS})
target_link_libraries(indi_synscan indidriver AlignmentDriver)

install(TARGETS indi_synscan RUNTIME DESTINATION bin )

########### Magellan I #############
set(magellan_SRCS
   ${indimain_SRCS}
   ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/magellandriver.c
   ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/magellan1.cpp )

add_executable(indi_magellan1 ${magellan_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_magellan1 ${NOVA_LIBRARIES})

install(TARGETS indi_magellan1 RUNTIME DESTINATION bin )

########### IEQ Pro / CEM60 #############
set(ieq_SRCS
   ${indimain_SRCS}
   ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/ieqprodriver.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/ieqpro.cpp )

add_executable(indi_ieq_telescope ${ieq_SRCS})

target_link_libraries(indi_ieq_telescope indidriver)

install(TARGETS indi_ieq_telescope RUNTIME DESTINATION bin )

########### Telescope Simulator ##############
set(telescopesimulator_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/telescope_simulator.cpp
   )

add_executable(indi_simulator_telescope ${telescopesimulator_SRCS})
target_link_libraries(indi_simulator_telescope indidriver)
install(TARGETS indi_simulator_telescope RUNTIME DESTINATION bin )

########### Telescope Scripting Gateway ##############
set(telescopescript_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/telescope_script.cpp
   )

add_executable(indi_script_telescope ${telescopescript_SRCS})
target_link_libraries(indi_script_telescope indidriver)
install(TARGETS indi_script_telescope RUNTIME DESTINATION bin )

########### CCD Simulator ##############
if (CFITSIO_FOUND)

set(ccdsimulator_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/ccd/ccd_simulator.cpp
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
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/focuser/focus_simulator.cpp
   )

add_executable(indi_simulator_focus ${focussimulator_SRCS})
target_link_libraries(indi_simulator_focus indidriver)
install(TARGETS indi_simulator_focus RUNTIME DESTINATION bin )

################ Robo Focuser ################

set(robofocus_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/focuser/robofocus.cpp
   )

add_executable(indi_robo_focus ${robofocus_SRCS})
target_link_libraries(indi_robo_focus indidriver)
install(TARGETS indi_robo_focus RUNTIME DESTINATION bin )


################ Rigelsys NFocus Focuser ################

set(nfocus_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/focuser/nfocus.cpp
   )

add_executable(indi_nfocus ${nfocus_SRCS})
target_link_libraries(indi_nfocus indidriver)
install(TARGETS indi_nfocus RUNTIME DESTINATION bin )


################ Rigelsys NStep Focuser ################

set(nstep_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/focuser/nstep.cpp
   )

add_executable(indi_nstep_focus ${nstep_SRCS})
target_link_libraries(indi_nstep_focus indidriver)
install(TARGETS indi_nstep_focus RUNTIME DESTINATION bin )


################ Moonlite Focuser ################

set(moonlite_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/focuser/moonlite.cpp
   )

add_executable(indi_moonlite_focus ${moonlite_SRCS})
target_link_libraries(indi_moonlite_focus indidriver)
install(TARGETS indi_moonlite_focus RUNTIME DESTINATION bin )

################ Microtouch Focuser ################

set(microtouch_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/focuser/microtouch.cpp
   )

add_executable(indi_microtouch_focus ${microtouch_SRCS})
target_link_libraries(indi_microtouch_focus indidriver)
install(TARGETS indi_microtouch_focus RUNTIME DESTINATION bin )

################ Baader SteelDrive Focuser ################

set(steeldrive_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/focuser/steeldrive.cpp
   )

add_executable(indi_steeldrive_focus ${steeldrive_SRCS})
target_link_libraries(indi_steeldrive_focus indidriver)
install(TARGETS indi_steeldrive_focus RUNTIME DESTINATION bin )

################ FocusLynx Focuser ################

set(focuslynx_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/focuser/focuslynxbase.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/focuser/focuslynx.cpp
   )

add_executable(indi_lynx_focus ${focuslynx_SRCS})
target_link_libraries(indi_lynx_focus indidriver)
install(TARGETS indi_lynx_focus RUNTIME DESTINATION bin )

################ PerfectStar Focuser ################

set(perfectstar_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/focuser/perfectstar.cpp
   )

add_executable(indi_perfectstar_focus ${perfectstar_SRCS})
target_link_libraries(indi_perfectstar_focus indidriver)
install(TARGETS indi_perfectstar_focus RUNTIME DESTINATION bin )
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/drivers/focuser/99-perfectstar.rules DESTINATION ${UDEVRULES_INSTALL_DIR})
endif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")

################ JMI Smart Focus Focuser ################

set(smartfocus_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/focuser/smartfocus.cpp
   )

add_executable(indi_smartfocus_focus ${smartfocus_SRCS})
target_link_libraries(indi_smartfocus_focus indidriver)
install(TARGETS indi_smartfocus_focus RUNTIME DESTINATION bin )

################ Optec TCF-S ################

set(tcfs_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/focuser/tcfs.cpp
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
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/filter_wheel/trutech_wheel.c
   )

add_executable(indi_trutech_wheel ${trutechwheel_SRCS}  ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_trutech_wheel ${NOVA_LIBRARIES} ${M_LIB} ${ZLIB_LIBRARY})

install(TARGETS indi_trutech_wheel RUNTIME DESTINATION bin )

########### Filter Simulator ##############
set(xagylwheel_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/filter_wheel/xagyl_wheel.cpp
   )

add_executable(indi_xagyl_wheel ${xagylwheel_SRCS})
target_link_libraries(indi_xagyl_wheel indidriver)
install(TARGETS indi_xagyl_wheel RUNTIME DESTINATION bin )

########### Filter Simulator ##############
set(filtersimulator_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/filter_wheel/filter_simulator.cpp
   )

add_executable(indi_simulator_wheel ${filtersimulator_SRCS})
target_link_libraries(indi_simulator_wheel indidriver)
install(TARGETS indi_simulator_wheel RUNTIME DESTINATION bin )

########## Optec Wheel IFW ############
set(optecwheel_SRCS
        ${indimain_SRCS}
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/filter_wheel/ifwoptec.cpp
   )

add_executable(indi_optec_wheel ${optecwheel_SRCS})
target_link_libraries(indi_optec_wheel indidriver)
install(TARGETS indi_optec_wheel RUNTIME DESTINATION bin )

#################################################################################

#####################################
########## DOME GROUP ############
#####################################

################ Dome Simulator ################

set(domesimulator_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/dome/dome_simulator.cpp
   )

add_executable(indi_simulator_dome ${domesimulator_SRCS})
target_link_libraries(indi_simulator_dome indidriver)
install(TARGETS indi_simulator_dome RUNTIME DESTINATION bin )

################ Roll Off ################

set(rolloff_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/dome/roll_off.cpp
   )

add_executable(indi_rolloff_dome ${rolloff_SRCS})
target_link_libraries(indi_rolloff_dome indidriver)
install(TARGETS indi_rolloff_dome RUNTIME DESTINATION bin )


################ Baader Dome ################

set(baaderdome_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/dome/baader_dome.cpp
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
   ${CMAKE_CURRENT_SOURCE_DIR}/drivers/video/stvdriver.c
   ${CMAKE_CURRENT_SOURCE_DIR}/drivers/video/stv.c )

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
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/video/v4l2driver.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/video/indi_v4l2driver.cpp)

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
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/auxiliary/watchdog.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/auxiliary/watchdogclient.cpp
   )

add_executable(indi_watchdog ${watchdog_SRCS})

target_link_libraries(indi_watchdog indidriver indiclient)
install(TARGETS indi_watchdog RUNTIME DESTINATION bin )

########### Flip Flat & Flip Man Driver ###############

set(flipflat_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/auxiliary/flip_flat.cpp
   )

add_executable(indi_flipflat ${flipflat_SRCS})

target_link_libraries(indi_flipflat indidriver)
install(TARGETS indi_flipflat RUNTIME DESTINATION bin )

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/drivers/auxiliary/99-flipflat.rules DESTINATION ${UDEVRULES_INSTALL_DIR})
endif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")


########### STAR2000 Driver ###############

set(STAR2000_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/auxiliary/STAR2kdriver.c
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/auxiliary/STAR2000.cpp
   )

add_executable(indi_star2000 ${STAR2000_SRCS})

target_link_libraries(indi_star2000 indidriver)
install(TARGETS indi_star2000 RUNTIME DESTINATION bin )

########### GPUSB Driver ###############

set(gpusb_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/auxiliary/gpdriver.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/auxiliary/gpusb.cpp
   )

add_executable(indi_gpusb ${gpusb_SRCS})

target_link_libraries(indi_gpusb indidriver)
install(TARGETS indi_gpusb RUNTIME DESTINATION bin )
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/drivers/auxiliary/99-gpusb.rules DESTINATION ${UDEVRULES_INSTALL_DIR})
endif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")

########### Joystick Driver ###############

set(joystick_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/auxiliary/joystickdriver.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/auxiliary/joystick.cpp
   )

add_executable(indi_joystick ${joystick_SRCS})

target_link_libraries(indi_joystick indidriver)
install(TARGETS indi_joystick RUNTIME DESTINATION bin )


endif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")

########### GPS Simulator Driver ###############

set(gpssimulator_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/auxiliary/gps_simulator.cpp
   )

add_executable(indi_simulator_gps ${gpssimulator_SRCS})

target_link_libraries(indi_simulator_gps indidriver)
install(TARGETS indi_simulator_gps RUNTIME DESTINATION bin )

#####################################
############ AUX GROUP ##############
#####################################

########### Weather Meta Driver ###############

set(weathermeta_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/weather/weathermeta.cpp
   )

add_executable(indi_meta_weather ${weathermeta_SRCS})

target_link_libraries(indi_meta_weather indidriver)
install(TARGETS indi_meta_weather RUNTIME DESTINATION bin )

########### Vantage Driver ###############

set(vantage_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/weather/vantage.cpp
   )

add_executable(indi_vantage_weather ${vantage_SRCS})

target_link_libraries(indi_vantage_weather indidriver)
install(TARGETS indi_vantage_weather RUNTIME DESTINATION bin )
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/drivers/weather/99-vantage.rules DESTINATION ${UDEVRULES_INSTALL_DIR})
endif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")

########### WunderGround Driver ###############

set(WunderGround_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/weather/gason.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/weather/wunderground.cpp
   )

add_executable(indi_wunderground_weather ${WunderGround_SRCS})

target_link_libraries(indi_wunderground_weather indidriver ${CURL_LIBRARIES})
install(TARGETS indi_wunderground_weather RUNTIME DESTINATION bin )

#####################################
############ INDI TOOLS #############
#####################################


########### getINDI ##############
set(getindi_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/eventloop.c
        ${CMAKE_CURRENT_SOURCE_DIR}/tools/getINDIproperty.c
   )

add_executable(indi_getprop ${getindi_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_getprop ${NOVA_LIBRARIES} ${M_LIB} ${ZLIB_LIBRARY})

install(TARGETS indi_getprop RUNTIME DESTINATION bin )

#################################################################################

########### setINDI ##############
set(setindi_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/eventloop.c
        ${CMAKE_CURRENT_SOURCE_DIR}/tools/setINDIproperty.c
   )

add_executable(indi_setprop ${setindi_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_setprop ${NOVA_LIBRARIES} ${M_LIB} ${ZLIB_LIBRARY})

install(TARGETS indi_setprop RUNTIME DESTINATION bin )

#################################################################################

########### evelINDI ##############
set(evalindi_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/eventloop.c
        ${CMAKE_CURRENT_SOURCE_DIR}/tools/compiler.c
        ${CMAKE_CURRENT_SOURCE_DIR}/tools/evalINDI.c
   )

add_executable(indi_eval ${evalindi_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_eval ${NOVA_LIBRARIES} ${M_LIB} ${ZLIB_LIBRARY})

install(TARGETS indi_eval RUNTIME DESTINATION bin )

#################################################################################
## Build Examples. Not installation

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/examples)
endif()

#################################################################################

install( FILES drivers.xml ${CMAKE_CURRENT_SOURCE_DIR}/drivers/focuser/indi_tcfs_sk.xml DESTINATION ${DATA_INSTALL_DIR})

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/libindi.pc.cmake ${CMAKE_CURRENT_BINARY_DIR}/libindi.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libindi.pc DESTINATION ${PKGCONFIG_INSTALL_PREFIX})

# End INDI server/drivers/tools build for POSIX systems
endif (NOT WIN32 AND NOT ANDROID)

# Do not build anything from the following stuff for Android
if (NOT ANDROID)

install( FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/indiapi.h
    ${CMAKE_CURRENT_SOURCE_DIR}/indidevapi.h
    ${CMAKE_CURRENT_SOURCE_DIR}/base64.h
    ${CMAKE_CURRENT_SOURCE_DIR}/eventloop.h
    ${CMAKE_CURRENT_SOURCE_DIR}/indidriver.h
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/lilxml.h
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indibase.h
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indibasetypes.h
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/basedevice.h
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/defaultdevice.h
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indiccd.h
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indifilterwheel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indifocuserinterface.h
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indifocuser.h
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/inditelescope.h
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indiguiderinterface.h
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indifilterinterface.h
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indiproperty.h
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indidome.h
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indigps.h
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indilightboxinterface.h
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indidustcapinterface.h
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indiweather.h
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/indicom.h
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indilogger.h
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indicontroller.h
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indiusbdevice.h
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/hidapi.h
    DESTINATION ${INCLUDE_INSTALL_DIR}/libindi COMPONENT Devel)

if (NOT WIN32)
    install( FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/baseclient.h
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/webcam/ccvt.h
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/webcam/ccvt_types.h
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/webcam/v4l2_record/v4l2_record.h
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/webcam/v4l2_record/ser_recorder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/webcam/v4l2_decode/v4l2_decode.h
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/webcam/v4l2_decode/v4l2_builtin_decoder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/webcam/v4l2_record/stream_recorder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/webcam/ccvt_types.h ${CMAKE_CURRENT_SOURCE_DIR}/libs/webcam/v4l2_colorspace.h
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/webcam/jpegutils.h
    DESTINATION ${INCLUDE_INSTALL_DIR}/libindi COMPONENT Devel)
endif(NOT WIN32)

if (Qt5Network_FOUND)
    install(FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/baseclientqt.h
        DESTINATION ${INCLUDE_INSTALL_DIR}/libindi COMPONENT Devel)
endif(Qt5Network_FOUND)

###################################################################################################
#########################################  Fast Blob  #############################################
###################################################################################################
IF (INDI_FAST_BLOB)
# Append ENCLEN attribute to outgoing BLOB elements to enable fast parsing by clients
add_definitions(-DWITH_ENCLEN)
ENDIF(INDI_FAST_BLOB)

###################################################################################################
######################################  Calculate Min/Max #########################################
###################################################################################################
IF (INDI_CALCULATE_MINMAX)
# Calculate Min/Max values to store them in FITS header
add_definitions(-DWITH_MINMAX)
ENDIF(INDI_CALCULATE_MINMAX)
###################################################################################################
#########################################  Tests  #################################################
###################################################################################################

FIND_PACKAGE (GTest)
FIND_PACKAGE (GMock)
IF (GTEST_FOUND)
  IF (INDI_BUILD_UNITTESTS)
    MESSAGE (STATUS  "Building unit tests")
    ADD_SUBDIRECTORY(test)
  ELSE (INDI_BUILD_UNITTESTS)
    MESSAGE (STATUS  "Not building unit tests")
  ENDIF (INDI_BUILD_UNITTESTS)
ELSE()
  MESSAGE (STATUS  "GTEST  not found, not building unit tests")
ENDIF (GTEST_FOUND)
endif(NOT ANDROID) # End not build anything from the following stuff for Android

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
