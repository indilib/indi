
include_directories( ${INDI_INCLUDE_DIR})
include_directories("../../drivers/telescope/")

add_executable(test_celestrondriver
    test_celestrondriver.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/celestrondriver.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/celestrongps.cpp"
)

target_link_libraries(test_celestrondriver
    indidriver
    ${GTEST_BOTH_LIBRARIES}
    ${GMOCK_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

add_test(test-celestrondriver test_celestrondriver)
