
include_directories(${INDI_INCLUDE_DIR})
include_directories("../../drivers/telescope/onstepx/")

add_executable(test_onstepxcomm
    test_onstepxcomm.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/onstepx/OnStepXComm.cpp"
)

target_link_libraries(test_onstepxcomm
    indidriver
    ${GTEST_BOTH_LIBRARIES}
    ${GMOCK_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

add_test(test-onstepxcomm test_onstepxcomm)

add_executable(test_onstepxprobe
    test_onstepxprobe.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/onstepx/OnStepXCore.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/onstepx/OnStepXComm.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/onstepx/OnStepXStatus.cpp"
)

target_link_libraries(test_onstepxprobe
    indidriver
    ${GTEST_BOTH_LIBRARIES}
    ${GMOCK_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

add_test(test-onstepxprobe test_onstepxprobe)

add_executable(test_onstepxstatus
    test_onstepxstatus.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/onstepx/OnStepXStatus.cpp"
)

target_link_libraries(test_onstepxstatus
    indidriver
    ${GTEST_BOTH_LIBRARIES}
    ${GMOCK_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

add_test(test-onstepxstatus test_onstepxstatus)

add_executable(test_onstepxformat
    test_onstepxformat.cpp
)

target_link_libraries(test_onstepxformat
    GTest::gtest_main)

add_test(test-onstepxformat test_onstepxformat)

add_executable(test_onstepxconfig
    test_onstepxconfig.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/onstepx/OnStepXConfig.cpp"
)

target_link_libraries(test_onstepxconfig
    indidriver
    ${GTEST_BOTH_LIBRARIES}
    ${GMOCK_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

add_test(test-onstepxconfig test_onstepxconfig)
