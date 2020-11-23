cmake_minimum_required(VERSION 3.0)
project(libindi C CXX)

# As moc files are generated in the binary dir, tell CMake to always look for
# includes there:
set(CMAKE_INCLUDE_CURRENT_DIR ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include(GNUInstallDirs)
include(FeatureSummary)
if(ANDROID OR "${CMAKE_SYSTEM_NAME}" STREQUAL "Android")
    set(ANDROID ON)
    add_definitions(-DANDROID)
endif()

include(CMakeCommon)

# Clang Format support
if(UNIX OR APPLE)
    set(FORMAT_CODE
        OFF
        CACHE BOOL "Enable Clang Format")
    if(FORMAT_CODE MATCHES ON)
        file(GLOB_RECURSE ALL_SOURCE_FILES *.c *.cpp *.h)

        foreach(SOURCE_FILE ${ALL_SOURCE_FILES})
            string(FIND ${SOURCE_FILE} ${CMAKE_SOURCE_DIR} DIR_FOUND)
            if(NOT ${DIR_FOUND} EQUAL 0)
                list(REMOVE_ITEM ALL_SOURCE_FILES ${SOURCE_FILE})
            endif()
        endforeach()

        find_program(CLANGFORMAT_EXE NAMES clang-format-5.0)
        if(CLANGFORMAT_EXE)
            add_custom_target(
                clang-format COMMAND ${CLANGFORMAT_EXE} -style=file -i
                                     ${ALL_SOURCE_FILES})
        endif()
    endif()
endif()

#####################################  INDI version  ################################################
# N.B. DO NOT Forget to update version also in indiapi.h Proper way is to use
# indiversion.h.cmake file but this would break make existing applications so
# let us stick to the old proven way

set(INDI_SOVERSION "1")
set(CMAKE_INDI_VERSION_MAJOR 1)
set(CMAKE_INDI_VERSION_MINOR 8)
set(CMAKE_INDI_VERSION_RELEASE 7)
set(CMAKE_INDI_VERSION_STRING
    "${CMAKE_INDI_VERSION_MAJOR}.${CMAKE_INDI_VERSION_MINOR}.${CMAKE_INDI_VERSION_RELEASE}"
)
set(INDI_VERSION
    ${CMAKE_INDI_VERSION_MAJOR}.${CMAKE_INDI_VERSION_MINOR}.${CMAKE_INDI_VERSION_RELEASE}
)

execute_process(
    COMMAND git describe --tags
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_TAG
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE GIT_TAG_RESULT)

if(NOT ${GIT_TAG_RESULT} EQUAL 0)
    set(GIT_TAG "${CMAKE_INDI_VERSION_STRING}-tgz")
endif()

add_definitions(-DGIT_TAG_STRING=\"${GIT_TAG}\")

########################################  Paths  ###################################################

set(DATA_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/share/indi/")
set(BIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/bin")
set(INCLUDE_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/include")

if(APPLE)
    set(CMAKE_SHARED_LINKER_FLAGS "-undefined dynamic_lookup")
endif(APPLE)

##################################  Install Directories  ###########################################
# the following are directories where stuff will be installed to
set(INCLUDE_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/include/")
set(PKGCONFIG_INSTALL_PREFIX "${CMAKE_INSTALL_LIBDIR}/pkgconfig/")
set(UDEVRULES_INSTALL_DIR
    "/lib/udev/rules.d"
    CACHE STRING "Base directory for udev rules")

set(PKG_CONFIG_LIBDIR ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR})

#####################################  Build Options  ##############################################
# Select which components to build and what options to apply

option(INDI_BUILD_SERVER "Build INDI Server" ON)
option(INDI_BUILD_DRIVERS "Build INDI Drivers, Tools, and Examples" ON)
option(INDI_BUILD_CLIENT "Build INDI POSIX Client" ON)
option(INDI_BUILD_QT5_CLIENT "Build INDI Qt5 Client" OFF)
option(INDI_BUILD_UNITTESTS "Build INDI tests" OFF)
option(INDI_BUILD_WEBSOCKET "Build INDI with Websocket support" OFF)
option(INDI_FAST_BLOB "Build INDI with Fast BLOB support" ON)
option(INDI_CALCULATE_MINMAX
       "Calculate and store image minimum and maximum values in FITS header"
       OFF)

###################################################################################################
#########################################  Fast Blob  #############################################
###################################################################################################

if(INDI_FAST_BLOB)
    # Append ENCLEN attribute to outgoing BLOB elements to enable fast parsing
    # by clients
    add_definitions(-DWITH_ENCLEN)
endif(INDI_FAST_BLOB)

###################################################################################################
######################################  Calculate Min/Max #########################################
###################################################################################################
if(INDI_CALCULATE_MINMAX)
    # Calculate Min/Max values to store them in FITS header
    add_definitions(-DWITH_MINMAX)
endif(INDI_CALCULATE_MINMAX)

###################################################################################################
#####################################  Components  ################################################
###################################################################################################
set_package_properties(
    Nova PROPERTIES
    DESCRIPTION
        "A general purpose, double precision, Celestial Mechanics, Astrometry and Astrodynamics library"
    URL "http://libnova.sourceforge.net"
    TYPE REQUIRED
    PURPOSE "Provides INDI with astrodynamics library.")
set_package_properties(
    CFITSIO PROPERTIES
    DESCRIPTION
        "A library for reading and writing data files in FITS (Flexible Image Transport System) data format"
    URL "http://heasarc.gsfc.nasa.gov/fitsio/fitsio.html"
    TYPE REQUIRED
    PURPOSE "Provides INDI with FITS I/O support.")

####################################################################################################
#
# Component   : INDI Server
# Dependencies: pthreads
# Supported OS: Linux, BSD, MacOS, Cygwin
#
#################################################################################################
if(INDI_BUILD_SERVER)
    if(WIN32 OR ANDROID)
        message(
            WARNING
                "INDI Server is only supported under Linux, BSD, MacOS, and Cygwin while current system is "
                ${CMAKE_SYSTEM_NAME})
    else()

        # 1. Dependencies
        find_package(Threads REQUIRED)
        # 1. Includes
        include_directories(${CMAKE_CURRENT_SOURCE_DIR})
        # 1. Build
        set(indiserver_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/indiserver.c
            ${CMAKE_CURRENT_SOURCE_DIR}/fq.c
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/lilxml.c)

        if(UNITY_BUILD)
            enable_unity_build(indiserver indiserver_SRC 10 c)
        endif()

        add_executable(indiserver ${indiserver_SRC})
        target_link_libraries(indiserver ${CMAKE_THREAD_LIBS_INIT})
        install(TARGETS indiserver RUNTIME DESTINATION bin)
    endif(WIN32 OR ANDROID)
endif(INDI_BUILD_SERVER)

#################################################################################################
#
# Component   : INDI Client
# Dependencies: zlib, cfitsio
# Supported OS: Linux, BSD, MacOS, Windows, Cygwin
# N.B. Windows support pending migration of networking code
#################################################################################################
if(INDI_BUILD_CLIENT AND NOT ANDROID)
    # 1. Dependencies
    find_package(Threads REQUIRED)
    find_package(ZLIB REQUIRED)
    find_package(CFITSIO REQUIRED)
    # 1. Includes
    include_directories(${CMAKE_CURRENT_BINARY_DIR})
    include_directories(${CMAKE_CURRENT_SOURCE_DIR})
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/libs)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase)
    include_directories(${ZLIB_INCLUDE_DIR})
    include_directories(${CFITSIO_INCLUDE_DIR})
    # 1. Build

    set(indiclient_C_SRC ${CMAKE_CURRENT_SOURCE_DIR}/libs/lilxml.c
                         ${CMAKE_CURRENT_SOURCE_DIR}/base64.c)

    set(indiclient_CXX_SRC
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/basedevice.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/baseclient.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indiproperty.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indistandardproperty.cpp)

    if(UNITY_BUILD)
        enable_unity_build(indiclient_c indiclient_C_SRC 10 c)
        enable_unity_build(indiclient_cxx indiclient_CXX_SRC 10 cpp)
    endif()

    set(indiclient_C_SRC
        ${indiclient_C_SRC} ${CMAKE_CURRENT_SOURCE_DIR}/libs/libastro.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indicom.c)

    add_library(indiclient STATIC ${indiclient_C_SRC} ${indiclient_CXX_SRC})
    if(NOT CYGWIN AND NOT WIN32)
        set_target_properties(indiclient PROPERTIES COMPILE_FLAGS "-fPIC")
    endif(NOT CYGWIN AND NOT WIN32)
    target_link_libraries(indiclient ${CMAKE_THREAD_LIBS_INIT})
    install(TARGETS indiclient ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(
        FILES ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/baseclient.h
        DESTINATION ${INCLUDE_INSTALL_DIR}/libindi
        COMPONENT Devel)
endif(INDI_BUILD_CLIENT AND NOT ANDROID)

#################################################################################################
#
# Component   : INDI Qt5 Client
# Dependencies: Qt5Network, zlib, cfitsio, Qt5Core
# Supported OS: Linux, BSD, MacOS, Cygwin, Windows, Android
#
#################################################################################################
if(INDI_BUILD_QT5_CLIENT)
    set(QT_ANDROID
        ""
        CACHE path "Qt Android path")
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
        find_package(Qt5Network REQUIRED)
        find_package(ZLIB REQUIRED)
        find_package(CFITSIO REQUIRED)
    endif()
    # 1. Includes
    include_directories(${CMAKE_CURRENT_BINARY_DIR})
    include_directories(${CMAKE_CURRENT_SOURCE_DIR})
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/libs)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase)
    include_directories(${CFITSIO_INCLUDE_DIR})
    # 1. Build
    message(STATUS "Building INDI Client with Qt5 support")
    set(indiclientqt_C_SRC ${CMAKE_CURRENT_SOURCE_DIR}/libs/lilxml.c
                           ${CMAKE_CURRENT_SOURCE_DIR}/base64.c)
    set(indiclientqt_CXX_SRC
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/basedevice.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/baseclientqt.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indiproperty.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indistandardproperty.cpp)
    if(UNITY_BUILD)
        enable_unity_build(indiclientqt_c indiclientqt_C_SRC 10 c)
        enable_unity_build(indiclientqt_cxx indiclientqt_CXX_SRC 10 cpp)
    endif()

    set(indiclientqt_C_SRC
        ${indiclientqt_C_SRC} ${CMAKE_CURRENT_SOURCE_DIR}/libs/libastro.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/indicom.c)

    add_library(indiclientqt STATIC ${indiclientqt_C_SRC}
                                    ${indiclientqt_CXX_SRC})
    if(NOT CYGWIN AND NOT WIN32)
        set_target_properties(indiclientqt PROPERTIES COMPILE_FLAGS "-fPIC")
    endif(NOT CYGWIN AND NOT WIN32)
    target_link_libraries(indiclientqt Qt5::Network)
    if(WIN32 OR ANDROID)
        install(TARGETS indiclientqt ARCHIVE DESTINATION lib)
    else(WIN32 OR ANDROID)
        install(TARGETS indiclientqt
                ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    endif(WIN32 OR ANDROID)
    install(
        FILES ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/baseclientqt.h
        DESTINATION ${INCLUDE_INSTALL_DIR}/libindi
        COMPONENT Devel)
endif(INDI_BUILD_QT5_CLIENT)

####################################################################################################
#
# Component   : Websocket
# Dependencies: libwebsocketpp, libboost, asio Supported
# OS: Linux, BSD, MacOS, Cygwin
#
#################################################################################################
if(INDI_BUILD_WEBSOCKET)
    find_package(websocketpp REQUIRED)
    find_package(Boost COMPONENTS system thread)
    include_directories(${Boost_INCLUDE_DIRS})
    add_definitions(-DHAVE_WEBSOCKET)
    set(HAVE_WEBSOCKET 1)
endif(INDI_BUILD_WEBSOCKET)

#################################################################################################
#
# Component   : INDI Drivers, Tools, and Examples
# Dependencies: pthreads, usb1, zLib, cfitsio, nova, curl, jpeg (Linux Only)
# Supported OS: Linux, BSD, MacOS, Cygwin
# N.B. Webcam drivers only supported under Linux (Video4Linux2). Joystick support only under Linux
#
#################################################################################################
if(INDI_BUILD_DRIVERS)
    if(WIN32 OR ANDROID)
        message(
            WARNING
                "INDI drivers are only supported under Linux, BSD, MacOS, and Cygwin while current system is "
                ${CMAKE_SYSTEM_NAME})
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
        find_library(M_LIB m)
        # 1. Includes
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
        if(${CMAKE_SYSTEM_NAME} MATCHES "Linux|FreeBSD")
            include_directories(${CMAKE_CURRENT_SOURCE_DIR}/libs/webcam)
        endif(${CMAKE_SYSTEM_NAME} MATCHES "Linux|FreeBSD")

        configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config-usb.h.cmake
                       ${CMAKE_CURRENT_BINARY_DIR}/config-usb.h)

###################################################################################################
########################################  Sources  ################################################
###################################################################################################
        if(APPLE)
            set(hidapi_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/hid_mac.c)
        elseif(WIN32)
            set(hidapi_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/hid_win.c)
        else()
            set(hidapi_SRCS
                ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/hid_libusb.c)
        endif()

        if(UNIX)
            find_package(OggTheora)
            if(OGGTHEORA_FOUND)
                include_directories(${THEORA_INCLUDE_DIRS})
                set(HAVE_THEORA 1)
                set(theorarecorder_CXX_SRC
                    ${CMAKE_CURRENT_SOURCE_DIR}/libs/stream/recorder/theorarecorder.cpp
                )
            endif(OGGTHEORA_FOUND)

            set(libstream_CXX_SRC
                ${CMAKE_CURRENT_SOURCE_DIR}/libs/stream/streammanager.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/libs/stream/fpsmeter.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/libs/stream/recorder/recorderinterface.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/libs/stream/recorder/recordermanager.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/libs/stream/recorder/serrecorder.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/libs/stream/encoder/encodermanager.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/libs/stream/encoder/encoderinterface.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/libs/stream/encoder/rawencoder.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/libs/stream/encoder/mjpegencoder.cpp
                ${theorarecorder_CXX_SRC})
            set(libstream_C_SRC
                ${CMAKE_CURRENT_SOURCE_DIR}/libs/stream/jpegutils.c
                ${CMAKE_CURRENT_SOURCE_DIR}/libs/stream/ccvt_c2.c
                ${CMAKE_CURRENT_SOURCE_DIR}/libs/stream/ccvt_misc.c)
            if(UNITY_BUILD)
                enable_unity_build(libstream libstream_C_SRC 10 c)
                enable_unity_build(libstream libstream_CXX_SRC 10 cpp)
            endif()

            if(${CMAKE_SYSTEM_NAME} MATCHES "Linux|FreeBSD")
                set(libwebcam_C_SRC
                    ${CMAKE_CURRENT_SOURCE_DIR}/libs/webcam/v4l2_colorspace.c)
                set(libwebcam_CXX_SRC
                    ${CMAKE_CURRENT_SOURCE_DIR}/libs/webcam/v4l2_base.cpp
                    ${CMAKE_CURRENT_SOURCE_DIR}/libs/webcam/v4l2_decode/v4l2_decode.cpp
                    ${CMAKE_CURRENT_SOURCE_DIR}/libs/webcam/v4l2_decode/v4l2_builtin_decoder.cpp
                )
                if(UNITY_BUILD)
                    enable_unity_build(libwebcam libwebcam_C_SRC 10 c)
                    enable_unity_build(libwebcam libwebcam_CXX_SRC 10 cpp)
                endif(UNITY_BUILD)
            endif(${CMAKE_SYSTEM_NAME} MATCHES "Linux|FreeBSD")
        endif(UNIX)

        set(libdsp_C_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/dsp/file.c
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/dsp/buffer.c
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/dsp/convert.c
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/dsp/fft.c
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/dsp/filters.c
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/dsp/signals.c
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/dsp/convolution.c
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/dsp/stats.c
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/dsp/stream.c)

        set(fpack_C_SRC ${CMAKE_CURRENT_SOURCE_DIR}/libs/fpack/fpack.c
                        ${CMAKE_CURRENT_SOURCE_DIR}/libs/fpack/fpackutil.c)

        set(indidriver_C_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/indidriver.c
            ${CMAKE_CURRENT_SOURCE_DIR}/indidrivermain.c
            ${CMAKE_CURRENT_SOURCE_DIR}/eventloop.c
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/lilxml.c
            ${CMAKE_CURRENT_SOURCE_DIR}/base64.c)

        set(indidriver_CXX_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/basedevice.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/defaultdevice.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indiproperty.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indiccd.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indiccdchip.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indisensorinterface.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indicorrelator.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indidetector.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indispectrograph.cpp
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
            # ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/connectionplugins/ttybase.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/dsp/manager.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/dsp/dspinterface.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/dsp/transforms.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/dsp/convolution.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/pid/pid.cpp)

        if(UNITY_BUILD)
            enable_unity_build(indidriver_c indidriver_C_SRC libdsp_C_SRC
                               fpack_C_SRC 10 c)
            enable_unity_build(indidriver_cxx indidriver_CXX_SRC 10 cpp)
        endif()

        set(indidriver_C_SRC
            ${indidriver_C_SRC} ${libdsp_C_SRC} ${fpack_C_SRC}
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/libastro.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indicom.c)

##################################################
########## INDI Default Driver Library ###########
##################################################

        if(CYGWIN)
            # For Cygwin we only build static library
            add_definitions(-U__STRICT_ANSI__)
            find_package(FFTW3 REQUIRED)
            find_package(Iconv REQUIRED)
            add_library(
                indidriver STATIC
                ${indidriver_C_SRC}
                ${indidriver_CXX_SRC}
                ${libstream_C_SRC}
                ${libstream_CXX_SRC}
                ${hidapi_SRCS}
                ${libdsp_C_SRC}
                ${fpack_C_SRC})
            target_compile_definitions(indidriver PRIVATE "-DHAVE_LIBNOVA")
            set_target_properties(
                indidriver
                PROPERTIES VERSION ${CMAKE_INDI_VERSION_STRING}
                           SOVERSION ${INDI_SOVERSION}
                           OUTPUT_NAME indidriver)
            target_link_libraries(
                indidriver
                ${ICONV_LIBRARIES}
                ${USB1_LIBRARIES}
                ${NOVA_LIBRARIES}
                ${CMAKE_THREAD_LIBS_INIT}
                ${CFITSIO_LIBRARIES}
                ${M_LIB}
                ${ZLIB_LIBRARY}
                ${JPEG_LIBRARY}
                ${FFTW3_LIBRARIES})
            if(OGGTHEORA_FOUND)
                target_link_libraries(indidriver ${OGGTHEORA_LIBRARIES}
                                      ${THEORA_LIBRARIES})
            endif()
            if(HAVE_WEBSOCKET)
                target_link_libraries(indidriver ${Boost_LIBRARIES})
            endif()
            install(TARGETS indidriver
                    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
        else()
            # Static indidriver Library
            find_package(FFTW3 REQUIRED)
            add_library(
                indidriverstatic STATIC
                ${indidriver_C_SRC}
                ${indidriver_CXX_SRC}
                ${libstream_C_SRC}
                ${libstream_CXX_SRC}
                ${libwebcam_C_SRC}
                ${libwebcam_CXX_SRC}
                ${hidapi_SRCS}
                ${libdsp_C_SRC}
                ${fpack_C_SRC})
            set_target_properties(indidriverstatic PROPERTIES COMPILE_FLAGS
                                                              "-fPIC")
            target_compile_definitions(indidriverstatic
                                       PRIVATE "-DHAVE_LIBNOVA")
            set_target_properties(
                indidriverstatic
                PROPERTIES VERSION ${CMAKE_INDI_VERSION_STRING}
                           SOVERSION ${INDI_SOVERSION}
                           OUTPUT_NAME indidriver)
            target_link_libraries(
                indidriverstatic
                ${USB1_LIBRARIES}
                ${NOVA_LIBRARIES}
                ${CMAKE_THREAD_LIBS_INIT}
                ${CFITSIO_LIBRARIES}
                ${M_LIB}
                ${ZLIB_LIBRARY}
                ${JPEG_LIBRARY}
                ${FFTW3_LIBRARIES})
            if(OGGTHEORA_FOUND)
                target_link_libraries(indidriverstatic ${OGGTHEORA_LIBRARIES}
                                      ${THEORA_LIBRARIES})
            endif()
            if(HAVE_WEBSOCKET)
                target_link_libraries(indidriverstatic ${Boost_LIBRARIES})
            endif()
            install(TARGETS indidriverstatic
                    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
            # Dynamic indidriver Library
            find_package(FFTW3 REQUIRED)
            add_library(
                indidriver SHARED
                ${indidriver_C_SRC}
                ${indidriver_CXX_SRC}
                ${libstream_C_SRC}
                ${libstream_CXX_SRC}
                ${libwebcam_C_SRC}
                ${libwebcam_CXX_SRC}
                ${hidapi_SRCS}
                ${libdsp_C_SRC}
                ${fpack_C_SRC})
            set_target_properties(indidriver PROPERTIES COMPILE_FLAGS "-fPIC")
            target_compile_definitions(indidriver PRIVATE "-DHAVE_LIBNOVA")
            set_target_properties(
                indidriver
                PROPERTIES VERSION ${CMAKE_INDI_VERSION_STRING}
                           SOVERSION ${INDI_SOVERSION}
                           OUTPUT_NAME indidriver)
            target_link_libraries(
                indidriver
                ${ICONV_LIBRARIES}
                ${USB1_LIBRARIES}
                ${NOVA_LIBRARIES}
                ${CMAKE_THREAD_LIBS_INIT}
                ${CFITSIO_LIBRARIES}
                ${M_LIB}
                ${ZLIB_LIBRARY}
                ${JPEG_LIBRARY}
                ${FFTW3_LIBRARIES})
            if(OGGTHEORA_FOUND)
                target_link_libraries(indidriver ${OGGTHEORA_LIBRARIES}
                                      ${THEORA_LIBRARIES})
            endif()
            if(HAVE_WEBSOCKET)
                target_link_libraries(indidriver ${Boost_LIBRARIES})
            endif()
            # IF (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
            # target_link_libraries(indidriver -lpthread) ENDIF ()
            install(TARGETS indidriver
                    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
            set(PKG_CONFIG_LIBS
                "${PKG_CONFIG_LIBS} -lindidriver -lindiAlignmentDriver")
        endif(CYGWIN)

        if(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
            # FreeBSD needs to find the correct GNU iconv library.
            find_package(Iconv REQUIRED)
            # The indidriver library is defined earlier in the file before the
            # find has been run. Add an explicit link to avoid runtime errors.
            target_link_libraries(indidriver ${ICONV_LIBRARIES})
        endif(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")

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
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/agent/group.cpp)

        add_executable(indi_imager_agent ${imager_SRCS})
        target_link_libraries(indi_imager_agent indidriver indiclient)
        install(TARGETS indi_imager_agent RUNTIME DESTINATION bin)

#################################################################################

#####################################
########## TELESCOPE GROUP ##########
#####################################

####################################################################################################
#
# Component   : LX200 Library
# Dependencies: pthreads
# Supported OS: Linux, BSD, MacOS, Cygwin
#
#################################################################################################
        set(lx200library_SRCS
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/lx200driver.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/lx200telescope.cpp)

        if(UNITY_BUILD)
            enable_unity_build(lx200library lx200library_SRCS 10 cpp)
        endif()

        add_library(indilx200 SHARED ${lx200library_SRCS})
        if(NOT CYGWIN AND NOT WIN32)
            set_target_properties(indilx200 PROPERTIES COMPILE_FLAGS "-fPIC")
        endif(NOT CYGWIN AND NOT WIN32)
        target_compile_definitions(indilx200 PRIVATE "-DHAVE_LIBNOVA")
        set_target_properties(
            indilx200
            PROPERTIES VERSION ${CMAKE_INDI_VERSION_STRING}
                       SOVERSION ${INDI_SOVERSION}
                       OUTPUT_NAME indilx200)

        install(TARGETS indilx200 LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
        install(
            FILES ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/lx200telescope.h
                  ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/lx200driver.h
            DESTINATION ${INCLUDE_INSTALL_DIR}/libindi/mounts
            COMPONENT Devel)
#################################################################################

########### LX200 Basic #############
        set(lx200basic_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/lx200driver.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/lx200basic.cpp)

        if(UNITY_BUILD)
            enable_unity_build(lx200basic lx200basic_SRC 10 cpp)
        endif()

        add_executable(indi_lx200basic ${lx200basic_SRC})

        target_link_libraries(indi_lx200basic indidriver)

        install(TARGETS indi_lx200basic RUNTIME DESTINATION bin)

#################################################################################

########### LX200 TeenAstro #############
        set(lx200_TeenAstro_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/lx200driver.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/lx200_TeenAstro.cpp)

        if(UNITY_BUILD)
            enable_unity_build(lx200_TeenAstro lx200_TeenAstro_SRC 10 cpp)
        endif()

        add_executable(indi_lx200_TeenAstro ${lx200_TeenAstro_SRC})

        target_link_libraries(indi_lx200_TeenAstro indidriver)

        install(TARGETS indi_lx200_TeenAstro RUNTIME DESTINATION bin)

#################################################################################

########### LX200 Generic ###########
        set(lx200generic_SRCS
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/lx200driver.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/lx200autostar.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/lx200_16.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/lx200gps.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/lx200generic.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/lx200telescope.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/lx200classic.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/lx200apdriver.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/lx200gemini.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/lx200zeq25.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/lx200gotonova.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/lx200pulsar2.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/lx200ap_experimentaldriver.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/lx200ap_experimental.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/lx200ap.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/lx200ap_gtocp2.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/lx200fs2.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/lx200ss2000pc.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/lx200_OnStep.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/lx200_10micron.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/ioptronHC8406.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/eq500x.cpp)

        if(UNITY_BUILD)
            enable_unity_build(lx200generic lx200generic_SRCS 10 cpp)
        endif()

        add_executable(indi_lx200generic ${lx200generic_SRCS})
        if(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
            target_compile_definitions(indi_lx200generic
                                       PRIVATE -D_XOPEN_SOURCE=700)
        else()
            target_compile_definitions(
                indi_lx200generic PRIVATE -D_XOPEN_SOURCE=600
                                          -D_POSIX_C_SOURCE=200809L)
        endif()
        target_link_libraries(indi_lx200generic indidriver)

        install(TARGETS indi_lx200generic RUNTIME DESTINATION bin)

        file(
            WRITE ${CMAKE_CURRENT_BINARY_DIR}/make_lx200generic_symlink.cmake
            "exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200classic)\n
exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200autostar)\n
exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200_16)\n
exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200gps)\n
exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200ap_experimental)\n
exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200ap_gtocp2)\n
exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200ap)\n
exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200gemini)\n
exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200zeq25)\n
exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200gotonova)\n
exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200pulsar2)\n
exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200fs2)\n
exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200ss2000pc)\n
exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200_OnStep)\n
exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_lx200_10micron)\n
exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_ioptronHC8406)\n
exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink indi_lx200generic \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_eq500x_telescope)\n
")
        set_target_properties(
            indi_lx200generic
            PROPERTIES
                POST_INSTALL_SCRIPT
                ${CMAKE_CURRENT_BINARY_DIR}/make_lx200generic_symlink.cmake)
