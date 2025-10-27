cmake_minimum_required(VERSION 3.18)
project(indidriver C CXX)
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.31)
    cmake_policy(SET CMP0177 NEW)
endif()

include_directories(.)
include_directories(timer)
include_directories(thread)
include_directories(stream)

# Dependency
find_package(FFTW3 REQUIRED)
find_package(UDEV)

list(APPEND ${PROJECT_NAME}_LIBS
    indicore
    indidevice
    eventloop
    dsp
    fpack
    AlignmentDriver
    ${HIDAPILIB}
    ${USB1_LIBRARIES}
    ${NOVA_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    ${CFITSIO_LIBRARIES}
    ${ZLIB_LIBRARY}
    ${JPEG_LIBRARY}
    ${FFTW3_LIBRARIES}
    ${M_LIB}
    ${CURL}
)

if(UDEV_FOUND)
    list(APPEND ${PROJECT_NAME}_LIBS ${UDEV_LIBRARIES})
endif()

# When bundled hid library is used
if(NOT SYSTEM_HIDAPILIB)
    list(APPEND ${PROJECT_NAME}_LIBS indi_hid)
endif()

# Add Iconv - FreeBSD/OpenBSD need to find the correct GNU iconv library.
if(CYGWIN OR UNIX OR (CMAKE_SYSTEM_NAME MATCHES "FreeBSD|OpenBSD"))
    find_package(Iconv REQUIRED)
    list(APPEND ${PROJECT_NAME}_LIBS ${Iconv_LIBRARIES})
endif()

# Add LibXISF
find_package(LibXISF)
if(LibXISF_FOUND)
    list(APPEND ${PROJECT_NAME}_LIBS ${LibXISF_LIBRARY})
    include_directories(${LibXISF_INCLUDE_DIR})
    add_definitions(-DHAVE_XISF)
endif()

# Add OggTheora, StreamManager, v4l2
if(UNIX)
    find_package(OggTheora)

    if(OGGTHEORA_FOUND)
        include_directories(${THEORA_INCLUDE_DIRS})
        set(HAVE_THEORA 1)
        list(APPEND ${PROJECT_NAME}_SOURCES
            stream/recorder/theorarecorder.cpp
        )
        list(APPEND ${PROJECT_NAME}_LIBS ${OGGTHEORA_LIBRARIES} ${THEORA_LIBRARIES})
    endif()

    list(APPEND ${PROJECT_NAME}_SOURCES
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
        DESTINATION ${INCLUDE_INSTALL_DIR}/libindi/stream
        COMPONENT Devel
    )

    install(FILES
        stream/encoder/encodermanager.h
        stream/encoder/encoderinterface.h
        stream/encoder/rawencoder.h
        stream/encoder/mjpegencoder.h
        DESTINATION ${INCLUDE_INSTALL_DIR}/libindi/stream/encoder
        COMPONENT Devel
    )

    install(FILES
        stream/recorder/recordermanager.h
        stream/recorder/recorderinterface.h
        stream/recorder/serrecorder.h
        DESTINATION ${INCLUDE_INSTALL_DIR}/libindi/stream/recorder
        COMPONENT Devel
    )

    if(${CMAKE_SYSTEM_NAME} MATCHES "Linux|FreeBSD|OpenBSD")
        list(APPEND ${PROJECT_NAME}_SOURCES
            webcam/v4l2_colorspace.c
            webcam/v4l2_base.cpp
            webcam/v4l2_decode/v4l2_decode.cpp
            webcam/v4l2_decode/v4l2_builtin_decoder.cpp
        )

        install(FILES
            webcam/v4l2_decode/v4l2_decode.h
            webcam/v4l2_decode/v4l2_builtin_decoder.h
            webcam/v4l2_colorspace.h
            DESTINATION ${INCLUDE_INSTALL_DIR}/libindi
            COMPONENT Devel
        )

        include_directories(webcam)
    endif()
endif()

# Sources
list(APPEND ${PROJECT_NAME}_SOURCES
    indidriver.c
    indidriverio.c
    indidrivermain.c
    defaultdevice.cpp
    hotplugmanager.cpp
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
    indigpsinterface.cpp
    indiweatherinterface.cpp
    indiimuinterface.cpp
    indifocuser.cpp
    indirotator.cpp
    indiusbdevice.cpp
    indiguiderinterface.cpp
    indifilterinterface.cpp
    indirotatorinterface.cpp
    indiinputinterface.cpp
    indioutputinterface.cpp
    indipowerinterface.cpp
    indidome.cpp
    indigps.cpp
    indiimu.cpp        
    indiweather.cpp
    indidustcapinterface.cpp
    indilightboxinterface.cpp
    indilogger.cpp
    indicontroller.cpp
    connectionplugins/connectioninterface.cpp
    connectionplugins/connectionserial.cpp
    connectionplugins/connectiontcp.cpp
    connectionplugins/connectioni2c.cpp
    dsp/manager.cpp
    dsp/dspinterface.cpp
    dsp/transforms.cpp
    dsp/convolution.cpp
    pid/pid.cpp
    fitskeyword.cpp
)

# Headers
list(APPEND ${PROJECT_NAME}_HEADERS
    indidriver.h
    pid/pid.h
    defaultdevice.h
    hotplugcapabledevice.h
    hotplugmanager.h
    indiccd.h
    indiccdchip.h
    indisensorinterface.h
    indicorrelator.h
    indidetector.h
    indispectrograph.h
    indireceiver.h
    indifilterwheel.h
    indifocuserinterface.h
    indigpsinterface.h
    indiweatherinterface.h
    indifocuser.h
    indirotator.h
    inditelescope.h
    indiguiderinterface.h
    indifilterinterface.h
    indirotatorinterface.h
    indiinputinterface.h
    indioutputinterface.h
    indipowerinterface.h
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
    indiimuinterface.h
    indiimu.h
    indiusbdevice.h
    fitskeyword.h
)


# If we build a shared library, we link other build products to it
# If we do not build a shared library, we link the static library
# If we build neither, we linke the object library
if(INDI_BUILD_SHARED)
    set(TARGET_SHARED ${PROJECT_NAME})
    set(TARGET_STATIC ${PROJECT_NAME}_STATIC)
    set(TARGET_OBJECT ${PROJECT_NAME}_OBJECT)
elseif(INDI_BUILD_STATIC)
    set(TARGET_STATIC ${PROJECT_NAME})
    set(TARGET_OBJECT ${PROJECT_NAME}_OBJECT)
else()
    set(TARGET_OBJECT ${PROJECT_NAME})
endif()

# Build Object Library
if(TRUE)
    add_library(${PROJECT_NAME}_OBJECT OBJECT)
    set_property(TARGET ${PROJECT_NAME}_OBJECT PROPERTY POSITION_INDEPENDENT_CODE 1)

    target_compile_definitions(${PROJECT_NAME}_OBJECT PRIVATE
        HAVE_LIBNOVA
        $<$<BOOL:${UDEV_FOUND}>:HAVE_UDEV>
    )

    target_sources(${PROJECT_NAME}_OBJECT
        PUBLIC
        ${${PROJECT_NAME}_HEADERS}
        PRIVATE
        ${${PROJECT_NAME}_SOURCES}
        ${${PROJECT_NAME}_PRIVATE_HEADERS}
    )

    target_link_libraries(${PROJECT_NAME}_OBJECT ${${PROJECT_NAME}_LIBS})

    install(FILES
        ${${PROJECT_NAME}_HEADERS}
        DESTINATION
        ${INCLUDE_INSTALL_DIR}/libindi
        COMPONENT Devel
    )

    install(FILES
        connectionplugins/connectioninterface.h
        connectionplugins/connectionserial.h
        connectionplugins/connectiontcp.h
        connectionplugins/connectioni2c.h
        DESTINATION ${INCLUDE_INSTALL_DIR}/libindi/connectionplugins
        COMPONENT Devel
    )

    install(FILES
        dsp/manager.h
        dsp/dspinterface.h
        dsp/transforms.h
        dsp/convolution.h
        DESTINATION ${INCLUDE_INSTALL_DIR}/libindi/dsp
        COMPONENT Devel
    )
endif()

# Build Static Library
if(INDI_BUILD_STATIC)
    add_library(${TARGET_STATIC} STATIC)

    target_link_libraries(${TARGET_STATIC} ${TARGET_OBJECT} ${${PROJECT_NAME}_LIBS})
    target_include_directories(${TARGET_STATIC} PUBLIC . timer thread stream alignment)

    set_target_properties(${TARGET_STATIC} PROPERTIES
        VERSION ${CMAKE_INDI_VERSION_STRING}
        SOVERSION ${INDI_SOVERSION}
        OUTPUT_NAME ${PROJECT_NAME} # this same name like shared library - backwards compatibility
    )

    # Propagate HAVE_UDEV to consumers of the static library
    if(UDEV_FOUND)
        target_compile_definitions(${TARGET_STATIC} INTERFACE HAVE_UDEV)
    endif()

    install(TARGETS ${TARGET_STATIC}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
endif()

# Build Shared Library
if(INDI_BUILD_SHARED)
    add_library(${TARGET_SHARED} SHARED)

    target_link_libraries(${TARGET_SHARED} PUBLIC ${TARGET_OBJECT} ${${PROJECT_NAME}_LIBS})
    target_include_directories(${TARGET_SHARED} PUBLIC . timer thread stream alignment)

    set_target_properties(${TARGET_SHARED} PROPERTIES
        VERSION ${CMAKE_INDI_VERSION_STRING}
        SOVERSION ${INDI_SOVERSION}
        OUTPUT_NAME ${PROJECT_NAME}
    )

    # Propagate HAVE_UDEV to consumers of the shared library
    if(UDEV_FOUND)
        target_compile_definitions(${TARGET_SHARED} INTERFACE HAVE_UDEV)
    endif()

    install(TARGETS ${TARGET_SHARED}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
endif()
