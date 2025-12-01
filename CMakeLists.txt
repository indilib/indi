cmake_minimum_required(VERSION 3.18)
PROJECT(libindi C CXX)
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.31)
    cmake_policy(SET CMP0177 NEW)
endif()

# As moc files are generated in the binary dir, tell CMake
# to always look for includes there:
set(CMAKE_INCLUDE_CURRENT_DIR ON)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include(GNUInstallDirs)
include(FeatureSummary)
include(CTest)

if(ANDROID OR "${CMAKE_SYSTEM_NAME}" STREQUAL "Android")
    set(ANDROID ON)
    add_definitions(-DANDROID)
endif()

include(CMakeCommon)
include(CheckSymbolExists)

# Clang Format support
if(UNIX OR APPLE)
    set(FORMAT_CODE OFF CACHE BOOL "Enable Clang Format")

    if(FORMAT_CODE MATCHES ON)
        FILE(GLOB_RECURSE ALL_SOURCE_FILES *.c *.cpp *.h)

        FOREACH(SOURCE_FILE ${ALL_SOURCE_FILES})
            STRING(FIND ${SOURCE_FILE} ${CMAKE_SOURCE_DIR} DIR_FOUND)

            if(NOT ${DIR_FOUND} EQUAL 0)
                LIST(REMOVE_ITEM ALL_SOURCE_FILES ${SOURCE_FILE})
            endif()
        ENDFOREACH()

        FIND_PROGRAM(CLANGFORMAT_EXE NAMES clang-format-5.0)

        if(CLANGFORMAT_EXE)
            ADD_CUSTOM_TARGET(clang-format COMMAND ${CLANGFORMAT_EXE} -style=file -i ${ALL_SOURCE_FILES})
        endif()
    endif()
endif()

# ####################################  INDI version  ################################################
set(CMAKE_INDI_VERSION_MAJOR 2)
set(CMAKE_INDI_VERSION_MINOR 1)
set(CMAKE_INDI_VERSION_RELEASE 7)

set(INDI_SOVERSION ${CMAKE_INDI_VERSION_MAJOR})
set(CMAKE_INDI_VERSION_STRING "${CMAKE_INDI_VERSION_MAJOR}.${CMAKE_INDI_VERSION_MINOR}.${CMAKE_INDI_VERSION_RELEASE}")
set(INDI_VERSION ${CMAKE_INDI_VERSION_MAJOR}.${CMAKE_INDI_VERSION_MINOR}.${CMAKE_INDI_VERSION_RELEASE})

execute_process(
    COMMAND git describe --tags
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_TAG
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE GIT_TAG_RESULT
)

if(NOT ${GIT_TAG_RESULT} EQUAL 0)
    set(GIT_TAG "${CMAKE_INDI_VERSION_STRING}-tgz")
endif()

