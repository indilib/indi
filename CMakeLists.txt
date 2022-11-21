cmake_minimum_required(VERSION 3.16)
PROJECT(libindi C CXX)

# As moc files are generated in the binary dir, tell CMake
# to always look for includes there:
set(CMAKE_INCLUDE_CURRENT_DIR ON)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include(GNUInstallDirs)
include(FeatureSummary)

if(ANDROID OR "${CMAKE_SYSTEM_NAME}" STREQUAL "Android")
    set(ANDROID ON)
    add_definitions(-DANDROID)
endif()

include(CMakeCommon)
include(CheckFunctionExists)

# Clang Format support
IF(UNIX OR APPLE)
    SET(FORMAT_CODE OFF CACHE BOOL "Enable Clang Format")

    IF(FORMAT_CODE MATCHES ON)
        FILE(GLOB_RECURSE ALL_SOURCE_FILES *.c *.cpp *.h)

        FOREACH(SOURCE_FILE ${ALL_SOURCE_FILES})
            STRING(FIND ${SOURCE_FILE} ${CMAKE_SOURCE_DIR} DIR_FOUND)

            IF(NOT ${DIR_FOUND} EQUAL 0)
                LIST(REMOVE_ITEM ALL_SOURCE_FILES ${SOURCE_FILE})
            ENDIF()
        ENDFOREACH()

        FIND_PROGRAM(CLANGFORMAT_EXE NAMES clang-format-5.0)

        IF(CLANGFORMAT_EXE)
            ADD_CUSTOM_TARGET(clang-format COMMAND ${CLANGFORMAT_EXE} -style=file -i ${ALL_SOURCE_FILES})
        ENDIF()
    ENDIF()
ENDIF()

# ####################################  INDI version  ################################################
# N.B. DO NOT Forget to update version also in indiapi.h
# Proper way is to use indiversion.h.cmake file but this would break make existing applications so let us stick to the old proven way
set(INDI_SOVERSION "1")
set(CMAKE_INDI_VERSION_MAJOR 1)
set(CMAKE_INDI_VERSION_MINOR 9)
set(CMAKE_INDI_VERSION_RELEASE 9)
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

IF(APPLE)
    set(CMAKE_SHARED_LINKER_FLAGS "-undefined dynamic_lookup")
ENDIF(APPLE)

# #################################  Install Directories  ###########################################
# # the following are directories where stuff will be installed to
set(INCLUDE_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/include/")
set(PKGCONFIG_INSTALL_PREFIX "${CMAKE_INSTALL_LIBDIR}/pkgconfig/")
set(UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")

set(PKG_CONFIG_LIBDIR ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR})

# ####################################  Build Options  ##############################################
# Select which components to build and what options to apply
OPTION(INDI_BUILD_SERVER "Build INDI Server" ON)
OPTION(INDI_BUILD_DRIVERS "Build INDI Drivers, Tools, and Examples" ON)
OPTION(INDI_BUILD_CLIENT "Build INDI POSIX Client" ON)
OPTION(INDI_BUILD_QT5_CLIENT "Build INDI Qt5 Client" OFF)
OPTION(INDI_BUILD_UNITTESTS "Build INDI tests" OFF)
OPTION(INDI_BUILD_INTEGTESTS "Build INDI integration tests" OFF)
OPTION(INDI_BUILD_WEBSOCKET "Build INDI with Websocket support" OFF)
OPTION(INDI_FAST_BLOB "Build INDI with Fast BLOB support" ON)

if(UNIX AND NOT APPLE)
    OPTION(INDI_SHARED_MEMORY "Build INDI with support for UNIX protocol with shared memory" ON)
else()
    OPTION(INDI_SHARED_MEMORY "Build INDI with support for UNIX protocol with shared memory (require shm specific settings)" OFF)
endif()

OPTION(INDI_CALCULATE_MINMAX "Calculate and store image minimum and maximum values in FITS header" OFF)

CHECK_FUNCTION_EXISTS(mremap HAVE_MREMAP)

if(HAVE_MREMAP)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DHAVE_MREMAP")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHAVE_MREMAP")
endif()

# ##################################################################################################
# ########################################  Fast Blob  #############################################
# ##################################################################################################
IF(INDI_FAST_BLOB)
    # Append ENCLEN attribute to outgoing BLOB elements to enable fast parsing by clients
    add_definitions(-DWITH_ENCLEN)
ENDIF(INDI_FAST_BLOB)

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
IF(INDI_CALCULATE_MINMAX)
    # Calculate Min/Max values to store them in FITS header
    add_definitions(-DWITH_MINMAX)
ENDIF(INDI_CALCULATE_MINMAX)

