cmake_minimum_required(VERSION 3.0)
PROJECT(libindi C CXX)

# Tell CMake to run moc when necessary:
set(CMAKE_AUTOMOC ON)
# As moc files are generated in the binary dir, tell CMake
# to always look for includes there:
set(CMAKE_INCLUDE_CURRENT_DIR ON)

include(GNUInstallDirs)
include(FeatureSummary)

if(ANDROID OR "${CMAKE_SYSTEM_NAME}" STREQUAL "Android")
  set(ANDROID ON)
  add_definitions(-DANDROID)
endif()

IF ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++11")
ELSE ()
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
ENDIF ()

# Ccache support
IF (ANDROID OR UNIX OR APPLE)
    FIND_PROGRAM(CCACHE_FOUND ccache)
    SET(CCACHE_SUPPORT OFF CACHE BOOL "Enable ccache support")
    IF ((CCACHE_FOUND OR ANDROID) AND CCACHE_SUPPORT MATCHES ON)
        SET_PROPERTY(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
        SET_PROPERTY(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
    ENDIF ()
ENDIF ()

# Add security (hardening flags)
IF (UNIX OR APPLE OR ANDROID)
    SET(SEC_COMP_FLAGS "-D_FORTIFY_SOURCE=2 -fstack-protector-all -fPIE")
    # Make sure to add optimization flag. Some systems require this for _FORTIFY_SOURCE.
    IF (NOT CMAKE_BUILD_TYPE MATCHES "MinSizeRel" AND NOT CMAKE_BUILD_TYPE MATCHES "Release" AND NOT CMAKE_BUILD_TYPE MATCHES "Debug")
        SET(SEC_COMP_FLAGS "${SEC_COMP_FLAGS} -O1")
    ENDIF ()
    IF (NOT ANDROID AND NOT "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" AND NOT APPLE)
        SET(SEC_COMP_FLAGS "${SEC_COMP_FLAGS} -Wa,--noexecstack")
    ENDIF ()
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${SEC_COMP_FLAGS}")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${SEC_COMP_FLAGS}")
    SET(SEC_LINK_FLAGS "")
    IF (NOT APPLE)
        SET(SEC_LINK_FLAGS "${SEC_LINK_FLAGS} -Wl,-z,nodump -Wl,-z,noexecstack -Wl,-z,relro -Wl,-z,now")
    ENDIF ()
    IF (NOT ANDROID AND NOT APPLE)
        SET(SEC_LINK_FLAGS "${SEC_LINK_FLAGS} -pie")
    ENDIF ()
    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${SEC_LINK_FLAGS}")
    SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${SEC_LINK_FLAGS}")
ENDIF ()

# Clang Format support
IF (UNIX OR APPLE)
    SET(FORMAT_CODE OFF CACHE BOOL "Enable Clang Format")
    IF (FORMAT_CODE MATCHES ON)
        FILE(GLOB_RECURSE ALL_SOURCE_FILES *.c *.cpp *.h)

        FOREACH(SOURCE_FILE ${ALL_SOURCE_FILES})
            STRING(FIND ${SOURCE_FILE} ${CMAKE_SOURCE_DIR} DIR_FOUND)
            IF (NOT ${DIR_FOUND} EQUAL 0)
                LIST(REMOVE_ITEM ALL_SOURCE_FILES ${SOURCE_FILE})
            ENDIF ()
        ENDFOREACH ()

        FIND_PROGRAM(CLANGFORMAT_EXE NAMES clang-format-5.0)
        IF (CLANGFORMAT_EXE)
            ADD_CUSTOM_TARGET(clang-format COMMAND ${CLANGFORMAT_EXE} -style=file -i ${ALL_SOURCE_FILES})
        ENDIF ()
    ENDIF ()
ENDIF ()

# Warning, debug and linker flags
IF (UNIX OR APPLE)
    SET(COMP_FLAGS "")
    SET(LINKER_FLAGS "")
    # Verbose warnings and turns all to errors
    SET(COMP_FLAGS "${COMP_FLAGS} -Wall -Wextra")
    IF (NOT "${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
        SET(COMP_FLAGS "${COMP_FLAGS} -Werror")
    ENDIF ()
    # Omit problematic warnings
    IF ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        SET(COMP_FLAGS "${COMP_FLAGS} -Wno-unused-but-set-variable")
    ENDIF ()

    # Minimal debug info with Clang
    IF ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        SET(COMP_FLAGS "${COMP_FLAGS} -gline-tables-only")
    ELSE ()
        SET(COMP_FLAGS "${COMP_FLAGS} -g")
    ENDIF ()

    # Note: The following flags are problematic on older systems with gcc 4.8
    IF ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 4.9.9))
        # Compress the debug sections
        # Note: Before valgrind 3.12.0, patch should be applied for valgrind (https://bugs.kde.org/show_bug.cgi?id=303877)
        IF (NOT APPLE AND NOT ANDROID AND NOT CMAKE_SYSTEM_PROCESSOR MATCHES arm AND NOT CMAKE_CXX_CLANG_TIDY)
            SET(COMP_FLAGS "${COMP_FLAGS} -Wa,--compress-debug-sections -Wno-unused-command-line-argument")
            SET(LINKER_FLAGS "${LINKER_FLAGS} -Wl,--compress-debug-sections=zlib")
        ENDIF ()

        # ld.gold is 2x faster than normal ld
        IF (NOT APPLE AND NOT CMAKE_SYSTEM_PROCESSOR MATCHES arm)
            SET(LINKER_FLAGS "${LINKER_FLAGS} -fuse-ld=gold")
            # Use Identical Code Folding
            SET(COMP_FLAGS "${COMP_FLAGS} -ffunction-sections")
            SET(LINKER_FLAGS "${LINKER_FLAGS} -Wl,--icf=safe")
        ENDIF ()
    ENDIF ()

    # Apply the flags
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COMP_FLAGS}")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMP_FLAGS}")
    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${LINKER_FLAGS}")
    SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${LINKER_FLAGS}")
