
include_directories( ${INDI_INCLUDE_DIR})
include_directories("../../drivers/telescope/")

add_executable(test_lx200drivers
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/lx200driver.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/lx200autostar.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/lx200_16.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/lx200gps.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/lx200generic.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/lx200telescope.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/lx200classic.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/lx200apdriver.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/lx200gemini.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/lx200zeq25.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/lx200gotonova.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/lx200pulsar2.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/lx200ap_experimentaldriver.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/lx200ap_experimental.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/lx200ap.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/lx200ap_gtocp2.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/lx200fs2.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/lx200ss2000pc.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/lx200_OnStep.cpp"    
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/lx200_10micron.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/ioptronHC8406.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/eq500x.cpp"
    test_eq500xdriver.cpp
)

target_link_libraries(test_lx200drivers
    indidriver
    ${GTEST_BOTH_LIBRARIES}
    ${GMOCK_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

add_test(test-lx200drivers test_lx200drivers)
