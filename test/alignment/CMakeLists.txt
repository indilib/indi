INCLUDE_DIRECTORIES( ${INDI_INCLUDE_DIR} )

ADD_EXECUTABLE(test_alignment
    test_alignment.cpp
)

TARGET_LINK_LIBRARIES(test_alignment
    indidriver
    AlignmentDriver
    ${GTEST_BOTH_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

ADD_TEST(test-alignment test_alignment)
