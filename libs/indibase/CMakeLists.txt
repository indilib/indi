cmake_minimum_required(VERSION 3.16)
project(indidriver C CXX)

include_directories(.)
include_directories(timer)
include_directories(thread)
include_directories(stream)

# Build object to easy build static/shared library
add_library(indidriverobject OBJECT)

# Shared libraries need PIC
set_property(TARGET indidriverobject PROPERTY POSITION_INDEPENDENT_CODE 1)

# Dependency
find_package(USB1 REQUIRED)
find_package(Nova REQUIRED)
find_package(Threads REQUIRED)
find_package(CFITSIO REQUIRED)
find_package(ZLIB REQUIRED)
find_package(JPEG REQUIRED)
find_package(FFTW3 REQUIRED)

find_library(M_LIB m)

list(APPEND indidriver_LIBS
    indicore
    indidevice
    eventloop
    dsp
    fpack
    hid
    ${USB1_LIBRARIES}
    ${NOVA_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    ${CFITSIO_LIBRARIES}
    ${ZLIB_LIBRARY}
    ${JPEG_LIBRARY}
    ${FFTW3_LIBRARIES}
    ${M_LIB}
)

target_compile_definitions(indidriverobject PRIVATE "-DHAVE_LIBNOVA")

# Add Iconv
if(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
    # FreeBSD needs to find the correct GNU iconv library.
    find_package(Iconv REQUIRED)
    list(APPEND indidriver_LIBS ${ICONV_LIBRARIES})
endif()

# Add WebSocket
if(HAVE_WEBSOCKET)
    list(APPEND indidriver_LIBS ${Boost_LIBRARIES})
endif()

# Add OggTheora, StreamManager, v4l2
if(UNIX)
    find_package(OggTheora)

    if(OGGTHEORA_FOUND)
        include_directories(${THEORA_INCLUDE_DIRS})
        set(HAVE_THEORA 1)
        list(APPEND indidriver_SOURCES
            stream/recorder/theorarecorder.cpp
        )
        list(APPEND indidriver_LIBS ${OGGTHEORA_LIBRARIES} ${THEORA_LIBRARIES})
    endif()

    list(APPEND indidriver_SOURCES
        stream/streammanager.cpp
        stream/fpsmeter.cpp
        stream/gammalut16.cpp
        stream/recorder/recorderinterface.cpp
        stream/recorder/recordermanager.cpp
        stream/recorder/serrecorder.cpp
        stream/encoder/encodermanager.cpp
        stream/encoder/encoderinterface.cpp
        stream/encoder/rawencoder.cpp
        stream/encoder/mjpegencoder.cpp
        stream/jpegutils.c
        stream/ccvt_c2.c
        stream/ccvt_misc.c
    )

    install(FILES
        stream/streammanager.h
        stream/fpsmeter.h
        stream/uniquequeue.h
        stream/gammalut16.h
        stream/jpegutils.h
        stream/ccvt.h
        stream/ccvt_types.h
        DESTINATION ${INCLUDE_install_DIR}/libindi/stream
        COMPONENT Devel
    )

    install(FILES
        stream/encoder/encodermanager.h
        stream/encoder/encoderinterface.h
        stream/encoder/rawencoder.h
        stream/encoder/mjpegencoder.h
        DESTINATION ${INCLUDE_install_DIR}/libindi/stream/encoder
        COMPONENT Devel
    )

    install(FILES
        stream/recorder/recordermanager.h
        stream/recorder/recorderinterface.h
        stream/recorder/serrecorder.h
        DESTINATION ${INCLUDE_install_DIR}/libindi/stream/recorder
        COMPONENT Devel
    )

    if(${CMAKE_SYSTEM_NAME} MATCHES "Linux|FreeBSD")
        list(APPEND indidriver_SOURCES
            webcam/v4l2_colorspace.c
            webcam/v4l2_base.cpp
            webcam/v4l2_decode/v4l2_decode.cpp
            webcam/v4l2_decode/v4l2_builtin_decoder.cpp
        )

        install(FILES
            webcam/v4l2_decode/v4l2_decode.h
            webcam/v4l2_decode/v4l2_builtin_decoder.h
            webcam/v4l2_colorspace.h
            DESTINATION ${INCLUDE_install_DIR}/libindi
            COMPONENT Devel
        )

        include_directories(webcam)
    endif()
endif()

list(APPEND indidriver_SOURCES
    indidriver.c
    indidriverio.c
    indidrivermain.c
    defaultdevice.cpp
    timer/inditimer.cpp
    timer/indielapsedtimer.cpp
    thread/indisinglethreadpool.cpp
    indiccd.cpp
    indiccdchip.cpp
    indisensorinterface.cpp
    indicorrelator.cpp
    indidetector.cpp
    indispectrograph.cpp
    indireceiver.cpp
    inditelescope.cpp
    indifilterwheel.cpp
    indifocuserinterface.cpp
    indiweatherinterface.cpp
    indifocuser.cpp
    indirotator.cpp
    indiusbdevice.cpp
    indiguiderinterface.cpp
    indifilterinterface.cpp
    indirotatorinterface.cpp
    indidome.cpp
    indigps.cpp
    indiweather.cpp
    indidustcapinterface.cpp
    indilightboxinterface.cpp
    indilogger.cpp
    indicontroller.cpp
    connectionplugins/connectioninterface.cpp
    connectionplugins/connectionserial.cpp
    connectionplugins/connectiontcp.cpp
    dsp/manager.cpp
    dsp/dspinterface.cpp
    dsp/transforms.cpp
    dsp/convolution.cpp
    pid/pid.cpp

    # connectionplugins/ttybase.cpp
)

list(APPEND indidriver_HEADERS
    indidriver.h
    pid/pid.h
    defaultdevice.h
    indiccd.h
    indiccdchip.h
    indisensorinterface.h
    indicorrelator.h
    indidetector.h
    indispectrograph.h
    indireceiver.h
    indifilterwheel.h
    indifocuserinterface.h
    indiweatherinterface.h
    indifocuser.h
    indirotator.h
    inditelescope.h
    indiguiderinterface.h
    indifilterinterface.h
    indirotatorinterface.h
    timer/inditimer.h
    timer/indielapsedtimer.h
    thread/indisinglethreadpool.h
    indidome.h
    indigps.h
    indilightboxinterface.h
    indidustcapinterface.h
    indiweather.h
    indilogger.h
    indicontroller.h
    indiusbdevice.h
)

target_sources(indidriverobject
    PUBLIC
    ${indidriver_HEADERS}
    PRIVATE
    ${indidriver_SOURCES}
)

target_link_libraries(indidriverobject ${indidriver_LIBS})

install(FILES
    ${indidriver_HEADERS}
    DESTINATION ${INCLUDE_install_DIR}/libindi
    COMPONENT Devel
)

install(FILES
    connectionplugins/connectioninterface.h
    connectionplugins/connectionserial.h
    connectionplugins/connectiontcp.h
    DESTINATION ${INCLUDE_install_DIR}/libindi/connectionplugins
    COMPONENT Devel
)

install(FILES
    dsp/manager.h
    dsp/dspinterface.h
    dsp/transforms.h
    dsp/convolution.h
    DESTINATION ${INCLUDE_install_DIR}/libindi/dsp
    COMPONENT Devel
)

# #################################################
# ######### INDI Default Driver Library ###########
# #################################################

# Static indidriver Library
if(INDI_BUILD_STATIC)
    add_library(indidriverstatic STATIC)
    target_link_libraries(indidriverstatic indidriverobject ${indidriver_LIBS})
    install(TARGETS indidriverstatic ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()

# Shared indidriver Library
add_library(indidriver SHARED)
target_link_libraries(indidriver indidriverobject ${indidriver_LIBS})
install(TARGETS indidriver LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