ENDIF ()

#####################################  INDI version  ################################################
# N.B. DO NOT Forget to update version also in indiapi.h
# Proper way is to use indiversion.h.cmake file but this would break make existing applications so let us stick to the old proven way

set(INDI_SOVERSION "1")
set(CMAKE_INDI_VERSION_MAJOR 1)
set(CMAKE_INDI_VERSION_MINOR 4)
set(CMAKE_INDI_VERSION_RELEASE 1)
set(CMAKE_INDI_VERSION_STRING "${CMAKE_INDI_VERSION_MAJOR}.${CMAKE_INDI_VERSION_MINOR}.${CMAKE_INDI_VERSION_RELEASE}")
set(INDI_VERSION ${CMAKE_INDI_VERSION_MAJOR}.${CMAKE_INDI_VERSION_MINOR}.${CMAKE_INDI_VERSION_RELEASE})

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
## the following are directories where stuff will be installed to
set(INCLUDE_INSTALL_DIR      "${CMAKE_INSTALL_PREFIX}/include/")
set(PKGCONFIG_INSTALL_PREFIX "${CMAKE_INSTALL_LIBDIR}/pkgconfig/")
set(UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")

set(PKG_CONFIG_LIBDIR ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR})

#####################################  Build Options  ##############################################
# Select which components to build and what options to apply

OPTION (INDI_BUILD_SERVER "Build INDI Server" ON)
OPTION (INDI_BUILD_DRIVERS "Build INDI Drivers, Tools, and Examples" ON)
OPTION (INDI_BUILD_POSIX_CLIENT "Build INDI POSIX Client" ON)
OPTION (INDI_BUILD_QT5_CLIENT "Build INDI Qt5 Client" OFF)
OPTION (INDI_BUILD_UNITTESTS "Build INDI tests" OFF)
OPTION (INDI_FAST_BLOB "Build INDI with Fast BLOB support" ON)
OPTION (INDI_CALCULATE_MINMAX "Calculate and store image minimum and maximum values in FITS header" OFF)

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

#######################################  config.h  #################################################
# Generate config.h from template
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indiversion.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/indiversion.h )

###################################################################################################
#####################################  Components  ################################################
###################################################################################################

set_package_properties(Nova PROPERTIES DESCRIPTION "A general purpose, double precision, Celestial Mechanics, Astrometry and Astrodynamics library" URL "http://libnova.sourceforge.net" TYPE REQUIRED PURPOSE "Provides INDI with astrodynamics library.")
set_package_properties(CFITSIO PROPERTIES DESCRIPTION "A library for reading and writing data files in FITS (Flexible Image Transport System) data format" URL "http://heasarc.gsfc.nasa.gov/fitsio/fitsio.html" TYPE REQUIRED PURPOSE "Provides INDI with FITS I/O support.")