#################################################################################

########### Celestron GPS ############
        set(celestrongps_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/celestrondriver.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/celestrongps.cpp)

        add_executable(indi_celestron_gps ${celestrongps_SRC})

        target_link_libraries(indi_celestron_gps indidriver)

        install(TARGETS indi_celestron_gps RUNTIME DESTINATION bin)
#################################################################################

########### Rainbow Mount driver ##########
        set(rainbow_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/rainbow.cpp)

        add_executable(indi_rainbow_telescope ${rainbow_SRC})

        target_link_libraries(indi_rainbow_telescope indidriver)

        install(TARGETS indi_rainbow_telescope RUNTIME DESTINATION bin)

########### Takahashi Temma ##########
        set(temma_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/temmadriver.cpp)

        add_executable(indi_temma_telescope ${temma_SRC})

        target_link_libraries(indi_temma_telescope indidriver AlignmentDriver)

        install(TARGETS indi_temma_telescope RUNTIME DESTINATION bin)
#################################################################################

########### Paramount ##########
        set(paramount_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/paramount.cpp)

        add_executable(indi_paramount_telescope ${paramount_SRC})

        target_link_libraries(indi_paramount_telescope indidriver)

        install(TARGETS indi_paramount_telescope RUNTIME DESTINATION bin)