# ##################################################################################################
# ####################################  Components  ################################################
# ##################################################################################################
set_package_properties(Nova PROPERTIES DESCRIPTION "A general purpose, double precision, Celestial Mechanics, Astrometry and Astrodynamics library" URL "http://libnova.sourceforge.net" TYPE REQUIRED PURPOSE "Provides INDI with astrodynamics library.")
set_package_properties(CFITSIO PROPERTIES DESCRIPTION "A library for reading and writing data files in FITS (Flexible Image Transport System) data format" URL "http://heasarc.gsfc.nasa.gov/fitsio/fitsio.html" TYPE REQUIRED PURPOSE "Provides INDI with FITS I/O support.")

# ###################################################################################################
#
# Component   : INDI Server
# Dependencies: pthreads
# Supported OS: Linux, BSD, MacOS, Cygwin
#
# ################################################################################################
if(INDI_BUILD_SERVER)
    if(WIN32 OR ANDROID)
        message(WARNING "INDI Server is only supported under Linux, BSD, MacOS, and Cygwin while current system is " ${CMAKE_SYSTEM_NAME})
    else()
        # 1. Dependencies
        find_package(Threads REQUIRED)
        find_package(Libev REQUIRED)

        # 2. Includes
        include_directories(${CMAKE_CURRENT_SOURCE_DIR})

        # 3. Build
        SET(indiserver_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/indiserver.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/base64.c
            ${CMAKE_CURRENT_SOURCE_DIR}/sharedblob.c
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/lilxml.cpp)

        IF(UNITY_BUILD)
            ENABLE_UNITY_BUILD(indiserver indiserver_SRC 10 c)
        ENDIF()

        add_executable(indiserver ${indiserver_SRC})
        target_link_libraries(indiserver ${CMAKE_THREAD_LIBS_INIT})
        target_include_directories(indiserver SYSTEM PRIVATE ${LIBEV_INCLUDE_DIRS})
        target_include_directories(indiserver PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/libs/)
        target_link_libraries(indiserver ${LIBEV_LIBRARIES})
        install(TARGETS indiserver RUNTIME DESTINATION bin)
    endif(WIN32 OR ANDROID)
endif(INDI_BUILD_SERVER)

# ################################################################################################
#
# Component   : INDI Client
# Dependencies: zlib, cfitsio
# Supported OS: Linux, BSD, MacOS, Windows, Cygwin
# N.B. Windows support pending migration of networking code
# ################################################################################################
if(INDI_BUILD_CLIENT AND NOT ANDROID)
    # 1. Dependencies
    find_package(Threads REQUIRED)
    find_package(ZLIB REQUIRED)
    find_package(CFITSIO REQUIRED)

    # 2. Includes
    include_directories(${CMAKE_CURRENT_BINARY_DIR})
    include_directories(${CMAKE_CURRENT_SOURCE_DIR})
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/libs)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/property)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/timer)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/thread)
    include_directories(${ZLIB_INCLUDE_DIR})
    include_directories(${CFITSIO_INCLUDE_DIR})

    # 3. Build
    SET(indiclient_C_SRC
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/lilxml.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/userio.c
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indiuserio.c
        ${CMAKE_CURRENT_SOURCE_DIR}/base64.c
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indicom.c)

    SET(indiclient_CXX_SRC
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/parentdevice.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/basedevice.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/baseclient.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indibase.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/abstractbaseclient.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/watchdeviceproperty.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/property/indiproperty.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/property/indiproperties.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/property/indipropertybasic.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/property/indipropertytext.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/property/indipropertynumber.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/property/indipropertyswitch.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/property/indipropertylight.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/property/indipropertyblob.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/property/indipropertyview_client.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indistandardproperty.cpp)

    if(INDI_SHARED_MEMORY)
        SET(indiclient_C_SRC ${indiclient_C_SRC}
            ${CMAKE_CURRENT_SOURCE_DIR}/sharedblob.c)

        SET(indiclient_CXX_SRC ${indiclient_CXX_SRC}
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/sharedblob_parse.cpp)
    endif()

    IF(UNITY_BUILD)
        ENABLE_UNITY_BUILD(indiclient_c indiclient_C_SRC 10 c)
        ENABLE_UNITY_BUILD(indiclient_cxx indiclient_CXX_SRC 10 cpp)
    ENDIF()

    add_library(indiclient STATIC ${indiclient_C_SRC} ${indiclient_CXX_SRC})

    add_subdirectory(libs/sockets)
    target_link_libraries(indiclient sockets ${ZLIB_LIBRARY})

    if(NOT CYGWIN AND NOT WIN32)
        set_target_properties(indiclient PROPERTIES COMPILE_FLAGS "-fPIC")
    endif(NOT CYGWIN AND NOT WIN32)

    target_link_libraries(indiclient ${CMAKE_THREAD_LIBS_INIT})
    install(TARGETS indiclient ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/baseclient.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/abstractbaseclient.h
        DESTINATION ${INCLUDE_INSTALL_DIR}/libindi COMPONENT Devel)