####################################################################################################
#
# Component   : INDI Server
# Dependencies: pthreads
# Supported OS: Linux, BSD, MacOS, Cygwin
#
#################################################################################################
if (INDI_BUILD_SERVER)
if (WIN32 OR ANDROID)
    message(WARNING "INDI Server is only supported under Linux, BSD, MacOS, and Cygwin while current system is " ${CMAKE_SYSTEM_NAME})
else()

# 1. Dependencies
find_package(Threads REQUIRED)
# 2. Includes
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
# 3. Build
add_executable(indiserver
    ${CMAKE_CURRENT_SOURCE_DIR}/indiserver.c
    ${CMAKE_CURRENT_SOURCE_DIR}/fq.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/lilxml.c
    )
target_link_libraries(indiserver ${CMAKE_THREAD_LIBS_INIT})
install(TARGETS indiserver RUNTIME DESTINATION bin)
endif (WIN32 OR ANDROID)
endif (INDI_BUILD_SERVER)

#################################################################################################
#
# Component   : INDI POSIX Client
# Dependencies: pthreads, zlib, cfitsio, nova
# Supported OS: Linux, BSD, MacOS, Cygwin
#
#################################################################################################
if (INDI_BUILD_POSIX_CLIENT)
if (WIN32 OR ANDROID)
    message(WARNING "INDI POSIX Client is only supported under Linux, BSD, MacOS, and Cygwin while current system is " ${CMAKE_SYSTEM_NAME})
else()
# 1. Dependencies
find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)
find_package(CFITSIO REQUIRED)
find_package(Nova REQUIRED)
# 2. Includes
include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/libs)
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase)
include_directories( ${CFITSIO_INCLUDE_DIR})
include_directories( ${NOVA_INCLUDE_DIR})
# 3. Build
add_library(indiclient STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/basedevice.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/baseclient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indiproperty.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/lilxml.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/indicom.c
    ${CMAKE_CURRENT_SOURCE_DIR}/base64.c)
if (NOT CYGWIN AND NOT WIN32)
set_target_properties(indiclient PROPERTIES COMPILE_FLAGS "-fPIC")
endif (NOT CYGWIN AND NOT WIN32)
target_link_libraries(indiclient ${CMAKE_THREAD_LIBS_INIT})
install(TARGETS indiclient ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/baseclient.h DESTINATION ${INCLUDE_INSTALL_DIR}/libindi COMPONENT Devel)
endif (WIN32 OR ANDROID)
endif (INDI_BUILD_POSIX_CLIENT)

#################################################################################################
#
# Component   : INDI Qt5 Client
# Dependencies: Qt5Network, zlib, cfitsio, nova
# Supported OS: Linux, BSD, MacOS, Cygwin, Windows, Android
#
#################################################################################################
if (INDI_BUILD_QT5_CLIENT)
set(QT_ANDROID "" CACHE path "Qt Android path")
# 1. Dependencies
if (ANDROID)
    include(${QT_ANDROID}/lib/cmake/Qt5Network/Qt5NetworkConfig.cmake)
    if (NOT CFITSIO_DIR)
        message(FATAL_ERROR CFITSIO_DIR must be set)
    else ()
        set(CFITSIO_INCLUDE_DIR ${CFITSIO_DIR})
        set(CFITSIO_LIBRARIES ${CFITSIO_DIR}/libcfitsio.a)
    endif ()
    if (NOT NOVA_DIR)
        message(FATAL_ERROR NOVA_DIR must be set)
    else ()
        set(NOVA_INCLUDE_DIR ${NOVA_DIR}/src ${NOVA_DIR}/src/libnova)
        set(NOVA_LIBRARIES ${NOVA_DIR}/lib/liblibnova.a)
    endif ()
else ()
    find_package(Qt5Network)
    find_package(ZLIB REQUIRED)
    find_package(CFITSIO REQUIRED)
    find_package(Nova REQUIRED)
endif ()
# 2. Includes
include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/libs)
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase)
include_directories( ${CFITSIO_INCLUDE_DIR})
include_directories( ${NOVA_INCLUDE_DIR})
# 3. Build
message(STATUS "Building INDI Client with Qt5 support")
add_library(indiclientqt STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/basedevice.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/baseclientqt.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indiproperty.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/lilxml.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/indicom.c
    ${CMAKE_CURRENT_SOURCE_DIR}/base64.c
    )
