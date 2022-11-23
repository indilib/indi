include_directories(timer)
include_directories(thread)
include_directories(stream)

if(UNIX)
    find_package(OggTheora)

    if(OGGTHEORA_FOUND)
        INCLUDE_DIRECTORIES(${THEORA_INCLUDE_DIRS})
        set(HAVE_THEORA 1)
        set(theorarecorder_SRC stream/recorder/theorarecorder.cpp)
    endif(OGGTHEORA_FOUND)

    set(libstream_SRC
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
        ${theorarecorder_SRC})

    if(${CMAKE_SYSTEM_NAME} MATCHES "Linux|FreeBSD")
        set(libwebcam_SRC
            webcam/v4l2_colorspace.c
            webcam/v4l2_base.cpp
            webcam/v4l2_decode/v4l2_decode.cpp
            webcam/v4l2_decode/v4l2_builtin_decoder.cpp
        )
        include_directories(webcam)
    endif(${CMAKE_SYSTEM_NAME} MATCHES "Linux|FreeBSD")
endif(UNIX)

set(indidriver_SRC
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
    ${indidriver_SRC}
    ${libstream_SRC}
    ${indidriver_SRC}
    ${libstream_SRC}
    ${libwebcam_SRC}

    # connectionplugins/ttybase.cpp
)

# #################################################
# ######### INDI Default Driver Library ###########
# #################################################
find_package(FFTW3 REQUIRED)

if(CYGWIN)
    # # For Cygwin we only build static library
    add_definitions(-U__STRICT_ANSI__)

    find_package(Iconv REQUIRED)
    add_library(indidriver STATIC ${indidriver_SRC})

    install(TARGETS indidriver ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
else()
    # # Static indidriver Library
    if(INDI_BUILD_STATIC)
        add_library(indidriverstatic STATIC ${indidriver_SRC})
        target_compile_definitions(indidriverstatic PRIVATE "-DHAVE_LIBNOVA")
        set_target_properties(indidriverstatic PROPERTIES COMPILE_FLAGS "-fPIC")
        set_target_properties(indidriverstatic PROPERTIES VERSION ${CMAKE_INDI_VERSION_STRING} SOVERSION ${INDI_SOVERSION} OUTPUT_NAME indidriver)
        target_link_libraries(indidriverstatic
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
            ${M_LIB}
            ${ZLIB_LIBRARY}
            ${JPEG_LIBRARY}
            ${FFTW3_LIBRARIES}
        )

        if(OGGTHEORA_FOUND)
            target_link_libraries(indidriverstatic ${OGGTHEORA_LIBRARIES} ${THEORA_LIBRARIES})
        endif()

        if(HAVE_WEBSOCKET)
            target_link_libraries(indidriverstatic ${Boost_LIBRARIES})
        endif()

        install(TARGETS indidriverstatic ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    endif()

    # # Dynamic indidriver Library
    add_library(indidriver SHARED ${indidriver_SRC})
    set_target_properties(indidriver PROPERTIES COMPILE_FLAGS "-fPIC")

    install(TARGETS indidriver LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
    set(PKG_CONFIG_LIBS "${PKG_CONFIG_LIBS} -lindidriver -lindiAlignmentDriver")
endif(CYGWIN)

set_target_properties(indidriver PROPERTIES VERSION ${CMAKE_INDI_VERSION_STRING} SOVERSION ${INDI_SOVERSION} OUTPUT_NAME indidriver)
target_link_libraries(indidriver indicore indidevice eventloop dsp fpack hid ${ICONV_LIBRARIES} ${USB1_LIBRARIES} ${NOVA_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${CFITSIO_LIBRARIES} ${M_LIB} ${ZLIB_LIBRARY} ${JPEG_LIBRARY} ${FFTW3_LIBRARIES})
target_compile_definitions(indidriver PRIVATE "-DHAVE_LIBNOVA")

if(OGGTHEORA_FOUND)
    target_link_libraries(indidriver ${OGGTHEORA_LIBRARIES} ${THEORA_LIBRARIES})
endif()

if(HAVE_WEBSOCKET)
    target_link_libraries(indidriver ${Boost_LIBRARIES})
endif()

if(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
    # FreeBSD needs to find the correct GNU iconv library.
    find_package(Iconv REQUIRED)

    # The indidriver library is defined earlier in the file before the
    # find has been run. Add an explicit link to avoid runtime errors.
    target_link_libraries(indidriver ${ICONV_LIBRARIES})
endif(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
