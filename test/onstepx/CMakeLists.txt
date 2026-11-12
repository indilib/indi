
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

add_executable(test_onstepxsite
    test_onstepxsite.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/onstepx/OnStepXSite.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/onstepx/OnStepXComm.cpp"
)

target_link_libraries(test_onstepxsite
    indidriver
    ${GTEST_BOTH_LIBRARIES}
    ${GMOCK_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    ${NOVA_LIBRARIES}
)

add_test(test-onstepxsite test_onstepxsite)

add_executable(test_onstepxlimits
    test_onstepxlimits.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/onstepx/OnStepXLimits.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/onstepx/OnStepXComm.cpp"
)

target_link_libraries(test_onstepxlimits
    indidriver
    ${GTEST_BOTH_LIBRARIES}
    ${GMOCK_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

add_test(test-onstepxlimits test_onstepxlimits)

add_executable(test_onstepxguide
    test_onstepxguide.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/onstepx/OnStepXComm.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/onstepx/OnStepXGuide.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/onstepx/OnStepXWeather.cpp"
)

target_link_libraries(test_onstepxguide
    indidriver
    ${GTEST_BOTH_LIBRARIES}
    ${GMOCK_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

add_test(test-onstepxguide test_onstepxguide)

add_executable(test_onstepxtracking
    test_onstepxtracking.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/onstepx/OnStepXComm.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/onstepx/OnStepXStatus.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/onstepx/OnStepXTracking.cpp"
)

target_link_libraries(test_onstepxtracking
    indidriver
    ${GTEST_BOTH_LIBRARIES}
    ${GMOCK_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

add_test(test-onstepxtracking test_onstepxtracking)

add_executable(test_onstepxmeridian
    test_onstepxmeridian.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/onstepx/OnStepXComm.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/onstepx/OnStepXMeridian.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/onstepx/OnStepXStatus.cpp"
)

target_link_libraries(test_onstepxmeridian
    indidriver
    ${GTEST_BOTH_LIBRARIES}
    ${GMOCK_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

add_test(test-onstepxmeridian test_onstepxmeridian)

add_executable(test_onstepxdriverstatus
    test_onstepxdriverstatus.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/onstepx/OnStepXComm.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/onstepx/OnStepXDriverStatus.cpp"
)

target_link_libraries(test_onstepxdriverstatus
    indidriver
    ${GTEST_BOTH_LIBRARIES}
    ${GMOCK_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

add_test(test-onstepxdriverstatus test_onstepxdriverstatus)

add_executable(test_onstepxfocuser
    test_onstepxfocuser.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/onstepx/OnStepXComm.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/onstepx/OnStepXFocuser.cpp"
)

target_link_libraries(test_onstepxfocuser
    indidriver
    ${GTEST_BOTH_LIBRARIES}
    ${GMOCK_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

add_test(test-onstepxfocuser test_onstepxfocuser)

add_executable(test_onstepxrotator
    test_onstepxrotator.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/onstepx/OnStepXComm.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/onstepx/OnStepXRotator.cpp"
)

target_link_libraries(test_onstepxrotator
    indidriver
    ${GTEST_BOTH_LIBRARIES}
    ${GMOCK_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

add_test(test-onstepxrotator test_onstepxrotator)

add_executable(test_onstepxauxfeatures
    test_onstepxauxfeatures.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/onstepx/OnStepXComm.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/onstepx/OnStepXAuxFeatures.cpp"
)

target_link_libraries(test_onstepxauxfeatures
    indidriver
    ${GTEST_BOTH_LIBRARIES}
    ${GMOCK_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

add_test(test-onstepxauxfeatures test_onstepxauxfeatures)

add_executable(test_onstepxpec
    test_onstepxpec.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/onstepx/OnStepXComm.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/onstepx/OnStepXPec.cpp"
)

target_link_libraries(test_onstepxpec
    indidriver
    ${GTEST_BOTH_LIBRARIES}
    ${GMOCK_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

add_test(test-onstepxpec test_onstepxpec)

add_executable(test_onstepxalignment
    test_onstepxalignment.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/onstepx/OnStepXComm.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/onstepx/OnStepXAlignment.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/onstepx/OnStepXStatus.cpp"
)

target_link_libraries(test_onstepxalignment
    indidriver
    ${GTEST_BOTH_LIBRARIES}
    ${GMOCK_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

add_test(test-onstepxalignment test_onstepxalignment)

add_executable(test_onstepxusbplugin
    test_onstepxusbplugin.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/onstepx/OnStepXComm.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/onstepx/OnStepXUsbPlugin.cpp"
)
target_link_libraries(test_onstepxusbplugin
    indidriver
    ${GTEST_BOTH_LIBRARIES}
    ${GMOCK_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

add_test(test-onstepxusbplugin test_onstepxusbplugin)

add_executable(test_onstepxmodelmath
    test_onstepxmodelmath.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/onstepx/OnStepXModelMath.cpp"
)

target_link_libraries(test_onstepxmodelmath
    ${GTEST_BOTH_LIBRARIES}
    ${GTEST_MAIN_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

add_test(test-onstepxmodelmath test_onstepxmodelmath)

add_executable(test_onstepxmodelfitter
    test_onstepxmodelfitter.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/onstepx/OnStepXModelFitter.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/onstepx/OnStepXModelMath.cpp")

target_link_libraries(test_onstepxmodelfitter
    GTest::gtest_main)

add_test(test-onstepxmodelfitter test_onstepxmodelfitter)

add_executable(test_onstepxmodelprotocol
    test_onstepxmodelprotocol.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/onstepx/OnStepXModelProtocol.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/onstepx/OnStepXModelMath.cpp")

target_link_libraries(test_onstepxmodelprotocol
    GTest::gtest_main)

add_test(test-onstepxmodelprotocol test_onstepxmodelprotocol)

add_executable(test_onstepxmodelbuilder
    test_onstepxmodelbuilder.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/onstepx/OnStepXModelBuilder.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/onstepx/OnStepXModelProtocol.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/onstepx/OnStepXModelFitter.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/onstepx/OnStepXModelMath.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/onstepx/OnStepXComm.cpp"
)

target_link_libraries(test_onstepxmodelbuilder
    indidriver
    ${GTEST_BOTH_LIBRARIES}
    ${GMOCK_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

add_test(test-onstepxmodelbuilder test_onstepxmodelbuilder)

add_executable(test_onstepxformat
    test_onstepxformat.cpp
)

target_link_libraries(test_onstepxformat
    GTest::gtest_main)

add_test(test-onstepxformat test_onstepxformat)

add_executable(test_onstepxinfo
    test_onstepxinfo.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/onstepx/OnStepXComm.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/onstepx/OnStepXInfo.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/onstepx/OnStepXStatus.cpp"
)

target_link_libraries(test_onstepxinfo
    indidriver
    ${GTEST_BOTH_LIBRARIES}
    ${GMOCK_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

add_test(test-onstepxinfo test_onstepxinfo)

add_executable(test_onstepxrotatordevice
    test_onstepxrotatordevice.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/onstepx/OnStepXComm.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/onstepx/OnStepXRotator.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/onstepx/OnStepXRotatorDevice.cpp"
)

target_link_libraries(test_onstepxrotatordevice
    indidriver
    ${GTEST_BOTH_LIBRARIES}
    ${GMOCK_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

add_test(test-onstepxrotatordevice test_onstepxrotatordevice)

add_executable(test_onstepxpowerdevice
    test_onstepxpowerdevice.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/onstepx/OnStepXComm.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/onstepx/OnStepXAuxFeatures.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/onstepx/OnStepXUsbPlugin.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/telescope/onstepx/OnStepXPowerDevice.cpp"
)

target_link_libraries(test_onstepxpowerdevice
    indidriver
    ${GTEST_BOTH_LIBRARIES}
    ${GMOCK_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

add_test(test-onstepxpowerdevice test_onstepxpowerdevice)

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