if (NOT CYGWIN AND NOT WIN32)
set_target_properties(indiclientqt PROPERTIES COMPILE_FLAGS "-fPIC")
endif(NOT CYGWIN AND NOT WIN32)
qt5_use_modules(indiclientqt Network)
if (WIN32 OR ANDROID)
install(TARGETS indiclientqt ARCHIVE DESTINATION lib)
else(WIN32 OR ANDROID)
install(TARGETS indiclientqt ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif(WIN32 OR ANDROID)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/baseclientqt.h DESTINATION ${INCLUDE_INSTALL_DIR}/libindi COMPONENT Devel)
endif (INDI_BUILD_QT5_CLIENT)

#################################################################################################
#
# Component   : INDI Drivers, Tools, and Examples
# Dependencies: pthreads, usb1, zLib, cfitsio, nova, curl, jpeg (Linux Only)
# Supported OS: Linux, BSD, MacOS, Cygwin
# N.B. Webcam drivers only supported under Linux (Video4Linux2). Joystick support only under Linux
#
#################################################################################################
if (INDI_BUILD_DRIVERS)
if (WIN32 OR ANDROID)
    message(WARNING "INDI drivers are only supported under Linux, BSD, MacOS, and Cygwin while current system is " ${CMAKE_SYSTEM_NAME})
else()
# 1. Dependencies
find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)
find_package(CFITSIO REQUIRED)
find_package(Nova REQUIRED)
find_package(USB-1 REQUIRED)
find_package(CURL REQUIRED)
find_package(GSL REQUIRED)
# libjpeg for V4L2
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  find_package(JPEG REQUIRED)
endif()
# Math Library
FIND_LIBRARY(M_LIB m)
# 2. Includes
include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/libs)
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase)
include_directories( ${CFITSIO_INCLUDE_DIR})
include_directories( ${NOVA_INCLUDE_DIR})
include_directories( ${LIBUSB_1_INCLUDE_DIRS})
include_directories( ${GSL_INCLUDE_DIRS})
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/libs/webcam)
  include_directories( ${JPEG_INCLUDE_DIR} )
endif()

###################################################################################################
########################################  Sources  ################################################
###################################################################################################
IF (APPLE)
set(hidapi_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/hid_mac.c)
ELSEIF (WIN32)
set(hidapi_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/hid_win.c)
ELSE ()
set(hidapi_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/hid_libusb.c)
ENDIF()

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

set (indidriver_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/indidriver.c
        ${CMAKE_CURRENT_SOURCE_DIR}/indidrivermain.c
        ${CMAKE_CURRENT_SOURCE_DIR}/eventloop.c
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/lilxml.c
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indicom.c
        ${CMAKE_CURRENT_SOURCE_DIR}/base64.c
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
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/connectionplugins/connectioninterface.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/connectionplugins/connectionserial.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/connectionplugins/connectiontcp.cpp

    )

##################################################
########## INDI Default Driver Library ###########
##################################################