endif(INDI_BUILD_CLIENT AND NOT ANDROID)

# ################################################################################################
#
# Component   : INDI Qt5 Client
# Dependencies: Qt5Network, zlib, cfitsio, Qt5Core
# Supported OS: Linux, BSD, MacOS, Cygwin, Windows, Android
#
# ################################################################################################
if(INDI_BUILD_QT5_CLIENT)
    set(QT_ANDROID "" CACHE path "Qt Android path")

    # Tell CMake to run moc when necessary:
    set(CMAKE_AUTOMOC ON)

    # 1. Dependencies
    if(ANDROID)
        set(Qt5Core_DIR "${QT_ANDROID}/lib/cmake/Qt5Core")
        include(${QT_ANDROID}/lib/cmake/Qt5/Qt5Config.cmake)
        include(${QT_ANDROID}/lib/cmake/Qt5Core/Qt5CoreConfig.cmake)
        include(${QT_ANDROID}/lib/cmake/Qt5Network/Qt5NetworkConfig.cmake)

        if(NOT CFITSIO_DIR)
            message(FATAL_ERROR CFITSIO_DIR must be set)
        else()
            set(CFITSIO_INCLUDE_DIR ${CFITSIO_DIR})
            set(CFITSIO_LIBRARIES ${CFITSIO_DIR}/libcfitsio.a)
        endif()
    else()
        find_package(Qt5Core REQUIRED)
        find_package(Qt5Network REQUIRED)
        find_package(ZLIB REQUIRED)
        find_package(CFITSIO REQUIRED)
    endif()

    # 2. Includes
    include_directories(${CMAKE_CURRENT_BINARY_DIR})
    include_directories(${CMAKE_CURRENT_SOURCE_DIR})
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/libs)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase)
    include_directories(${CFITSIO_INCLUDE_DIR})

    # 3. Build
    message(STATUS "Building INDI Client with Qt5 support")
    SET(indiclientqt_C_SRC
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/lilxml.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/userio.c
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indiuserio.c
        ${CMAKE_CURRENT_SOURCE_DIR}/base64.c
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indicom.c)

    SET(indiclientqt_CXX_SRC
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/libastro.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/parentdevice.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/basedevice.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/baseclientqt.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indibase.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/abstractbaseclient.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/property/indiproperty.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/property/indiproperties.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/property/indipropertybasic.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/property/indipropertytext.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/property/indipropertynumber.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/property/indipropertyswitch.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/property/indipropertylight.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/property/indipropertyblob.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/property/indipropertyview_client.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indistandardproperty.cpp)

    if(INDI_SHARED_MEMORY)
        SET(indiclientqt_C_SRC ${indiclientqt_C_SRC}
            ${CMAKE_CURRENT_SOURCE_DIR}/sharedblob.c)

        SET(indiclientqt_CXX_SRC ${indiclientqt_CXX_SRC}
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/sharedblob_parse.cpp)
    endif()

    IF(UNITY_BUILD)
        ENABLE_UNITY_BUILD(indiclientqt_c indiclientqt_C_SRC 10 c)
        ENABLE_UNITY_BUILD(indiclientqt_cxx indiclientqt_CXX_SRC 10 cpp)
    ENDIF()

    add_library(indiclientqt STATIC ${indiclientqt_C_SRC} ${indiclientqt_CXX_SRC})

    if(NOT CYGWIN AND NOT WIN32)
        set_target_properties(indiclientqt PROPERTIES COMPILE_FLAGS "-fPIC")
    endif(NOT CYGWIN AND NOT WIN32)

    target_link_libraries(indiclientqt Qt5::Network)

    if(WIN32 OR ANDROID)
        install(TARGETS indiclientqt ARCHIVE DESTINATION lib)
    else(WIN32 OR ANDROID)
        install(TARGETS indiclientqt ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    endif(WIN32 OR ANDROID)

    install(FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/baseclientqt.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/abstractbaseclient.h
        DESTINATION ${INCLUDE_INSTALL_DIR}/libindi COMPONENT Devel)
endif(INDI_BUILD_QT5_CLIENT)

# ###################################################################################################
#
# Component   : Websocket
# Dependencies: libwebsocketpp, libboost, asio
# Supported OS: Linux, BSD, MacOS, Cygwin
#
# ################################################################################################
if(INDI_BUILD_WEBSOCKET)
    find_package(websocketpp REQUIRED)
    find_package(Boost COMPONENTS system thread)
    include_directories(${Boost_INCLUDE_DIRS})
    add_definitions(-DHAVE_WEBSOCKET)
    SET(HAVE_WEBSOCKET 1)
endif(INDI_BUILD_WEBSOCKET)

# ################################################################################################
#
# Component   : INDI Drivers, Tools, and Examples
# Dependencies: pthreads, usb1, zLib, cfitsio, nova, curl, jpeg (Linux Only)
# Supported OS: Linux, BSD, MacOS, Cygwin
# N.B. Webcam drivers only supported under Linux (Video4Linux2). Joystick support only under Linux
#
# ################################################################################################
if(INDI_BUILD_DRIVERS)
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

        # Math Library
        FIND_LIBRARY(M_LIB m)

        # 2. Includes
        include_directories(${CMAKE_CURRENT_BINARY_DIR})
        include_directories(${CMAKE_CURRENT_SOURCE_DIR})
        include_directories(${CMAKE_CURRENT_SOURCE_DIR}/libs)
        include_directories(${CMAKE_CURRENT_SOURCE_DIR}/libs/dsp)
        include_directories(${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase)
        include_directories(${CMAKE_CURRENT_SOURCE_DIR}/libs/stream)
        include_directories(${CFITSIO_INCLUDE_DIR})
        include_directories(${NOVA_INCLUDE_DIR})
        include_directories(${USB1_INCLUDE_DIRS})
        include_directories(${GSL_INCLUDE_DIRS})
        include_directories(${JPEG_INCLUDE_DIR})

        IF(${CMAKE_SYSTEM_NAME} MATCHES "Linux|FreeBSD")
            include_directories(${CMAKE_CURRENT_SOURCE_DIR}/libs/webcam)
        ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "Linux|FreeBSD")

        configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config-usb.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-usb.h)

        if(CMAKE_VERSION VERSION_LESS 3.12.0)
            set(CURL ${CURL_LIBRARIES})
        else()
            set(CURL CURL::libcurl)
        endif()

        # ##################################################################################################
        # #######################################  Sources  ################################################
        # ##################################################################################################
        IF(APPLE)
            SET(hidapi_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/hid_mac.c)
        ELSEIF(WIN32)
            SET(hidapi_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/hid_win.c)
        ELSE()
            SET(hidapi_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/hid_libusb.c)
        ENDIF()

        IF(UNIX)
            find_package(OggTheora)

            IF(OGGTHEORA_FOUND)
                INCLUDE_DIRECTORIES(${THEORA_INCLUDE_DIRS})
                SET(HAVE_THEORA 1)
                SET(theorarecorder_CXX_SRC ${CMAKE_CURRENT_SOURCE_DIR}/libs/stream/recorder/theorarecorder.cpp)
            ENDIF(OGGTHEORA_FOUND)

            SET(libstream_CXX_SRC
                ${CMAKE_CURRENT_SOURCE_DIR}/libs/stream/streammanager.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/libs/stream/fpsmeter.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/libs/stream/gammalut16.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/libs/stream/recorder/recorderinterface.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/libs/stream/recorder/recordermanager.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/libs/stream/recorder/serrecorder.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/libs/stream/encoder/encodermanager.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/libs/stream/encoder/encoderinterface.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/libs/stream/encoder/rawencoder.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/libs/stream/encoder/mjpegencoder.cpp
                ${theorarecorder_CXX_SRC}
            )
            SET(libstream_C_SRC
                ${CMAKE_CURRENT_SOURCE_DIR}/libs/stream/jpegutils.c
                ${CMAKE_CURRENT_SOURCE_DIR}/libs/stream/ccvt_c2.c
                ${CMAKE_CURRENT_SOURCE_DIR}/libs/stream/ccvt_misc.c)

            IF(UNITY_BUILD)
                ENABLE_UNITY_BUILD(libstream libstream_C_SRC 10 c)
                ENABLE_UNITY_BUILD(libstream libstream_CXX_SRC 10 cpp)
            ENDIF()

            IF(${CMAKE_SYSTEM_NAME} MATCHES "Linux|FreeBSD")
                SET(libwebcam_C_SRC
                    ${CMAKE_CURRENT_SOURCE_DIR}/libs/webcam/v4l2_colorspace.c)
                SET(libwebcam_CXX_SRC
                    ${CMAKE_CURRENT_SOURCE_DIR}/libs/webcam/v4l2_base.cpp
                    ${CMAKE_CURRENT_SOURCE_DIR}/libs/webcam/v4l2_decode/v4l2_decode.cpp
                    ${CMAKE_CURRENT_SOURCE_DIR}/libs/webcam/v4l2_decode/v4l2_builtin_decoder.cpp
                )

                IF(UNITY_BUILD)
                    ENABLE_UNITY_BUILD(libwebcam libwebcam_C_SRC 10 c)
                    ENABLE_UNITY_BUILD(libwebcam libwebcam_CXX_SRC 10 cpp)
                ENDIF(UNITY_BUILD)
            ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "Linux|FreeBSD")
        ENDIF(UNIX)

        SET(libdsp_C_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/dsp/fits.c
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/dsp/file.c
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/dsp/buffer.c
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/dsp/convert.c
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/dsp/fft.c
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/dsp/filters.c
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/dsp/signals.c
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/dsp/convolution.c
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/dsp/stats.c
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/dsp/stream.c
        )

        set(fpack_C_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/fpack/fpack.c
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/fpack/fpackutil.c
        )

        SET(indidriver_C_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/indidriver.c
            ${CMAKE_CURRENT_SOURCE_DIR}/indidriverio.c
            ${CMAKE_CURRENT_SOURCE_DIR}/indidrivermain.c
            ${CMAKE_CURRENT_SOURCE_DIR}/eventloop.c
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/lilxml.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/userio.c
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indiuserio.c
            ${CMAKE_CURRENT_SOURCE_DIR}/sharedblob.c
            ${CMAKE_CURRENT_SOURCE_DIR}/base64.c
        )

        SET(indidriver_CXX_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/libastro.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/parentdevice.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/basedevice.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/defaultdevice.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/watchdeviceproperty.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/sharedblob_parse.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/property/indiproperty.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/property/indiproperties.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/property/indipropertybasic.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/property/indipropertytext.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/property/indipropertynumber.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/property/indipropertyswitch.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/property/indipropertylight.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/property/indipropertyblob.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/property/indipropertyview_driver.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/timer/inditimer.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/timer/indielapsedtimer.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/thread/indisinglethreadpool.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indiutility.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indiccd.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indiccdchip.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indisensorinterface.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indicorrelator.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indidetector.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indispectrograph.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indireceiver.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/inditelescope.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indifilterwheel.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indifocuserinterface.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indiweatherinterface.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indifocuser.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indirotator.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indiusbdevice.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indiguiderinterface.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indifilterinterface.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indirotatorinterface.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indidome.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indigps.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indiweather.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indidustcapinterface.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indilightboxinterface.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indilogger.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indicontroller.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indistandardproperty.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/connectionplugins/connectioninterface.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/connectionplugins/connectionserial.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/connectionplugins/connectiontcp.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/dsp/manager.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/dsp/dspinterface.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/dsp/transforms.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/dsp/convolution.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/pid/pid.cpp
            # ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/connectionplugins/ttybase.cpp
        )

        IF(UNITY_BUILD)
            ENABLE_UNITY_BUILD(indidriver_c indidriver_C_SRC libdsp_C_SRC fpack_C_SRC 10 c)
            ENABLE_UNITY_BUILD(indidriver_cxx indidriver_CXX_SRC 10 cpp)
        ENDIF()

        SET(indidriver_C_SRC
            ${indidriver_C_SRC}
            ${libdsp_C_SRC}
            ${fpack_C_SRC}
            ${libstream_C_SRC}
            ${hidapi_SRCS}
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indicom.c)

        SET(indidriver_CXX_SRC
            ${indidriver_CXX_SRC}
            ${libstream_CXX_SRC})

        # #################################################
        # ######### INDI Default Driver Library ###########
        # #################################################

        find_package(FFTW3 REQUIRED)

        if(CYGWIN)
            # # For Cygwin we only build static library
            add_definitions(-U__STRICT_ANSI__)
            
            find_package(Iconv REQUIRED)
            add_library(indidriver STATIC ${indidriver_C_SRC} ${indidriver_CXX_SRC})

            install(TARGETS indidriver ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
        else()
            # # Static indidriver Library
            add_library(indidriverstatic STATIC ${indidriver_C_SRC} ${indidriver_CXX_SRC} ${libwebcam_C_SRC} ${libwebcam_CXX_SRC})
            set_target_properties(indidriverstatic PROPERTIES COMPILE_FLAGS "-fPIC")
            target_compile_definitions(indidriverstatic PRIVATE "-DHAVE_LIBNOVA")
            set_target_properties(indidriverstatic PROPERTIES VERSION ${CMAKE_INDI_VERSION_STRING} SOVERSION ${INDI_SOVERSION} OUTPUT_NAME indidriver)
            target_link_libraries(indidriverstatic ${USB1_LIBRARIES} ${NOVA_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${CFITSIO_LIBRARIES} ${M_LIB} ${ZLIB_LIBRARY} ${JPEG_LIBRARY} ${FFTW3_LIBRARIES})

            IF(OGGTHEORA_FOUND)
                target_link_libraries(indidriverstatic ${OGGTHEORA_LIBRARIES} ${THEORA_LIBRARIES})
            ENDIF()

            IF(HAVE_WEBSOCKET)
                target_link_libraries(indidriverstatic ${Boost_LIBRARIES})
            ENDIF()

            install(TARGETS indidriverstatic ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

            # # Dynamic indidriver Library
            add_library(indidriver SHARED ${indidriver_C_SRC} ${indidriver_CXX_SRC} ${libwebcam_C_SRC} ${libwebcam_CXX_SRC})
            set_target_properties(indidriver PROPERTIES COMPILE_FLAGS "-fPIC")

            # IF (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
            # target_link_libraries(indidriver -lpthread)
            # ENDIF ()
            install(TARGETS indidriver LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
            set(PKG_CONFIG_LIBS "${PKG_CONFIG_LIBS} -lindidriver -lindiAlignmentDriver")
        endif(CYGWIN)

        set_target_properties(indidriver PROPERTIES VERSION ${CMAKE_INDI_VERSION_STRING} SOVERSION ${INDI_SOVERSION} OUTPUT_NAME indidriver)
        target_link_libraries(indidriver ${ICONV_LIBRARIES} ${USB1_LIBRARIES} ${NOVA_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${CFITSIO_LIBRARIES} ${M_LIB} ${ZLIB_LIBRARY} ${JPEG_LIBRARY} ${FFTW3_LIBRARIES})
        target_compile_definitions(indidriver PRIVATE "-DHAVE_LIBNOVA")

        IF(OGGTHEORA_FOUND)
            target_link_libraries(indidriver ${OGGTHEORA_LIBRARIES} ${THEORA_LIBRARIES})
        ENDIF()

        IF(HAVE_WEBSOCKET)
            target_link_libraries(indidriver ${Boost_LIBRARIES})
        ENDIF()

        IF(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
            # FreeBSD needs to find the correct GNU iconv library.
            find_package(Iconv REQUIRED)

            # The indidriver library is defined earlier in the file before the
            # find has been run. Add an explicit link to avoid runtime errors.
            target_link_libraries(indidriver ${ICONV_LIBRARIES})
        ENDIF(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")

        CHECK_FUNCTION_EXISTS(mremap HAVE_MREMAP)

        # #################################################
        # ########## INDI Alignment Subsystem #############
        # #################################################
        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libs/alignment)

        # #################################################
        # ########## INDI Drivers #########################
        # #################################################
        add_subdirectory(drivers)
        install(FILES drivers.xml ${CMAKE_CURRENT_SOURCE_DIR}/drivers/focuser/indi_tcfs_sk.xml DESTINATION ${DATA_INSTALL_DIR})

        # ####################################
        # ########### INDI TOOLS #############
        # ####################################

        # ########## getINDI ##############
        SET(indi_get_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/eventloop.c
            ${CMAKE_CURRENT_SOURCE_DIR}/base64.c
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/getINDIproperty.c
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/lilxml.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/userio.c
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indiuserio.c
            ${CMAKE_CURRENT_SOURCE_DIR}/sharedblob.c)

        IF(UNITY_BUILD)
            ENABLE_UNITY_BUILD(indi_get indi_get_SRC 10 c)
        ENDIF()

        SET(indi_get_SRC ${indi_get_SRC}
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/libastro.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indicom.c)

        add_executable(indi_getprop ${indi_get_SRC})

        target_link_libraries(indi_getprop ${NOVA_LIBRARIES} ${M_LIB} ${ZLIB_LIBRARY})

        install(TARGETS indi_getprop RUNTIME DESTINATION bin)

        # ################################################################################

        # ########## setINDI ##############
        SET(indi_set_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/eventloop.c
            ${CMAKE_CURRENT_SOURCE_DIR}/base64.c
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/setINDIproperty.c
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/lilxml.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/userio.c
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indiuserio.c
            ${CMAKE_CURRENT_SOURCE_DIR}/sharedblob.c)

        IF(UNITY_BUILD)
            ENABLE_UNITY_BUILD(indi_set indi_set_SRC 10 c)
        ENDIF()

        SET(indi_set_SRC ${indi_set_SRC}
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/libastro.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indicom.c)

        add_executable(indi_setprop ${indi_set_SRC})

        target_link_libraries(indi_setprop ${NOVA_LIBRARIES} ${M_LIB} ${ZLIB_LIBRARY})

        install(TARGETS indi_setprop RUNTIME DESTINATION bin)

        # ################################################################################

        # ########## evalINDI ##############
        SET(indi_eval_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/eventloop.c
            ${CMAKE_CURRENT_SOURCE_DIR}/base64.c
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/compiler.c
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/evalINDI.c
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/lilxml.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/userio.c
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indiuserio.c
            ${CMAKE_CURRENT_SOURCE_DIR}/sharedblob.c)

        IF(UNITY_BUILD)
            ENABLE_UNITY_BUILD(indi_eval indi_eval_SRC 10 c)
        ENDIF()

        SET(indi_eval_SRC ${indi_eval_SRC}
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/libastro.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indicom.c)

        add_executable(indi_eval ${indi_eval_SRC})

        target_link_libraries(indi_eval ${NOVA_LIBRARIES} ${M_LIB} ${ZLIB_LIBRARY})

        install(TARGETS indi_eval RUNTIME DESTINATION bin)

        # ########## HID Test ##############
        SET(indi_hid_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/hidtest.cpp
        )

        IF(UNITY_BUILD)
            ENABLE_UNITY_BUILD(indi_hid indi_hid_SRC 10 cpp)
        ENDIF()

        if(APPLE)
            set(LIBS "-framework IOKit -framework CoreFoundation")
        elseif(CYGWIN)
            set(LIBS ${ICONV_LIBRARIES})
        endif(APPLE)

        SET(indi_hid_SRC ${indi_hid_SRC} ${hidapi_SRCS})

        add_executable(indi_hid_test ${indi_hid_SRC})

        target_link_libraries(indi_hid_test ${ICONV_LIBRARIES} ${USB1_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${LIBS})

        install(TARGETS indi_hid_test RUNTIME DESTINATION bin)

        # ################################################################################
        # # Build Examples
        IF(INDI_BUILD_CLIENT)
            add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/examples)
        ELSE()
            message(WARNING "Skipping build of examples since INDI POSIX client is not built")
        ENDIF()

        # ################################################################################
        configure_file(${CMAKE_CURRENT_SOURCE_DIR}/libindi.pc.cmake ${CMAKE_CURRENT_BINARY_DIR}/libindi.pc @ONLY)
        install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libindi.pc DESTINATION ${PKGCONFIG_INSTALL_PREFIX})

        if(UNIX)
            INSTALL(FILES
                ${CMAKE_CURRENT_SOURCE_DIR}/libs/stream/streammanager.h
                ${CMAKE_CURRENT_SOURCE_DIR}/libs/stream/fpsmeter.h
                ${CMAKE_CURRENT_SOURCE_DIR}/libs/stream/uniquequeue.h
                ${CMAKE_CURRENT_SOURCE_DIR}/libs/stream/gammalut16.h
                ${CMAKE_CURRENT_SOURCE_DIR}/libs/stream/jpegutils.h
                ${CMAKE_CURRENT_SOURCE_DIR}/libs/stream/ccvt.h
                ${CMAKE_CURRENT_SOURCE_DIR}/libs/stream/ccvt_types.h
                DESTINATION ${INCLUDE_INSTALL_DIR}/libindi/stream COMPONENT Devel)
            INSTALL(FILES
                ${CMAKE_CURRENT_SOURCE_DIR}/libs/stream/encoder/encodermanager.h
                ${CMAKE_CURRENT_SOURCE_DIR}/libs/stream/encoder/encoderinterface.h
                ${CMAKE_CURRENT_SOURCE_DIR}/libs/stream/encoder/rawencoder.h
                ${CMAKE_CURRENT_SOURCE_DIR}/libs/stream/encoder/mjpegencoder.h
                DESTINATION ${INCLUDE_INSTALL_DIR}/libindi/stream/encoder COMPONENT Devel)
            INSTALL(FILES
                ${CMAKE_CURRENT_SOURCE_DIR}/libs/stream/recorder/recordermanager.h
                ${CMAKE_CURRENT_SOURCE_DIR}/libs/stream/recorder/recorderinterface.h
                ${CMAKE_CURRENT_SOURCE_DIR}/libs/stream/recorder/serrecorder.h
                DESTINATION ${INCLUDE_INSTALL_DIR}/libindi/stream/recorder COMPONENT Devel)

            if(${CMAKE_SYSTEM_NAME} MATCHES "Linux|FreeBSD")
                INSTALL(FILES
                    ${CMAKE_CURRENT_SOURCE_DIR}/libs/webcam/v4l2_decode/v4l2_decode.h
                    ${CMAKE_CURRENT_SOURCE_DIR}/libs/webcam/v4l2_decode/v4l2_builtin_decoder.h
                    ${CMAKE_CURRENT_SOURCE_DIR}/libs/webcam/v4l2_colorspace.h
                    DESTINATION ${INCLUDE_INSTALL_DIR}/libindi COMPONENT Devel)
            ENDIF()
        ENDIF()

        # ##################################################################################################
        # ########################################  Tests  #################################################
        # ##################################################################################################
        IF(DEFINED GTEST_ROOT)
            MESSAGE(STATUS "Using GTEST from ${GTEST_ROOT}")
            add_subdirectory(${GTEST_ROOT}
                "${CMAKE_CURRENT_BINARY_DIR}/googletest" EXCLUDE_FROM_ALL)
            set(GTEST_FOUND true)
        ELSE(DEFINED GTEST_ROOT)
            find_package(GTest)
        ENDIF()

        find_package(GMock)

        IF(GTEST_FOUND)
            IF(INDI_BUILD_UNITTESTS)
                MESSAGE(STATUS "Building unit tests")
                ADD_SUBDIRECTORY(test)
            ELSE(INDI_BUILD_UNITTESTS)
                MESSAGE(STATUS "Not building unit tests")
            ENDIF(INDI_BUILD_UNITTESTS)

            IF(INDI_BUILD_INTEGTESTS)
                MESSAGE(STATUS "Building integration tests")
                ADD_SUBDIRECTORY(integs)
            ELSE(INDI_BUILD_INTEGTESTS)
                MESSAGE(STATUS "Not building integration tests")
            ENDIF(INDI_BUILD_INTEGTESTS)

        ELSE()
            MESSAGE(STATUS "GTEST not found, not building tests")
        ENDIF(GTEST_FOUND)
    endif(WIN32 OR ANDROID)
endif(INDI_BUILD_DRIVERS)

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
if(INDI_BUILD_DRIVERS OR INDI_BUILD_CLIENT OR INDI_BUILD_QT5_CLIENT)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indiversion.h DESTINATION ${INCLUDE_INSTALL_DIR}/libindi COMPONENT Devel)
    install(FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/indiapi.h
        ${CMAKE_CURRENT_SOURCE_DIR}/indidevapi.h
        ${CMAKE_CURRENT_SOURCE_DIR}/base64.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/lilxml.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indililxml.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/libastro.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indicom.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/json.h
        ${CMAKE_CURRENT_SOURCE_DIR}/eventloop.h
        ${CMAKE_CURRENT_SOURCE_DIR}/indidriver.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/dsp/dsp.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/dsp/fits_extensions.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/dsp/fits.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/dsp/sdfits.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/dsp/fitsidi.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/pid/pid.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indibase.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indibasetypes.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/basedevice.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/parentdevice.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/defaultdevice.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indiccd.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indiccdchip.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indisensorinterface.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indicorrelator.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indidetector.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indispectrograph.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indireceiver.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indifilterwheel.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indifocuserinterface.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indiweatherinterface.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indifocuser.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indirotator.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/inditelescope.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indiguiderinterface.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indifilterinterface.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indirotatorinterface.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/property/indiproperty.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/property/indiproperties.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/property/indipropertybasic.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/property/indipropertytext.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/property/indipropertynumber.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/property/indipropertyswitch.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/property/indipropertylight.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/property/indipropertyblob.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/property/indipropertyview.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/property/indiwidgetview.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/property/indiwidgettraits.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/timer/inditimer.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/timer/indielapsedtimer.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/thread/indisinglethreadpool.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indiutility.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indimacros.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indistandardproperty.h
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

    if(HAVE_WEBSOCKET)
        install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indiwsserver.h
            DESTINATION ${INCLUDE_INSTALL_DIR}/libindi COMPONENT Devel)
    endif()

    install(FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/connectionplugins/connectioninterface.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/connectionplugins/connectionserial.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/connectionplugins/connectiontcp.h
        DESTINATION ${INCLUDE_INSTALL_DIR}/libindi/connectionplugins COMPONENT Devel)

    install(FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/dsp/manager.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/dsp/dspinterface.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/dsp/transforms.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/dsp/convolution.h
        DESTINATION ${INCLUDE_INSTALL_DIR}/libindi/dsp COMPONENT Devel)
endif(INDI_BUILD_DRIVERS OR INDI_BUILD_CLIENT OR INDI_BUILD_QT5_CLIENT)

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

if(INDI_BUILD_QT5_CLIENT)
    message(STATUS "## INDI Qt5 Client")
endif()