add_definitions(-DGIT_TAG_STRING=\"${GIT_TAG}\")

# #######################################  Paths  ###################################################
set(DATA_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/share/indi/")
set(BIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/bin")
set(INCLUDE_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/include")

if(APPLE)
    set(CMAKE_SHARED_LINKER_FLAGS "-undefined dynamic_lookup")
endif(APPLE)

# #################################  Install Directories  ###########################################
# # the following are directories where stuff will be installed to
set(INCLUDE_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/include/")
set(PKGCONFIG_INSTALL_PREFIX "${CMAKE_INSTALL_LIBDIR}/pkgconfig/")
set(UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")

set(PKG_CONFIG_LIBDIR ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR})

# ####################################  Build Options  ##############################################
# Select which components to build and what options to apply
OPTION(INDI_BUILD_SERVER "Build INDI Server" ON)
OPTION(INDI_BUILD_CLIENT "Build INDI POSIX Client" ON)
OPTION(INDI_BUILD_DRIVERS "Build INDI Drivers, Tools, and Examples" ON)
OPTION(INDI_BUILD_COMMON "Build base classes for INDI 3rd Party Drivers" ON)
OPTION(INDI_BUILD_QT_CLIENT "Build INDI Qt Client" OFF)
OPTION(INDI_BUILD_UNITTESTS "Build INDI tests" OFF)
OPTION(INDI_BUILD_INTEGTESTS "Build INDI integration tests" OFF)
OPTION(INDI_FAST_BLOB "Build INDI with Fast BLOB support" ON)
OPTION(INDI_BUILD_SHARED "Build shared library" ON)
OPTION(INDI_BUILD_STATIC "Build static library" ON)
OPTION(INDI_BUILD_XISF "Build XISF support" ON)
OPTION(INDI_BUILD_EXAMPLES "Build INDI examples" ON)
OPTION(INDI_INSTALL_UDEV_RULES "Install INDI udev rules" ON)

# System provided or bundled libs
OPTION(INDI_SYSTEM_HIDAPILIB "Use system provided hidapilib" OFF)
OPTION(INDI_SYSTEM_HTTPLIB "Use system provided httplib" OFF)
OPTION(INDI_SYSTEM_JSONLIB "Use system provided json library" OFF)

if(UNIX AND NOT APPLE)
    OPTION(INDI_SHARED_MEMORY "Build INDI with support for UNIX protocol with shared memory" ON)
else()
    OPTION(INDI_SHARED_MEMORY "Build INDI with support for UNIX protocol with shared memory (require shm specific settings)" OFF)
endif()

OPTION(INDI_CALCULATE_MINMAX "Calculate and store image minimum and maximum values in FITS header" OFF)

set(CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)
check_symbol_exists(mremap sys/mman.h HAVE_MREMAP)

if(HAVE_MREMAP)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DHAVE_MREMAP")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHAVE_MREMAP")
endif()

# ##################################################################################################
# ########################################  Fast Blob  #############################################
# ##################################################################################################
if(INDI_FAST_BLOB)
    # Append ENCLEN attribute to outgoing BLOB elements to enable fast parsing by clients
    add_definitions(-DWITH_ENCLEN)
endif(INDI_FAST_BLOB)

# ##################################################################################################
# ###################################  UNIX protocol / SHM  ########################################
# ##################################################################################################
if(INDI_SHARED_MEMORY)
    if(APPLE)
        message(WARNING "Shared memory protocol require specific shared memory settings")
    else()
        add_definitions(-DENABLE_INDI_SHARED_MEMORY)
    endif()
endif()

# ##################################################################################################
# #####################################  Calculate Min/Max #########################################
# ##################################################################################################
if(INDI_CALCULATE_MINMAX)
    # Calculate Min/Max values to store them in FITS header
    add_definitions(-DWITH_MINMAX)
endif(INDI_CALCULATE_MINMAX)

# ##################################################################################################
# ####################################  Components  ################################################
# ##################################################################################################
set_package_properties(Nova PROPERTIES DESCRIPTION "A general purpose, double precision, Celestial Mechanics, Astrometry and Astrodynamics library" URL "http://libnova.sourceforge.net" TYPE REQUIRED PURPOSE "Provides INDI with astrodynamics library.")
set_package_properties(CFITSIO PROPERTIES DESCRIPTION "A library for reading and writing data files in FITS (Flexible Image Transport System) data format" URL "http://heasarc.gsfc.nasa.gov/fitsio/fitsio.html" TYPE REQUIRED PURPOSE "Provides INDI with FITS I/O support.")

# ##################################################################################################
# #########  Put runtime files together in PE/COFF platforms to enable in-tree execution  ##########
# ##################################################################################################
if (WIN32 OR CYGWIN)
    if (NOT DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY)
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
    endif ()
endif ()

# ##################################################################################################
# ####################################  Common Files  ##############################################
# ##################################################################################################
include_directories(libs)
install(FILES
    libs/indimacros.h
    DESTINATION
    ${INCLUDE_INSTALL_DIR}/libindi
    COMPONENT Devel
)

# ##################################################################################################
# ####################################  bundled libs  ##############################################
# ##################################################################################################
if(INDI_SYSTEM_HIDAPILIB)
    find_package(hidapi REQUIRED)
    set(SYSTEM_HIDAPILIB 1)
    set(HIDAPILIB hidapi::libusb)
    add_definitions(-D_USE_SYSTEM_HIDAPILIB)
    message(STATUS "Using system provided hidapilib version ${hidapi_VERSION_STR}")
else()
    set(SYSTEM_HIDAPILIB 0)
    set(HIDAPILIB "")
    message(STATUS "Using bundled hidapi")
endif(INDI_SYSTEM_HIDAPILIB)

if(INDI_SYSTEM_HTTPLIB)
    find_package(httplib REQUIRED)
    include_directories(${HTTPLIB_INCLUDE_DIR})
    set(SYSTEM_HTTPLIB 1)
    message(STATUS "Using system provided httplib version ${HTTPLIB_VERSION}")
else()
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/libs/httplib)
    set(SYSTEM_HTTPLIB 0)
    message(STATUS "Using bundled httplib")
endif(INDI_SYSTEM_HTTPLIB)

if(INDI_SYSTEM_JSONLIB)
    find_package(nlohmann_json REQUIRED)
    set(SYSTEM_JSONLIB 1)
    set(JSONLIB nlohmann_json::nlohmann_json)
    add_definitions(-D_USE_SYSTEM_JSONLIB)
    message(STATUS "Using system provided Niels Lohmann's json library")
else(INDI_SYSTEM_JSONLIB)
    set(SYSTEM_JSONLIB 0)
    set(JSONLIB "")
    message(STATUS "Using bundled json library")
endif(INDI_SYSTEM_JSONLIB)

# ###################################################################################################
#
# Component   : INDI Core
# Dependencies:
# Supported OS: All
#
# ##################################################################################################
add_subdirectory(libs/indicore)

# ##################################################################################################
#
# Component   : indidevice
# Dependencies: indicore
# Supported OS: All
#
# ##################################################################################################
add_subdirectory(libs/indidevice)

# ##################################################################################################
#
# Component   : indiabstractclient
# Dependencies: indicore, indidevice
# Supported OS: All
#
# ##################################################################################################
if(INDI_BUILD_CLIENT OR INDI_BUILD_QT_CLIENT)
    add_subdirectory(libs/indiabstractclient)
endif()

# ##################################################################################################
#
# Component   : INDI Server
# Dependencies: pthreads
# Supported OS: Linux, BSD, MacOS, Cygwin
#
# ##################################################################################################
if(INDI_BUILD_SERVER)
    add_subdirectory(indiserver)
endif()

# ##################################################################################################
#
# Component   : INDI Client
# Dependencies: zlib, cfitsio
# Supported OS: Linux, BSD, MacOS, Windows, Cygwin
# N.B. Windows support pending migration of networking code
#
# ##################################################################################################
if(INDI_BUILD_CLIENT AND NOT ANDROID)
    message(STATUS "Building INDI Client")
    add_subdirectory(libs/sockets)
    add_subdirectory(libs/indiclient)
endif(INDI_BUILD_CLIENT AND NOT ANDROID)

# ##################################################################################################
#
# Component   : INDI Qt Client
# Dependencies: Qt5/Qt6 Network, zlib, cfitsio, Qt5/Qt6 Core
# Supported OS: Linux, BSD, MacOS, Cygwin, Windows, Android
#
# ##################################################################################################
if(INDI_BUILD_QT_CLIENT)
    message(STATUS "Building INDI Qt Client")
    add_subdirectory(libs/indiclientqt)
endif(INDI_BUILD_QT_CLIENT)


# ##################################################################################################
#
# Component   : INDI Drivers, Tools, and Examples
# Dependencies: pthreads, usb1, zLib, cfitsio, nova, curl, jpeg (Linux Only)
# Supported OS: Linux, BSD, MacOS, Cygwin
# N.B. Webcam drivers only supported under Linux (Video4Linux2). Joystick support only under Linux
#
# ##################################################################################################


if(INDI_BUILD_DRIVERS OR INDI_BUILD_COMMON)
    # part required for both drivers supplied by INDI and for 3rd party drivers
    if(WIN32 OR ANDROID)
        message(WARNING "INDI drivers are only supported under Linux, BSD, MacOS, and Cygwin while current system is " ${CMAKE_SYSTEM_NAME})
    else(WIN32 OR ANDROID)
        # 1. Dependencies
        find_package(Threads REQUIRED)
        find_package(ZLIB REQUIRED)
        find_package(CFITSIO REQUIRED)
        find_package(Nova REQUIRED)
        find_package(USB1 REQUIRED)
        find_package(CURL REQUIRED)
        find_package(GSL REQUIRED)
        find_package(JPEG REQUIRED)
        find_library(M_LIB m)
        if(CYGWIN OR UNIX)
            find_package(Iconv REQUIRED)
        endif()

        if(CMAKE_VERSION VERSION_LESS 3.12.0)
            set(CURL ${CURL_LIBRARIES})
        else()
            set(CURL CURL::libcurl)
        endif()

        include_directories(${CFITSIO_INCLUDE_DIR})
        include_directories(${NOVA_INCLUDE_DIR})
        include_directories(${USB1_INCLUDE_DIRS})
        include_directories(${GSL_INCLUDE_DIRS})
        include_directories(${JPEG_INCLUDE_DIR})
        include_directories(${ZLIB_INCLUDE_DIR})
        include_directories(libs/indibase)
        include_directories(libs/indibase/timer)
        include_directories(libs/indiclient)
        include_directories(libs/indiabstractclient)
        include_directories(libs/indicore)
        include_directories(libs/indidevice)
        include_directories(libs/indidevice/property)
        include_directories(${CMAKE_CURRENT_BINARY_DIR}/libs/indicore)

        configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config-usb.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-usb.h)

        add_subdirectory(libs/eventloop)
        add_subdirectory(libs/dsp)
        add_subdirectory(libs/fpack)
        if(NOT SYSTEM_HIDAPILIB)
            add_subdirectory(libs/hid)
        endif(NOT SYSTEM_HIDAPILIB)

        # #################################################
        # ########## INDI Driver Library ##################
        # #################################################
        add_subdirectory(libs/indibase)

        # #################################################
        # ########## INDI Alignment Subsystem #############
        # #################################################
        add_subdirectory(libs/alignment)

        # drivers directly supplied by INDI
        if(INDI_BUILD_DRIVERS)
            # #################################################
            # ########## INDI Drivers #########################
            # #################################################
            add_subdirectory(drivers)
            install(FILES drivers.xml ${CMAKE_CURRENT_SOURCE_DIR}/drivers/focuser/indi_tcfs_sk.xml DESTINATION ${DATA_INSTALL_DIR})
        endif()

        # ####################################
        # ########### INDI TOOLS #############
        # ####################################
        add_subdirectory(tools)

        # ####################################
        # ########### EXamples ###############
        # ####################################
        if(INDI_BUILD_CLIENT)
            if(INDI_BUILD_EXAMPLES)
                add_subdirectory(examples)
            endif()
        else()
            message(WARNING "Skipping build of examples since INDI POSIX client is not built")
        endif()

        # ################################################################################
        configure_file(${CMAKE_CURRENT_SOURCE_DIR}/libindi.pc.cmake ${CMAKE_CURRENT_BINARY_DIR}/libindi.pc @ONLY)
        install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libindi.pc DESTINATION ${PKGCONFIG_INSTALL_PREFIX})

        # ##################################################################################################
        # ########################################  Tests  #################################################
        # ##################################################################################################
        if(DEFINED GTEST_ROOT)
            message(STATUS "Using GTEST from ${GTEST_ROOT}")
            add_subdirectory(${GTEST_ROOT}
                "${CMAKE_CURRENT_BINARY_DIR}/googletest" EXCLUDE_FROM_ALL)
            set(GTEST_FOUND true)
        else(DEFINED GTEST_ROOT)
            find_package(GTest)
        endif()

        find_package(GMock)

        if(GTEST_FOUND AND BUILD_TESTING)
            if(INDI_BUILD_UNITTESTS)
                message(STATUS "Building unit tests")
                add_subdirectory(test)
            else(INDI_BUILD_UNITTESTS)
                message(STATUS "Not building unit tests")
            endif(INDI_BUILD_UNITTESTS)

            if(INDI_BUILD_INTEGTESTS)
                message(STATUS "Building integration tests")
                add_subdirectory(integs)
            else(INDI_BUILD_INTEGTESTS)
                message(STATUS "Not building integration tests")
            endif(INDI_BUILD_INTEGTESTS)

        elseif(NOT BUILD_TESTING)
            message(STATUS "GTEST found, but tests disabled")
        else()
            message(STATUS "GTEST not found, not building tests")
        endif(GTEST_FOUND AND BUILD_TESTING)
    endif(WIN32 OR ANDROID)
endif(INDI_BUILD_DRIVERS OR INDI_BUILD_COMMON)

# ##################################################################################################
# ######################################  config.h  ################################################
# ##################################################################################################
# Generate config.h from template
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indiversion.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/indiversion.h)