#################################################################################

########### Syncscan ###############
        set(synscan_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/synscandriver.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/synscanmount.cpp)

        add_executable(indi_synscan_telescope ${synscan_SRC})
        target_link_libraries(indi_synscan_telescope indidriver AlignmentDriver)

        install(TARGETS indi_synscan_telescope RUNTIME DESTINATION bin)

########### Syncscan Legacy ###############
        set(synscanlegacy_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/synscandriverlegacy.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/synscanmountlegacy.cpp
        )

        add_executable(indi_synscanlegacy_telescope ${synscanlegacy_SRC})
        target_link_libraries(indi_synscanlegacy_telescope indidriver
                              AlignmentDriver)

        install(TARGETS indi_synscanlegacy_telescope RUNTIME DESTINATION bin)

########### Sky Commander ###############
        set(skycommander_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/skycommander.cpp)

        add_executable(indi_skycommander_telescope ${skycommander_SRC})
        target_link_libraries(indi_skycommander_telescope indidriver)

        install(TARGETS indi_skycommander_telescope RUNTIME DESTINATION bin)

########### Generic Digital Setting Circle ###############
        set(dsc_SRC ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/dsc.cpp)

        add_executable(indi_dsc_telescope ${dsc_SRC})
        target_link_libraries(indi_dsc_telescope indidriver AlignmentDriver)

        install(TARGETS indi_dsc_telescope RUNTIME DESTINATION bin)