if (CYGWIN)
## For Cygwin we only build static library
add_definitions(-U__STRICT_ANSI__)
find_package(Iconv REQUIRED)
add_library(indidriver STATIC ${indidriver_SRCS} ${hidapi_SRCS})
set_target_properties(indidriver PROPERTIES VERSION ${CMAKE_INDI_VERSION_STRING} SOVERSION ${INDI_SOVERSION} OUTPUT_NAME indidriver)
target_link_libraries(indidriver ${ICONV_LIBRARIES} ${LIBUSB_1_LIBRARIES} ${NOVA_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${CFITSIO_LIBRARIES} ${M_LIB} ${ZLIB_LIBRARY} ${JPEG_LIBRARY})
install(TARGETS indidriver ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
else()
## Static indidriver Library
add_library(indidriverstatic STATIC ${indidriver_SRCS} ${libwebcam_SRCS} ${hidapi_SRCS})
set_target_properties(indidriverstatic PROPERTIES COMPILE_FLAGS "-fPIC")
set_target_properties(indidriverstatic PROPERTIES VERSION ${CMAKE_INDI_VERSION_STRING} SOVERSION ${INDI_SOVERSION} OUTPUT_NAME indidriver)
target_link_libraries(indidriverstatic ${LIBUSB_1_LIBRARIES} ${NOVA_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${CFITSIO_LIBRARIES} ${M_LIB} ${ZLIB_LIBRARY} ${JPEG_LIBRARY})
install(TARGETS indidriverstatic ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
## Dynamic indidriver Library
add_library(indidriver SHARED ${indidriver_SRCS} ${libwebcam_SRCS} ${hidapi_SRCS})
set_target_properties(indidriver PROPERTIES COMPILE_FLAGS "-fPIC")
set_target_properties(indidriver PROPERTIES VERSION ${CMAKE_INDI_VERSION_STRING} SOVERSION ${INDI_SOVERSION} OUTPUT_NAME indidriver)
target_link_libraries(indidriver ${LIBUSB_1_LIBRARIES} ${NOVA_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${CFITSIO_LIBRARIES} ${M_LIB} ${ZLIB_LIBRARY} ${JPEG_LIBRARY})
install(TARGETS indidriver LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
set(PKG_CONFIG_LIBS "${PKG_CONFIG_LIBS} -lindidriver -lindiAlignmentDriver")
endif(CYGWIN)


##################################################
########### INDI Alignment Subsystem #############
##################################################
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/alignment)

#####################################
######## AGENT GROUP ################
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
   ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/lx200driver.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/lx200basic.cpp )

add_executable(indi_lx200basic ${lx200basic_SRCS})

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
   ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/lx200_10micron.cpp
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
exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink ${BIN_INSTALL_DIR}/indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200_10micron)\n
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

########### Paramount ##########
set(paramount_SRCS
   ${indimain_SRCS}
   ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/paramount.cpp )

add_executable(indi_paramount_telescope ${paramount_SRCS})

target_link_libraries(indi_paramount_telescope indidriver)

install(TARGETS indi_paramount_telescope RUNTIME DESTINATION bin )

#################################################################################

########### Syncscan ###############
set(synscan_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/synscanmount.cpp )

add_executable(indi_synscan_telescope ${synscan_SRCS})
target_link_libraries(indi_synscan_telescope indidriver AlignmentDriver)

install(TARGETS indi_synscan_telescope RUNTIME DESTINATION bin )

########### Sky Commander ###############
set(skycommander_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/skycommander.cpp )

add_executable(indi_skycommander_telescope ${skycommander_SRCS})
target_link_libraries(indi_skycommander_telescope indidriver)

install(TARGETS indi_skycommander_telescope RUNTIME DESTINATION bin )

########### Generic Digital Setting Circle ###############
set(dsc_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/dsc.cpp )

add_executable(indi_dsc_telescope ${dsc_SRCS})
target_link_libraries(indi_dsc_telescope indidriver AlignmentDriver)

install(TARGETS indi_dsc_telescope RUNTIME DESTINATION bin )

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
set(ccdsimulator_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/ccd/ccd_simulator.cpp
   )

add_executable(indi_simulator_ccd ${ccdsimulator_SRCS})
target_link_libraries(indi_simulator_ccd indidriver)
install(TARGETS indi_simulator_ccd RUNTIME DESTINATION bin )

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


################ NightCrawler Focuser & Rotator ################

set(nightcrawler_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/focuser/nightcrawler.cpp
   )

add_executable(indi_nightcrawler_focus ${nightcrawler_SRCS})
target_link_libraries(indi_nightcrawler_focus indidriver)
install(TARGETS indi_nightcrawler_focus RUNTIME DESTINATION bin )

################ Moonlite Focuser ################

set(moonlite_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/focuser/moonlite.cpp
   )

add_executable(indi_moonlite_focus ${moonlite_SRCS})
target_link_libraries(indi_moonlite_focus indidriver)
install(TARGETS indi_moonlite_focus RUNTIME DESTINATION bin )

################## USB Focus V3 ##################

set(usbfocusv3_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/focuser/usbfocusv3.cpp
   )

add_executable(indi_usbfocusv3_focus ${usbfocusv3_SRCS})
target_link_libraries(indi_usbfocusv3_focus indidriver)
install(TARGETS indi_usbfocusv3_focus RUNTIME DESTINATION bin )

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

################ hitechfocus Focuser ################
set(hitecastrodcfocuser_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/focuser/hitecastrodcfocuser.cpp
   )

add_executable(indi_hitecastrodc_focus ${hitecastrodcfocuser_SRCS})
target_link_libraries(indi_hitecastrodc_focus indidriver)
install(TARGETS indi_hitecastrodc_focus RUNTIME DESTINATION bin )
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/drivers/focuser/99-hitecastrodcfocuser.rules DESTINATION ${UDEVRULES_INSTALL_DIR})
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

########### XAGYL Wheel ##############
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

########## Quantum Wheel ############
set(quantum_wheel_SRCS
        ${indimain_SRCS}
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/filter_wheel/quantum_wheel.cpp
   )

add_executable(indi_quantum_wheel ${quantum_wheel_SRCS})
target_link_libraries(indi_quantum_wheel indidriver)
install(TARGETS indi_quantum_wheel RUNTIME DESTINATION bin )

########## TruTech Wheel ############
set(trutechwheel_SRCS
        ${indimain_SRCS}
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/filter_wheel/trutech_wheel.cpp
   )

add_executable(indi_trutech_wheel ${trutechwheel_SRCS})
target_link_libraries(indi_trutech_wheel indidriver)
install(TARGETS indi_trutech_wheel RUNTIME DESTINATION bin )

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

########### Dome Scripting Gateway ##############
set(domescript_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/dome/dome_script.cpp
   )

add_executable(indi_script_dome ${domescript_SRCS})
target_link_libraries(indi_script_dome indidriver)
install(TARGETS indi_script_dome RUNTIME DESTINATION bin )

#################################################################################

#########################################
########### VIDEO GROUP   ###############
#########################################

########### INDI::CCD V4L Driver ###############
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")

set(v4l2driverccd_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/lx/Lx.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/video/v4l2driver.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/video/indi_v4l2driver.cpp)

add_executable(indi_v4l2_ccd ${v4l2driverccd_SRCS} ${libwebcam_SRCS})

target_link_libraries(indi_v4l2_ccd ${JPEG_LIBRARY} indidriver)

install(TARGETS indi_v4l2_ccd RUNTIME DESTINATION bin )

endif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")

#################################################################################

#####################################
############ AUX GROUP ##############
#####################################

########### SkySafari Middleware ##############
if (INDI_BUILD_POSIX_CLIENT)
set(skysafari_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/auxiliary/skysafari.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/auxiliary/skysafariclient.cpp
   )

add_executable(indi_skysafari ${skysafari_SRCS})
target_link_libraries(indi_skysafari indidriver indiclient)
install(TARGETS indi_skysafari RUNTIME DESTINATION bin )
else()
    message(WARNING "Skipping build of INDI SkySafari driver since INDI POSIX Client is not built")
endif (INDI_BUILD_POSIX_CLIENT)

########### Watch dog ###############
if (INDI_BUILD_POSIX_CLIENT)
set(watchdog_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/auxiliary/watchdog.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/auxiliary/watchdogclient.cpp
   )

add_executable(indi_watchdog ${watchdog_SRCS})

target_link_libraries(indi_watchdog indidriver indiclient)
install(TARGETS indi_watchdog RUNTIME DESTINATION bin )
else()
    message(WARNING "Skipping build of INDI WatchDog driver since INDI POSIX Client is not built")
endif (INDI_BUILD_POSIX_CLIENT)

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

########### Sky Quality Meter ###############

set(sqm_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/auxiliary/sqm.cpp
   )

add_executable(indi_sqm_weather ${sqm_SRCS})

target_link_libraries(indi_sqm_weather indidriver)
install(TARGETS indi_sqm_weather RUNTIME DESTINATION bin )

########### Astrometry Driver ###############

set(astrometry_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/auxiliary/astrometrydriver.cpp
   )

add_executable(indi_astrometry ${astrometry_SRCS})

target_link_libraries(indi_astrometry indidriver)
install(TARGETS indi_astrometry RUNTIME DESTINATION bin )


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

########### Joystick Driver ###############
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
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
        ${CMAKE_CURRENT_SOURCE_DIR}/base64.c
        ${CMAKE_CURRENT_SOURCE_DIR}/tools/getINDIproperty.c
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/lilxml.c
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indicom.c
        )

add_executable(indi_getprop ${getindi_SRCS})

target_link_libraries(indi_getprop ${NOVA_LIBRARIES} ${M_LIB} ${ZLIB_LIBRARY})

install(TARGETS indi_getprop RUNTIME DESTINATION bin )

#################################################################################

########### setINDI ##############
add_executable(indi_setprop
    ${CMAKE_CURRENT_SOURCE_DIR}/eventloop.c
    ${CMAKE_CURRENT_SOURCE_DIR}/base64.c
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/setINDIproperty.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/lilxml.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/indicom.c)

target_link_libraries(indi_setprop ${NOVA_LIBRARIES} ${M_LIB} ${ZLIB_LIBRARY})

install(TARGETS indi_setprop RUNTIME DESTINATION bin )

#################################################################################

########### evelINDI ##############
add_executable(indi_eval
    ${CMAKE_CURRENT_SOURCE_DIR}/eventloop.c
    ${CMAKE_CURRENT_SOURCE_DIR}/base64.c
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/compiler.c
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/evalINDI.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/lilxml.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/indicom.c)

target_link_libraries(indi_eval ${NOVA_LIBRARIES} ${M_LIB} ${ZLIB_LIBRARY})

install(TARGETS indi_eval RUNTIME DESTINATION bin )

#################################################################################
## Build Examples

if (INDI_BUILD_POSIX_CLIENT)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/examples)
else()
    message(WARNING "Skipping build of examples since INDI POSIX client is not built")