if(MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-D_WINSOCK_DEPRECATED_NO_WARNINGS)
endif()

# Install common dev files for all except server
if(INDI_BUILD_DRIVERS OR INDI_BUILD_CLIENT OR INDI_BUILD_QT_CLIENT)
    install(FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/inicpp.h
        ${CMAKE_CURRENT_BINARY_DIR}/indiversion.h
        DESTINATION ${INCLUDE_INSTALL_DIR}/libindi COMPONENT Devel
    )
    if(NOT SYSTEM_HTTPLIB)
        install(FILES
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/httplib/httplib.h
            DESTINATION ${INCLUDE_INSTALL_DIR}/libindi COMPONENT Devel
        )
    endif(NOT SYSTEM_HTTPLIB)
    if(NOT SYSTEM_JSONLIB)
        install(FILES
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indijson.hpp
            DESTINATION ${INCLUDE_INSTALL_DIR}/libindi COMPONENT Devel
        )
    endif(NOT SYSTEM_JSONLIB)
endif(INDI_BUILD_DRIVERS OR INDI_BUILD_CLIENT OR INDI_BUILD_QT_CLIENT)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)

message(STATUS "The following components are going to be built:")

if(INDI_BUILD_SERVER)
    message(STATUS "## INDI Server")
endif()

if(INDI_BUILD_DRIVERS)
    message(STATUS "## INDI Drivers, Tools, and Examples")
endif()

if(INDI_BUILD_CLIENT)
    message(STATUS "## INDI Client")
endif()

if(INDI_BUILD_QT_CLIENT)
    message(STATUS "## INDI Qt Client")
endif()