########### IEQ Pro Legacy #############
        set(ieqlegacy_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/ieqprolegacydriver.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/ieqprolegacy.cpp)

        if(UNITY_BUILD)
            enable_unity_build(ieqlegacy ieqlegacy_SRC 10 cpp)
        endif()

        add_executable(indi_ieqlegacy_telescope ${ieqlegacy_SRC})

        target_link_libraries(indi_ieqlegacy_telescope indidriver)

        install(TARGETS indi_ieqlegacy_telescope RUNTIME DESTINATION bin)

########### IEQ Pro #############
        set(ieq_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/ieqdriverbase.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/ieqpro.cpp)

        if(UNITY_BUILD)
            enable_unity_build(ieq ieq_SRC 10 cpp)
        endif()

        add_executable(indi_ieq_telescope ${ieq_SRC})

        target_link_libraries(indi_ieq_telescope indidriver)

        install(TARGETS indi_ieq_telescope RUNTIME DESTINATION bin)

########### IOptronV3 / CEM120 #############
        set(ioptronv3_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/ioptronv3driver.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/ioptronv3.cpp)

        if(UNITY_BUILD)
            enable_unity_build(ioptronv3 ioptronv3_SRC 10 cpp)
        endif()

        add_executable(indi_ioptronv3_telescope ${ioptronv3_SRC})

        target_link_libraries(indi_ioptronv3_telescope indidriver)

        install(TARGETS indi_ioptronv3_telescope RUNTIME DESTINATION bin)

########### Explore Scientific PMC8 #############
        set(pmc8_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/pmc8driver.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/pmc8.cpp)

        add_executable(indi_pmc8_telescope ${pmc8_SRC})

        target_link_libraries(indi_pmc8_telescope indidriver)

        install(TARGETS indi_pmc8_telescope RUNTIME DESTINATION bin)

########### Telescope Simulator ##############
        set(telescopesimulator_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/telescope_simulator.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/scopesim_helper.cpp)

        add_executable(indi_simulator_telescope ${telescopesimulator_SRC})
        target_link_libraries(indi_simulator_telescope indidriver)
        install(TARGETS indi_simulator_telescope RUNTIME DESTINATION bin)

########### Telescope Scripting Gateway ##############
        set(telescopescript_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/telescope/telescope_script.cpp)

        add_executable(indi_script_telescope ${telescopescript_SRC})
        target_link_libraries(indi_script_telescope indidriver)
        install(TARGETS indi_script_telescope RUNTIME DESTINATION bin)

################ Detector Simulator ################

        set(spectrographsimulator_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/spectrograph/spectrograph_simulator.cpp
        )

        add_executable(indi_simulator_spectrograph ${spectrographsimulator_SRC})
        target_link_libraries(indi_simulator_spectrograph indidriver)
        install(TARGETS indi_simulator_spectrograph RUNTIME DESTINATION bin)

########### CCD Simulator ##############
        set(ccdsimulator_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/ccd/ccd_simulator.cpp)

        add_executable(indi_simulator_ccd ${ccdsimulator_SRC})
        target_link_libraries(indi_simulator_ccd indidriver)
        install(TARGETS indi_simulator_ccd RUNTIME DESTINATION bin)

########### Guide Simulator ##############
        set(guidesimulator_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/ccd/guide_simulator.cpp)

        add_executable(indi_simulator_guide ${guidesimulator_SRC})
        target_link_libraries(indi_simulator_guide indidriver)
        install(TARGETS indi_simulator_guide RUNTIME DESTINATION bin)

#####################################
########## FOCUSER GROUP ############
#####################################

#################################################################################

################ Focuser Simulator ################

        set(focussimulator_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/focuser/focus_simulator.cpp)

        add_executable(indi_simulator_focus ${focussimulator_SRC})
        target_link_libraries(indi_simulator_focus indidriver)
        install(TARGETS indi_simulator_focus RUNTIME DESTINATION bin)

################ Robo Focuser ################

        set(robofocus_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/focuser/robofocus.cpp)

        add_executable(indi_robo_focus ${robofocus_SRC})
        target_link_libraries(indi_robo_focus indidriver)
        install(TARGETS indi_robo_focus RUNTIME DESTINATION bin)

################ Robo Focuser ################

        set(fcusb_SRC ${CMAKE_CURRENT_SOURCE_DIR}/drivers/focuser/fcusb.cpp)

        add_executable(indi_fcusb_focus ${fcusb_SRC})
        target_link_libraries(indi_fcusb_focus indidriver)
        install(TARGETS indi_fcusb_focus RUNTIME DESTINATION bin)

################ Rigelsys NFocus Focuser ################

        set(nfocus_SRC ${CMAKE_CURRENT_SOURCE_DIR}/drivers/focuser/nfocus.cpp)

        add_executable(indi_nfocus ${nfocus_SRC})
        target_link_libraries(indi_nfocus indidriver)
        install(TARGETS indi_nfocus RUNTIME DESTINATION bin)

################ Rigelsys NStep Focuser ################

        set(nstep_SRC ${CMAKE_CURRENT_SOURCE_DIR}/drivers/focuser/nstep.cpp)

        add_executable(indi_nstep_focus ${nstep_SRC})
        target_link_libraries(indi_nstep_focus indidriver)
        install(TARGETS indi_nstep_focus RUNTIME DESTINATION bin)

################ PlaneWave EFA ################

        set(efa_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/focuser/planewave_efa.cpp)

        add_executable(indi_efa_focus ${efa_SRC})
        target_link_libraries(indi_efa_focus indidriver)
        install(TARGETS indi_efa_focus RUNTIME DESTINATION bin)

################ Celestron SCT Focuser ################

        set(celestronsct_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/focuser/celestronauxpacket.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/focuser/celestron.cpp)

        add_executable(indi_celestron_sct_focus ${celestronsct_SRC})
        target_link_libraries(indi_celestron_sct_focus indidriver)
        install(TARGETS indi_celestron_sct_focus RUNTIME DESTINATION bin)

################ AAF2 Focuser ################

        set(aaf2_SRC ${CMAKE_CURRENT_SOURCE_DIR}/drivers/focuser/aaf2.cpp)

        add_executable(indi_aaf2_focus ${aaf2_SRC})
        target_link_libraries(indi_aaf2_focus indidriver)
        install(TARGETS indi_aaf2_focus RUNTIME DESTINATION bin)

################ Moonlite Focuser ################

        set(moonlite_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/focuser/moonlite.cpp)

        add_executable(indi_moonlite_focus ${moonlite_SRC})
        target_link_libraries(indi_moonlite_focus indidriver)
        install(TARGETS indi_moonlite_focus RUNTIME DESTINATION bin)

################ Moonlite DRO Dual Focuser ################

        set(moonlitedro_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/focuser/moonlite_dro.cpp)

        add_executable(indi_moonlitedro_focus ${moonlitedro_SRC})
        target_link_libraries(indi_moonlitedro_focus indidriver)
        install(TARGETS indi_moonlitedro_focus RUNTIME DESTINATION bin)

################ MyFocuserPro2 Focuser ################

        set(myfocuserpro2_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/focuser/myfocuserpro2.cpp)

        add_executable(indi_myfocuserpro2_focus ${myfocuserpro2_SRC})
        target_link_libraries(indi_myfocuserpro2_focus indidriver)
        install(TARGETS indi_myfocuserpro2_focus RUNTIME DESTINATION bin)

################ OnFocus Focuser ################

        set(onfocus_SRC ${CMAKE_CURRENT_SOURCE_DIR}/drivers/focuser/onfocus.cpp)

        add_executable(indi_onfocus_focus ${onfocus_SRC})
        target_link_libraries(indi_onfocus_focus indidriver)
        install(TARGETS indi_onfocus_focus RUNTIME DESTINATION bin)