endif()

#################################################################################

install( FILES drivers.xml ${CMAKE_CURRENT_SOURCE_DIR}/drivers/focuser/indi_tcfs_sk.xml DESTINATION ${DATA_INSTALL_DIR})

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/libindi.pc.cmake ${CMAKE_CURRENT_BINARY_DIR}/libindi.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libindi.pc DESTINATION ${PKGCONFIG_INSTALL_PREFIX})

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    install( FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/webcam/ccvt.h
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/webcam/ccvt_types.h
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/webcam/v4l2_record/v4l2_record.h
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/webcam/v4l2_record/ser_recorder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/webcam/v4l2_decode/v4l2_decode.h
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/webcam/v4l2_decode/v4l2_builtin_decoder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/webcam/v4l2_record/stream_recorder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/webcam/ccvt_types.h
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/webcam/v4l2_colorspace.h
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/webcam/jpegutils.h
    DESTINATION ${INCLUDE_INSTALL_DIR}/libindi COMPONENT Devel)
endif()

###################################################################################################
#########################################  Tests  #################################################
###################################################################################################

find_package (GTest)
find_package (GMock)
IF (GTEST_FOUND)
  IF (INDI_BUILD_UNITTESTS)
    MESSAGE (STATUS  "Building unit tests")
    ADD_SUBDIRECTORY(test)
  ELSE (INDI_BUILD_UNITTESTS)
    MESSAGE (STATUS  "Not building unit tests")
  ENDIF (INDI_BUILD_UNITTESTS)
