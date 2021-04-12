
include_directories( ${INDI_INCLUDE_DIR})
include_directories("../../drivers/telescope/")

ADD_EXECUTABLE(test_scopesim_helper
    test_scopesim_helper.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/scopesim_helper.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/telescope_simulator.cpp"
)

TARGET_LINK_LIBRARIES(test_scopesim_helper
    indidriver
    ${GTEST_BOTH_LIBRARIES}
    ${GMOCK_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

ADD_TEST(test-scopesim test_scopesim_helper)