################ Sesto Senso Focuser ################
        set(sesto_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/focuser/sestosenso.cpp)

        add_executable(indi_sestosenso_focus ${sesto_SRC})
        target_link_libraries(indi_sestosenso_focus indidriver)
        install(TARGETS indi_sestosenso_focus RUNTIME DESTINATION bin)

################ Sesto Senso 2 Focuser ################
        set(sesto2_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/focuser/sestosenso2.cpp)

        add_executable(indi_sestosenso2_focus ${sesto2_SRC})
        target_link_libraries(indi_sestosenso2_focus indidriver)
        install(TARGETS indi_sestosenso2_focus RUNTIME DESTINATION bin)

################ Televue FocusMaster ################
        # SET(focusmaster_SRC
        # ${CMAKE_CURRENT_SOURCE_DIR}/drivers/focuser/focusmaster.cpp)

        # add_executable(indi_focusmaster_focus ${focusmaster_SRC})
        # target_link_libraries(indi_focusmaster_focus indidriver)
        # install(TARGETS indi_focusmaster_focus RUNTIME DESTINATION bin)

########### Lakeside ###########
        set(indilakeside_SRCS
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/focuser/lakeside.cpp)

        add_executable(indi_lakeside_focus ${indilakeside_SRCS})
        if(NOT "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang"
           AND NOT "${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
            set_target_properties(
                indi_lakeside_focus PROPERTIES COMPILE_FLAGS
                                               "-Wno-format-overflow")
        endif()
        target_link_libraries(indi_lakeside_focus indidriver)
        install(TARGETS indi_lakeside_focus RUNTIME DESTINATION bin)

################ Pegasus DMFC Focuser ################
        set(pegasus_SRC ${CMAKE_CURRENT_SOURCE_DIR}/drivers/focuser/dmfc.cpp)

        add_executable(indi_dmfc_focus ${pegasus_SRC})
        target_link_libraries(indi_dmfc_focus indidriver)
        install(TARGETS indi_dmfc_focus RUNTIME DESTINATION bin)
################## USB Focus V3 ##################

        set(usbfocusv3_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/focuser/usbfocusv3.cpp)

        add_executable(indi_usbfocusv3_focus ${usbfocusv3_SRC})
        target_link_libraries(indi_usbfocusv3_focus indidriver)
        install(TARGETS indi_usbfocusv3_focus RUNTIME DESTINATION bin)

################ Microtouch Focuser ################

        set(microtouch_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/focuser/microtouch.cpp)

        add_executable(indi_microtouch_focus ${microtouch_SRC})
        target_link_libraries(indi_microtouch_focus indidriver)
        install(TARGETS indi_microtouch_focus RUNTIME DESTINATION bin)

################ Baader SteelDrive II Focuser ################

        set(steeldrive2_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/focuser/steeldrive2.cpp)

        add_executable(indi_steeldrive2_focus ${steeldrive2_SRC})
        target_link_libraries(indi_steeldrive2_focus indidriver)
        install(TARGETS indi_steeldrive2_focus RUNTIME DESTINATION bin)

################ Baader SteelDrive Focuser ################

        set(steeldrive_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/focuser/steeldrive.cpp)

        add_executable(indi_steeldrive_focus ${steeldrive_SRC})
        target_link_libraries(indi_steeldrive_focus indidriver)
        install(TARGETS indi_steeldrive_focus RUNTIME DESTINATION bin)

################ FocusLynx Focuser ################

        set(focuslynx_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/focuser/focuslynxbase.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/focuser/focuslynx.cpp)

        if(UNITY_BUILD)
            enable_unity_build(focuslynx focuslynx_SRC 10 cpp)
        endif()

        add_executable(indi_lynx_focus ${focuslynx_SRC})
        target_link_libraries(indi_lynx_focus indidriver)
        install(TARGETS indi_lynx_focus RUNTIME DESTINATION bin)

################ PerfectStar Focuser ################

        set(perfectstar_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/focuser/perfectstar.cpp)

        add_executable(indi_perfectstar_focus ${perfectstar_SRC})
        target_link_libraries(indi_perfectstar_focus indidriver)
        install(TARGETS indi_perfectstar_focus RUNTIME DESTINATION bin)

################ Starlight Instruments EFS ################

        set(siefs_SRC ${CMAKE_CURRENT_SOURCE_DIR}/drivers/focuser/si_efs.cpp)

        add_executable(indi_siefs_focus ${siefs_SRC})
        target_link_libraries(indi_siefs_focus indidriver)
        install(TARGETS indi_siefs_focus RUNTIME DESTINATION bin)

################ hitechfocus Focuser ################

        set(hitecastrodcfocuser_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/focuser/hitecastrodcfocuser.cpp)

        add_executable(indi_hitecastrodc_focus ${hitecastrodcfocuser_SRC})
        target_link_libraries(indi_hitecastrodc_focus indidriver)
        install(TARGETS indi_hitecastrodc_focus RUNTIME DESTINATION bin)

################ DeepSky Dad AF1 ################

        set(deepskydad_af1_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/focuser/deepskydad_af1.cpp)

        add_executable(indi_deepskydad_af1_focus ${deepskydad_af1_SRC})
        target_link_libraries(indi_deepskydad_af1_focus indidriver)
        install(TARGETS indi_deepskydad_af1_focus RUNTIME DESTINATION bin)

################ DeepSky Dad AF2 ################

        set(deepskydad_af2_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/focuser/deepskydad_af2.cpp)

        add_executable(indi_deepskydad_af2_focus ${deepskydad_af2_SRC})
        target_link_libraries(indi_deepskydad_af2_focus indidriver)
        install(TARGETS indi_deepskydad_af2_focus RUNTIME DESTINATION bin)

################ DeepSky Dad AF3 ################

        set(deepskydad_af3_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/focuser/deepskydad_af3.cpp)

        add_executable(indi_deepskydad_af3_focus ${deepskydad_af3_SRC})
        target_link_libraries(indi_deepskydad_af3_focus indidriver)
        install(TARGETS indi_deepskydad_af3_focus RUNTIME DESTINATION bin)

################ JMI Smart Focus Focuser ################

        set(smartfocus_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/focuser/smartfocus.cpp)

        add_executable(indi_smartfocus_focus ${smartfocus_SRC})
        target_link_libraries(indi_smartfocus_focus indidriver)
        install(TARGETS indi_smartfocus_focus RUNTIME DESTINATION bin)

################ Optec TCF-S ################

        set(tcfs_SRC ${CMAKE_CURRENT_SOURCE_DIR}/drivers/focuser/tcfs.cpp)

        add_executable(indi_tcfs_focus ${tcfs_SRC})

        target_link_libraries(indi_tcfs_focus indidriver)
        install(TARGETS indi_tcfs_focus RUNTIME DESTINATION bin)

        file(
            WRITE ${CMAKE_CURRENT_BINARY_DIR}/make_tcfs_symlink.cmake
            "exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink indi_tcfs_focus \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_tcfs3_focus)\n"
        )
        set_target_properties(
            indi_tcfs_focus
            PROPERTIES POST_INSTALL_SCRIPT
                       ${CMAKE_CURRENT_BINARY_DIR}/make_tcfs_symlink.cmake)

################ Lacerta MFOC Focuser ################

        set(lacerta_mfoc_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/focuser/lacerta_mfoc.cpp)

        add_executable(indi_lacerta_mfoc_focus ${lacerta_mfoc_SRC})
        target_link_libraries(indi_lacerta_mfoc_focus indidriver)
        install(TARGETS indi_lacerta_mfoc_focus RUNTIME DESTINATION bin)

        # UDEV Rules File for Focusers #### One rule for for all INDI standard
        # focusers ####
        if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
            install(
                FILES
                    ${CMAKE_CURRENT_SOURCE_DIR}/drivers/focuser/99-focusers.rules
                DESTINATION ${UDEVRULES_INSTALL_DIR})
        endif()
#################################################################################

#####################################
######## Rotator GROUP #########
#####################################

################ Rotator Simulator ########

        set(rotatorsim_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/rotator/rotator_simulator.cpp)

        if(UNITY_BUILD)
            enable_unity_build(rotatorsim rotatorsim_SRC 10 cpp)
        endif()

        add_executable(indi_simulator_rotator ${rotatorsim_SRC})
        target_link_libraries(indi_simulator_rotator indidriver)
        install(TARGETS indi_simulator_rotator RUNTIME DESTINATION bin)

################ Optec Gemini Focusing Rotator ########

        set(gemini_SRC ${CMAKE_CURRENT_SOURCE_DIR}/drivers/rotator/gemini.cpp)

        if(UNITY_BUILD)
            enable_unity_build(gemini gemini_SRC 10 cpp)
        endif()

        add_executable(indi_gemini_focus ${gemini_SRC})
        target_link_libraries(indi_gemini_focus indidriver)
        install(TARGETS indi_gemini_focus RUNTIME DESTINATION bin)

################ NightCrawler Focusing Rotator ################

        set(nightcrawler_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/rotator/nightcrawler.cpp)

        add_executable(indi_nightcrawler_focus ${nightcrawler_SRC})
        target_link_libraries(indi_nightcrawler_focus indidriver)
        install(TARGETS indi_nightcrawler_focus RUNTIME DESTINATION bin)

################ Pyxis Rotator ################

        set(pyxis_SRC ${CMAKE_CURRENT_SOURCE_DIR}/drivers/rotator/pyxis.cpp)

        add_executable(indi_pyxis_rotator ${pyxis_SRC})
        target_link_libraries(indi_pyxis_rotator indidriver)
        install(TARGETS indi_pyxis_rotator RUNTIME DESTINATION bin)

################ Falcon Rotator ################

        set(falcon_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/rotator/pegasus_falcon.cpp)

        add_executable(indi_falcon_rotator ${falcon_SRC})
        target_link_libraries(indi_falcon_rotator indidriver)
        install(TARGETS indi_falcon_rotator RUNTIME DESTINATION bin)

################ Integra85 Focusing Rotator ################

        set(integra_SRC ${CMAKE_CURRENT_SOURCE_DIR}/drivers/rotator/integra.cpp)

        add_executable(indi_integra_focus ${integra_SRC})
        target_link_libraries(indi_integra_focus indidriver)
        install(TARGETS indi_integra_focus RUNTIME DESTINATION bin)

#################################################################################

#####################################
######## FILTER WHEEL GROUP #########
#####################################

########### XAGYL Wheel ##############
        set(xagylwheel_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/filter_wheel/xagyl_wheel.cpp)

        add_executable(indi_xagyl_wheel ${xagylwheel_SRC})
        target_link_libraries(indi_xagyl_wheel indidriver)
        install(TARGETS indi_xagyl_wheel RUNTIME DESTINATION bin)

########### Filter Simulator ##############
        set(filtersimulator_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/filter_wheel/filter_simulator.cpp
        )

        add_executable(indi_simulator_wheel ${filtersimulator_SRC})
        target_link_libraries(indi_simulator_wheel indidriver)
        install(TARGETS indi_simulator_wheel RUNTIME DESTINATION bin)

########### Manual Filter ##############
        set(manualfilter_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/filter_wheel/manual_filter.cpp)

        add_executable(indi_manual_wheel ${manualfilter_SRC})
        target_link_libraries(indi_manual_wheel indidriver)
        install(TARGETS indi_manual_wheel RUNTIME DESTINATION bin)

########## Optec Wheel IFW ############
        set(optecwheel_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/filter_wheel/ifwoptec.cpp)

        add_executable(indi_optec_wheel ${optecwheel_SRC})
        target_link_libraries(indi_optec_wheel indidriver)
        install(TARGETS indi_optec_wheel RUNTIME DESTINATION bin)

########## Quantum Wheel ############
        set(quantum_wheel_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/filter_wheel/quantum_wheel.cpp)

        add_executable(indi_quantum_wheel ${quantum_wheel_SRC})
        target_link_libraries(indi_quantum_wheel indidriver)
        install(TARGETS indi_quantum_wheel RUNTIME DESTINATION bin)

########## TruTech Wheel ############
        set(trutechwheel_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/filter_wheel/trutech_wheel.cpp)

        add_executable(indi_trutech_wheel ${trutechwheel_SRC})
        target_link_libraries(indi_trutech_wheel indidriver)
        install(TARGETS indi_trutech_wheel RUNTIME DESTINATION bin)

########## QHYCFW Wheel 1 ############
        set(qhycfw1_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/filter_wheel/qhycfw1.cpp)

        add_executable(indi_qhycfw1_wheel ${qhycfw1_SRC})
        target_link_libraries(indi_qhycfw1_wheel indidriver)
        install(TARGETS indi_qhycfw1_wheel RUNTIME DESTINATION bin)

########## QHYCFW Wheel 2 ############
        set(qhycfw2_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/filter_wheel/qhycfw2.cpp)

        add_executable(indi_qhycfw2_wheel ${qhycfw2_SRC})
        target_link_libraries(indi_qhycfw2_wheel indidriver)
        install(TARGETS indi_qhycfw2_wheel RUNTIME DESTINATION bin)

########## QHYCFW Wheel 3 ############
        set(qhycfw3_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/filter_wheel/qhycfw3.cpp)

        add_executable(indi_qhycfw3_wheel ${qhycfw3_SRC})
        target_link_libraries(indi_qhycfw3_wheel indidriver)
        install(TARGETS indi_qhycfw3_wheel RUNTIME DESTINATION bin)
#################################################################################

##################################
########## DOME GROUP ############
#####################################

################ Dome Simulator ################

        set(domesimulator_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/dome/dome_simulator.cpp)

        add_executable(indi_simulator_dome ${domesimulator_SRC})
        target_link_libraries(indi_simulator_dome indidriver)
        install(TARGETS indi_simulator_dome RUNTIME DESTINATION bin)

################ Roll Off ################

        set(rolloff_SRC ${CMAKE_CURRENT_SOURCE_DIR}/drivers/dome/roll_off.cpp)

        add_executable(indi_rolloff_dome ${rolloff_SRC})
        target_link_libraries(indi_rolloff_dome indidriver)
        install(TARGETS indi_rolloff_dome RUNTIME DESTINATION bin)

################ Baader Dome ################

        set(baaderdome_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/dome/baader_dome.cpp)

        add_executable(indi_baader_dome ${baaderdome_SRC})
        target_link_libraries(indi_baader_dome indidriver)
        install(TARGETS indi_baader_dome RUNTIME DESTINATION bin)

################ Dome Pro 2 ##$##############

        set(domepro2_SRC ${CMAKE_CURRENT_SOURCE_DIR}/drivers/dome/domepro2.cpp)

        add_executable(indi_domepro2_dome ${domepro2_SRC})
        target_link_libraries(indi_domepro2_dome indidriver)
        install(TARGETS indi_domepro2_dome RUNTIME DESTINATION bin)

################ Rigel Dome ################

        set(rigeldome_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/dome/rigel_dome.cpp)

        add_executable(indi_rigel_dome ${rigeldome_SRC})
        target_link_libraries(indi_rigel_dome indidriver)
        install(TARGETS indi_rigel_dome RUNTIME DESTINATION bin)

################ ScopeDome Dome ################

        set(scopedome_SRCS
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/dome/scopedome_dome.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/dome/scopedome_usb21.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/dome/scopedome_sim.cpp)

        add_executable(indi_scopedome_dome ${scopedome_SRCS})
        target_link_libraries(indi_scopedome_dome indidriver)
        install(TARGETS indi_scopedome_dome RUNTIME DESTINATION bin)

########### Dome Scripting Gateway ##############
        set(domescript_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/dome/dome_script.cpp)

        add_executable(indi_script_dome ${domescript_SRC})
        target_link_libraries(indi_script_dome indidriver)
        install(TARGETS indi_script_dome RUNTIME DESTINATION bin)
#################################################################################

#########################################
########### VIDEO GROUP   ###############
#########################################

########### INDI::CCD V4L Driver ###############
        if(${CMAKE_SYSTEM_NAME} MATCHES "Linux|FreeBSD")
            set(v4l2driverccd_SRC
                ${CMAKE_CURRENT_SOURCE_DIR}/libs/lx/Lx.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/drivers/video/v4l2driver.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/drivers/video/indi_v4l2driver.cpp)

            if(UNITY_BUILD)
                enable_unity_build(v4l2driverccd v4l2driverccd_SRC 10 cpp)
            endif()

            add_executable(
                indi_v4l2_ccd ${v4l2driverccd_SRC} ${libwebcam_C_SRC}
                              ${libwebcam_CXX_SRC})
            target_link_libraries(indi_v4l2_ccd ${JPEG_LIBRARY} indidriver)
            install(TARGETS indi_v4l2_ccd RUNTIME DESTINATION bin)

            # For DBK21 camera
            if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
                install(
                    FILES
                        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/video/80-dbk21-camera.rules
                    DESTINATION ${UDEVRULES_INSTALL_DIR})
            endif()
        endif()
#################################################################################

#####################################
############ AUX GROUP ##############
#####################################
        if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
            install(
                FILES
                    ${CMAKE_CURRENT_SOURCE_DIR}/drivers/auxiliary/99-indi_auxiliary.rules
                DESTINATION ${UDEVRULES_INSTALL_DIR})
        endif()
########### SkySafari Middleware ##############
        if(INDI_BUILD_CLIENT)
            set(skysafari_SRC
                ${CMAKE_CURRENT_SOURCE_DIR}/drivers/auxiliary/skysafari.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/drivers/auxiliary/skysafariclient.cpp
            )

            if(UNITY_BUILD)
                enable_unity_build(skysafari skysafari_SRC 10 cpp)
            endif()

            add_executable(indi_skysafari ${skysafari_SRC})
            target_link_libraries(indi_skysafari indidriver indiclient)
            install(TARGETS indi_skysafari RUNTIME DESTINATION bin)
        else()
            message(
                WARNING
                    "Skipping build of INDI SkySafari driver since INDI POSIX Client is not built"
            )
        endif()

########### Watch dog ###############
        if(INDI_BUILD_CLIENT)
            set(watchdog_SRC
                ${CMAKE_CURRENT_SOURCE_DIR}/drivers/auxiliary/watchdog.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/drivers/auxiliary/watchdogclient.cpp
            )

            if(UNITY_BUILD)
                enable_unity_build(watchdog watchdog_SRC 10 cpp)
            endif()

            add_executable(indi_watchdog ${watchdog_SRC})

            target_link_libraries(indi_watchdog indidriver indiclient)
            install(TARGETS indi_watchdog RUNTIME DESTINATION bin)
        else()
            message(
                WARNING
                    "Skipping build of INDI WatchDog driver since INDI POSIX Client is not built"
            )
        endif()

########### Flip Flat & Flip Man Driver ###############

        set(flipflat_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/auxiliary/flip_flat.cpp)

        add_executable(indi_flipflat ${flipflat_SRC})
        target_link_libraries(indi_flipflat indidriver)
        install(TARGETS indi_flipflat RUNTIME DESTINATION bin)

########### Pegasus Ultimate Power Box Driver ###############

        set(pegasus_upb_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/auxiliary/pegasus_upb.cpp)

        add_executable(indi_pegasus_upb ${pegasus_upb_SRC})
        target_link_libraries(indi_pegasus_upb indidriver)
        install(TARGETS indi_pegasus_upb RUNTIME DESTINATION bin)

########### Pegasus Pocket Power Box Driver ###############

        set(pegasus_ppb_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/auxiliary/pegasus_ppb.cpp)

        add_executable(indi_pegasus_ppb ${pegasus_ppb_SRC})
        target_link_libraries(indi_pegasus_ppb indidriver)
        install(TARGETS indi_pegasus_ppb RUNTIME DESTINATION bin)

########### Pegasus Pocket Power Box Advance Driver ###############

        set(pegasus_ppba_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/auxiliary/pegasus_ppba.cpp)

        add_executable(indi_pegasus_ppba ${pegasus_ppba_SRC})
        target_link_libraries(indi_pegasus_ppba indidriver)
        install(TARGETS indi_pegasus_ppba RUNTIME DESTINATION bin)

########### SnapCap Driver ###############

        set(snapcap_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/auxiliary/snapcap.cpp)

        add_executable(indi_snapcap ${snapcap_SRC})
        target_link_libraries(indi_snapcap indidriver)
        install(TARGETS indi_snapcap RUNTIME DESTINATION bin)

########### Light Pollution Meter #############

        set(lpm_SRC ${CMAKE_CURRENT_SOURCE_DIR}/drivers/auxiliary/lpm.cpp)

        add_executable(indi_astromech_lpm ${lpm_SRC})
        target_link_libraries(indi_astromech_lpm indidriver)
        install(TARGETS indi_astromech_lpm RUNTIME DESTINATION bin)

########### Sky Quality Meter ###############

        set(sqm_SRC ${CMAKE_CURRENT_SOURCE_DIR}/drivers/auxiliary/sqm.cpp)

        add_executable(indi_sqm_weather ${sqm_SRC})
        target_link_libraries(indi_sqm_weather indidriver)
        install(TARGETS indi_sqm_weather RUNTIME DESTINATION bin)

########### Sky Quality Meter Simulator ###############

        set(sqm_simulator_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/auxiliary/sqm_simulator.cpp)

        add_executable(indi_simulator_sqm ${sqm_simulator_SRC})
        target_link_libraries(indi_simulator_sqm indidriver)
        install(TARGETS indi_simulator_sqm RUNTIME DESTINATION bin)

########### Astrometry Driver ###############

        set(astrometry_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/auxiliary/astrometrydriver.cpp)

        add_executable(indi_astrometry ${astrometry_SRC})
        target_link_libraries(indi_astrometry indidriver)
        install(TARGETS indi_astrometry RUNTIME DESTINATION bin)

########### STAR2000 Driver ###############

        set(STAR2000_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/auxiliary/STAR2kdriver.c
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/auxiliary/STAR2000.cpp)

        add_executable(indi_star2000 ${STAR2000_SRC})
        target_link_libraries(indi_star2000 indidriver)
        install(TARGETS indi_star2000 RUNTIME DESTINATION bin)

########### Arduino ST4 Driver ###############

        set(arduinost4_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/auxiliary/arduino_st4.cpp)

        if(UNITY_BUILD)
            enable_unity_build(arduinost4 arduinost4_SRC 10 cpp)
        endif()

        add_executable(indi_arduinost4 ${arduinost4_SRC})
        target_link_libraries(indi_arduinost4 indidriver)
        install(TARGETS indi_arduinost4 RUNTIME DESTINATION bin)

########### PlaneWave Delta-T Driver ###############

        set(planewave_deltat_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/auxiliary/planewave_delta.cpp)

        if(UNITY_BUILD)
            enable_unity_build(indi_planewave_deltat planewave_deltat_SRC 10
                               cpp)
        endif()

        add_executable(indi_planewave_deltat ${planewave_deltat_SRC})
        target_link_libraries(indi_planewave_deltat indidriver)
        install(TARGETS indi_planewave_deltat RUNTIME DESTINATION bin)

########### GPUSB Driver ###############

        set(gpusb_SRC ${CMAKE_CURRENT_SOURCE_DIR}/drivers/auxiliary/gpdriver.cpp
                      ${CMAKE_CURRENT_SOURCE_DIR}/drivers/auxiliary/gpusb.cpp)

        if(UNITY_BUILD)
            enable_unity_build(gpusb gpusb_SRC 10 cpp)
        endif()

        add_executable(indi_gpusb ${gpusb_SRC})
        target_link_libraries(indi_gpusb indidriver)
        install(TARGETS indi_gpusb RUNTIME DESTINATION bin)

########### Joystick Driver ###############
        if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
            set(joystick_SRC
                ${CMAKE_CURRENT_SOURCE_DIR}/drivers/auxiliary/joystickdriver.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/drivers/auxiliary/joystick.cpp)

            if(UNITY_BUILD)
                enable_unity_build(joystick joystick_SRC 10 cpp)
            endif()

            add_executable(indi_joystick ${joystick_SRC})
            target_link_libraries(indi_joystick indidriver)
            install(TARGETS indi_joystick RUNTIME DESTINATION bin)
        endif()

########### GPS Simulator Driver ###############

        set(gpssimulator_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/auxiliary/gps_simulator.cpp)

        add_executable(indi_simulator_gps ${gpssimulator_SRC})
        target_link_libraries(indi_simulator_gps indidriver)
        install(TARGETS indi_simulator_gps RUNTIME DESTINATION bin)

########### USB_Dewpoint Driver ###############

        set(usb_dewpoint_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/auxiliary/usb_dewpoint.cpp)

        add_executable(indi_usbdewpoint ${usb_dewpoint_SRC})
        target_link_libraries(indi_usbdewpoint indidriver)
        install(TARGETS indi_usbdewpoint RUNTIME DESTINATION bin)

#####################################
########## WEATHER GROUP ############
#####################################

########### Weather Simulator ###############

        set(weathersimulator_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/weather/weathersimulator.cpp)

        add_executable(indi_simulator_weather ${weathersimulator_SRC})
        target_link_libraries(indi_simulator_weather indidriver)
        install(TARGETS indi_simulator_weather RUNTIME DESTINATION bin)

########### Weather Meta Driver ###############

        set(weathermeta_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/weather/weathermeta.cpp)

        add_executable(indi_meta_weather ${weathermeta_SRC})
        target_link_libraries(indi_meta_weather indidriver)
        install(TARGETS indi_meta_weather RUNTIME DESTINATION bin)

########### Weather File Watcher ###############

        set(weatherwatcher_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/weather/weatherwatcher.cpp)

        add_executable(indi_watcher_weather ${weatherwatcher_SRC})
        target_link_libraries(indi_watcher_weather indidriver ${CURL_LIBRARIES})
        install(TARGETS indi_watcher_weather RUNTIME DESTINATION bin)

########### Weather Safety Proxy ###############

        set(weathersafetyproxy_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/weather/gason.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/weather/weather_safety_proxy.cpp
        )

        add_executable(indi_weather_safety_proxy ${weathersafetyproxy_SRC})
        target_link_libraries(indi_weather_safety_proxy indidriver
                              ${CURL_LIBRARIES})
        install(TARGETS indi_weather_safety_proxy RUNTIME DESTINATION bin)

########### MBox Driver ###############

        set(mbox_SRC ${CMAKE_CURRENT_SOURCE_DIR}/drivers/weather/mbox.cpp)

        add_executable(indi_mbox_weather ${mbox_SRC})
        target_link_libraries(indi_mbox_weather indidriver)
        install(TARGETS indi_mbox_weather RUNTIME DESTINATION bin)

########### Vantage Driver ###############

        set(vantage_SRC ${CMAKE_CURRENT_SOURCE_DIR}/drivers/weather/vantage.cpp)

        add_executable(indi_vantage_weather ${vantage_SRC})
        target_link_libraries(indi_vantage_weather indidriver)
        install(TARGETS indi_vantage_weather RUNTIME DESTINATION bin)

        if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
            install(
                FILES
                    ${CMAKE_CURRENT_SOURCE_DIR}/drivers/weather/99-vantage.rules
                DESTINATION ${UDEVRULES_INSTALL_DIR})
        endif()

########### OpenWeatherMap Driver ###############

        set(OpenWeatherMap_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/weather/gason.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/drivers/weather/openweathermap.cpp)

        if(UNITY_BUILD)
            enable_unity_build(OpenWeatherMap OpenWeatherMap_SRC 10 cpp)
        endif()

        add_executable(indi_openweathermap_weather ${OpenWeatherMap_SRC})
        target_link_libraries(indi_openweathermap_weather indidriver
                              ${CURL_LIBRARIES})
        install(TARGETS indi_openweathermap_weather RUNTIME DESTINATION bin)

#####################################
############ INDI TOOLS #############
#####################################

########### getINDI ##############
        set(indi_get_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/eventloop.c
            ${CMAKE_CURRENT_SOURCE_DIR}/base64.c
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/getINDIproperty.c
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/lilxml.c)

        if(UNITY_BUILD)
            enable_unity_build(indi_get indi_get_SRC 10 c)
        endif()

        set(indi_get_SRC
            ${indi_get_SRC} ${CMAKE_CURRENT_SOURCE_DIR}/libs/libastro.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indicom.c)

        add_executable(indi_getprop ${indi_get_SRC})

        target_link_libraries(indi_getprop ${NOVA_LIBRARIES} ${M_LIB}
                              ${ZLIB_LIBRARY})

        install(TARGETS indi_getprop RUNTIME DESTINATION bin)

#################################################################################

########### setINDI ##############
        set(indi_set_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/eventloop.c
            ${CMAKE_CURRENT_SOURCE_DIR}/base64.c
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/setINDIproperty.c
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/lilxml.c)

        if(UNITY_BUILD)
            enable_unity_build(indi_set indi_set_SRC 10 c)
        endif()

        set(indi_set_SRC
            ${indi_set_SRC} ${CMAKE_CURRENT_SOURCE_DIR}/libs/libastro.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indicom.c)

        add_executable(indi_setprop ${indi_set_SRC})

        target_link_libraries(indi_setprop ${NOVA_LIBRARIES} ${M_LIB}
                              ${ZLIB_LIBRARY})

        install(TARGETS indi_setprop RUNTIME DESTINATION bin)
#################################################################################

########### evalINDI ##############
        set(indi_eval_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/eventloop.c
            ${CMAKE_CURRENT_SOURCE_DIR}/base64.c
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/compiler.c
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/evalINDI.c
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/lilxml.c)

        if(UNITY_BUILD)
            enable_unity_build(indi_eval indi_eval_SRC 10 c)
        endif()

        set(indi_eval_SRC
            ${indi_eval_SRC} ${CMAKE_CURRENT_SOURCE_DIR}/libs/libastro.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indicom.c)

        add_executable(indi_eval ${indi_eval_SRC})

        target_link_libraries(indi_eval ${NOVA_LIBRARIES} ${M_LIB}
                              ${ZLIB_LIBRARY})

        install(TARGETS indi_eval RUNTIME DESTINATION bin)

########### HID Test ##############
        set(indi_hid_SRC ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/hidtest.cpp)

        if(UNITY_BUILD)
            enable_unity_build(indi_hid indi_hid_SRC 10 cpp)
        endif()

        if(APPLE)
            set(LIBS "-framework IOKit -framework CoreFoundation")
        elseif(CYGWIN)
            set(LIBS ${ICONV_LIBRARIES})
        endif(APPLE)

        set(indi_hid_SRC ${indi_hid_SRC} ${hidapi_SRCS})

        add_executable(indi_hid_test ${indi_hid_SRC})

        target_link_libraries(
            indi_hid_test ${ICONV_LIBRARIES} ${USB1_LIBRARIES}
            ${CMAKE_THREAD_LIBS_INIT} ${LIBS})

        install(TARGETS indi_hid_test RUNTIME DESTINATION bin)

#################################################################################

## Build Examples

        if(INDI_BUILD_CLIENT)
            add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/examples)
        else()
            message(
                WARNING
                    "Skipping build of examples since INDI POSIX client is not built"
            )
        endif()