ELSE()
  MESSAGE (STATUS  "GTEST not found, not building unit tests")
ENDIF (GTEST_FOUND)

endif (WIN32 OR ANDROID)
endif(INDI_BUILD_DRIVERS)

# Install common dev files for all except server
if (INDI_BUILD_DRIVERS OR INDI_BUILD_POSIX_CLIENT OR INDI_BUILD_QT5_CLIENT)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indiversion.h DESTINATION ${INCLUDE_INSTALL_DIR}/libindi COMPONENT Devel)
    install( FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/indiapi.h
        ${CMAKE_CURRENT_SOURCE_DIR}/indidevapi.h
        ${CMAKE_CURRENT_SOURCE_DIR}/base64.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/lilxml.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indicom.h
        ${CMAKE_CURRENT_SOURCE_DIR}/eventloop.h
        ${CMAKE_CURRENT_SOURCE_DIR}/indidriver.h
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
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indilogger.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indicontroller.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indiusbdevice.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/hidapi.h
        DESTINATION ${INCLUDE_INSTALL_DIR}/libindi COMPONENT Devel)

install( FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/connectionplugins/connectioninterface.h
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/connectionplugins/connectionserial.h
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/connectionplugins/connectiontcp.h
    DESTINATION ${INCLUDE_INSTALL_DIR}/libindi/connectionplugins COMPONENT Devel)
endif (INDI_BUILD_DRIVERS OR INDI_BUILD_POSIX_CLIENT OR INDI_BUILD_QT5_CLIENT)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)

message(STATUS "The following components are going to be built:")
if (INDI_BUILD_SERVER)
    message(STATUS "## INDI Server")
endif()
if (INDI_BUILD_DRIVERS)
    message(STATUS "## INDI Drivers, Tools, and Examples")
endif()
if (INDI_BUILD_POSIX_CLIENT)
    message(STATUS "## INDI POSIX Client")
endif()
if (INDI_BUILD_QT5_CLIENT)
    message(STATUS "## INDI Qt5 Client")
endif()
