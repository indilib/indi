INCLUDE_DIRECTORIES( ${INDI_INCLUDE_DIR} )
INCLUDE_DIRECTORIES( "../../drivers/ccd" )

ADD_EXECUTABLE(test_ccd_simulator
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/ccd/sky_renderer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/ccd/ccd_simulator.cpp"
    test_ccd_simulator.cpp
)

TARGET_LINK_LIBRARIES(test_ccd_simulator
    indidriver
    ${GTEST_BOTH_LIBRARIES}
    ${GMOCK_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

ADD_TEST(test_ccd_simulator test_ccd_simulator)

ADD_EXECUTABLE(test_guide_simulator
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/ccd/sky_renderer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/ccd/guide_simulator.cpp"
    test_guide_simulator.cpp
)

TARGET_LINK_LIBRARIES(test_guide_simulator
    indidriver
    ${GTEST_BOTH_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

ADD_TEST(test_guide_simulator test_guide_simulator)

ADD_EXECUTABLE(test_telescope_simulator
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/telescope_simulator.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/scopesim_helper.cpp"
    test_telescope_simulator.cpp
)
TARGET_LINK_LIBRARIES(test_telescope_simulator
    AlignmentDriver
    indidriver
    ${GTEST_BOTH_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)
ADD_TEST(test_telescope_simulator test_telescope_simulator)

ADD_EXECUTABLE(test_dome_shutter_park_policy
    test_dome_shutter_park_policy.cpp
)
TARGET_LINK_LIBRARIES(test_dome_shutter_park_policy
    indidriver
    ${GTEST_BOTH_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)
ADD_TEST(test_dome_shutter_park_policy test_dome_shutter_park_policy)

ADD_EXECUTABLE(test_connection_serial
    test_connection_serial.cpp
)
TARGET_LINK_LIBRARIES(test_connection_serial
    indidriver
    ${GTEST_BOTH_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)
ADD_TEST(test_connection_serial test_connection_serial)

# The OnStep driver lives in indi_lx200generic, whose loader references every LX200 driver.
SET(LX200GENERIC_SOURCES
    lx200driver.cpp lx200autostar.cpp lx200_16.cpp lx200gps.cpp lx200generic.cpp lx200telescope.cpp
    lx200classic.cpp lx200gemini.cpp lx200zeq25.cpp lx200am5.cpp lx200gotonova.cpp lx200pulsar2.cpp
    lx200apdriver.cpp lx200ap_v2.cpp lx200ap_gtocp2.cpp lx200fs2.cpp lx200ss2000pc.cpp lx200_OnStep.cpp
    lx200_OpenAstroTech.cpp lx200_pegasus_nyx101.cpp lx200_10micron.cpp ioptronHC8406.cpp eq500x.cpp
    lx200_esp32go.cpp toup_sh20.cpp lx200_proxisky.cpp)
LIST(TRANSFORM LX200GENERIC_SOURCES PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/")
ADD_EXECUTABLE(test_lx200_onstep
    ${LX200GENERIC_SOURCES}
    test_lx200_onstep.cpp
)
IF(NOT CMAKE_SYSTEM_NAME MATCHES "OpenBSD|FreeBSD|Darwin")
    TARGET_COMPILE_DEFINITIONS(test_lx200_onstep PRIVATE -D_XOPEN_SOURCE=600 -D_POSIX_C_SOURCE=200809L)
ENDIF()
TARGET_LINK_LIBRARIES(test_lx200_onstep
    indidriver
    util
    ${GTEST_BOTH_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)
ADD_TEST(test_lx200_onstep test_lx200_onstep)
# The EFA test emulates the RTS/CTS lines on a pseudo terminal through Linux ioctl calls.
IF(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    ADD_EXECUTABLE(test_planewave_efa
        "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/focuser/planewave_efa.cpp"
        test_planewave_efa.cpp
    )
    TARGET_LINK_LIBRARIES(test_planewave_efa
        indidriver
        util
        ${GTEST_BOTH_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT}
    )
    ADD_TEST(test_planewave_efa test_planewave_efa)
ENDIF()