#################################################################################

        install(
            FILES drivers.xml
                  ${CMAKE_CURRENT_SOURCE_DIR}/drivers/focuser/indi_tcfs_sk.xml
            DESTINATION ${DATA_INSTALL_DIR})

        configure_file(${CMAKE_CURRENT_SOURCE_DIR}/libindi.pc.cmake
                       ${CMAKE_CURRENT_BINARY_DIR}/libindi.pc @ONLY)
        install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libindi.pc
                DESTINATION ${PKGCONFIG_INSTALL_PREFIX})

        if(UNIX)
            install(
                FILES ${CMAKE_CURRENT_SOURCE_DIR}/libs/stream/streammanager.h
                      ${CMAKE_CURRENT_SOURCE_DIR}/libs/stream/fpsmeter.h
                      ${CMAKE_CURRENT_SOURCE_DIR}/libs/stream/jpegutils.h
                      ${CMAKE_CURRENT_SOURCE_DIR}/libs/stream/ccvt.h
                      ${CMAKE_CURRENT_SOURCE_DIR}/libs/stream/ccvt_types.h
                DESTINATION ${INCLUDE_INSTALL_DIR}/libindi/stream
                COMPONENT Devel)
            install(
                FILES
                    ${CMAKE_CURRENT_SOURCE_DIR}/libs/stream/encoder/encodermanager.h
                    ${CMAKE_CURRENT_SOURCE_DIR}/libs/stream/encoder/encoderinterface.h
                    ${CMAKE_CURRENT_SOURCE_DIR}/libs/stream/encoder/rawencoder.h
                    ${CMAKE_CURRENT_SOURCE_DIR}/libs/stream/encoder/mjpegencoder.h
                DESTINATION ${INCLUDE_INSTALL_DIR}/libindi/stream/encoder
                COMPONENT Devel)
            install(
                FILES
                    ${CMAKE_CURRENT_SOURCE_DIR}/libs/stream/recorder/recordermanager.h
                    ${CMAKE_CURRENT_SOURCE_DIR}/libs/stream/recorder/recorderinterface.h
                    ${CMAKE_CURRENT_SOURCE_DIR}/libs/stream/recorder/serrecorder.h
                DESTINATION ${INCLUDE_INSTALL_DIR}/libindi/stream/recorder
                COMPONENT Devel)
            if(${CMAKE_SYSTEM_NAME} MATCHES "Linux|FreeBSD")
                install(
                    FILES
                        ${CMAKE_CURRENT_SOURCE_DIR}/libs/webcam/v4l2_decode/v4l2_decode.h
                        ${CMAKE_CURRENT_SOURCE_DIR}/libs/webcam/v4l2_decode/v4l2_builtin_decoder.h
                        ${CMAKE_CURRENT_SOURCE_DIR}/libs/webcam/v4l2_colorspace.h
                    DESTINATION ${INCLUDE_INSTALL_DIR}/libindi
                    COMPONENT Devel)
            endif()
        endif()
