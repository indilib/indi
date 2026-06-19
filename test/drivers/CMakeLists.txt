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