###################################################################################################
#########################################  Tests  #################################################
###################################################################################################

        find_package(GTest)
        find_package(GMock)
        if(GTEST_FOUND)
            if(INDI_BUILD_UNITTESTS)
                message(STATUS "Building unit tests")
                add_subdirectory(test)
            else(INDI_BUILD_UNITTESTS)
                message(STATUS "Not building unit tests")
            endif(INDI_BUILD_UNITTESTS)
        else()
            message(STATUS "GTEST not found, not building unit tests")
        endif(GTEST_FOUND)

    endif(WIN32 OR ANDROID)
endif(INDI_BUILD_DRIVERS)

###################################################################################################
#######################################  config.h  ################################################
###################################################################################################
# Generate config.h from template
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/config.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indiversion.h.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/indiversion.h)

if(MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-D_WINSOCK_DEPRECATED_NO_WARNINGS)
endif()

# Install common dev files for all except server
if(INDI_BUILD_DRIVERS
   OR INDI_BUILD_CLIENT
   OR INDI_BUILD_QT5_CLIENT)
    install(
        FILES ${CMAKE_CURRENT_BINARY_DIR}/indiversion.h
        DESTINATION ${INCLUDE_INSTALL_DIR}/libindi
        COMPONENT Devel)
    install(
        FILES ${CMAKE_CURRENT_SOURCE_DIR}/indiapi.h
              ${CMAKE_CURRENT_SOURCE_DIR}/indidevapi.h
              ${CMAKE_CURRENT_SOURCE_DIR}/base64.h
              ${CMAKE_CURRENT_SOURCE_DIR}/libs/lilxml.h
              ${CMAKE_CURRENT_SOURCE_DIR}/libs/libastro.h
              ${CMAKE_CURRENT_SOURCE_DIR}/libs/indicom.h
              ${CMAKE_CURRENT_SOURCE_DIR}/eventloop.h
              ${CMAKE_CURRENT_SOURCE_DIR}/indidriver.h
              ${CMAKE_CURRENT_SOURCE_DIR}/libs/dsp/dsp.h
              ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/pid/pid.h
              ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indibase.h
              ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indibasetypes.h
              ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/basedevice.h
              ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/defaultdevice.h
              ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indiccd.h
              ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indiccdchip.h
              ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indisensorinterface.h
              ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indicorrelator.h
              ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indidetector.h
              ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indispectrograph.h
              ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indifilterwheel.h
              ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indifocuserinterface.h
              ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indiweatherinterface.h
              ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indifocuser.h
              ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indirotator.h
              ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/inditelescope.h
              ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indiguiderinterface.h
              ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indifilterinterface.h
              ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indirotatorinterface.h
              ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indiproperty.h
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
        DESTINATION ${INCLUDE_INSTALL_DIR}/libindi
        COMPONENT Devel)

    if(HAVE_WEBSOCKET)
        install(
            FILES ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/indiwsserver.h
            DESTINATION ${INCLUDE_INSTALL_DIR}/libindi
            COMPONENT Devel)
    endif()
    install(
        FILES
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/connectionplugins/connectioninterface.h
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/connectionplugins/connectionserial.h
            ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/connectionplugins/connectiontcp.h
        DESTINATION ${INCLUDE_INSTALL_DIR}/libindi/connectionplugins
        COMPONENT Devel)

    install(
        FILES ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/dsp/manager.h
              ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/dsp/dspinterface.h
              ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/dsp/transforms.h
              ${CMAKE_CURRENT_SOURCE_DIR}/libs/indibase/dsp/convolution.h
        DESTINATION ${INCLUDE_INSTALL_DIR}/libindi/dsp
        COMPONENT Devel)
endif(
    INDI_BUILD_DRIVERS
    OR INDI_BUILD_CLIENT
    OR INDI_BUILD_QT5_CLIENT)

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
