
# ########## OnStepX (mount) ##########
add_executable(indi_onstepx
    main_mount.cpp
    OnStepXAlignment.cpp
    OnStepXAuxFeatures.cpp
    OnStepXCore.cpp
    OnStepXComm.cpp
    OnStepXConfig.cpp
    OnStepXDriverStatus.cpp
    OnStepXFocuser.cpp
    OnStepXGuide.cpp
    OnStepXInfo.cpp
    OnStepXLimits.cpp
    OnStepXMeridian.cpp
    OnStepXMount.cpp
    OnStepXModelBuilder.cpp
    OnStepXModelMath.cpp
    OnStepXModelFitter.cpp
    OnStepXModelProtocol.cpp
    OnStepXPec.cpp
    OnStepXRotator.cpp
    OnStepXRotatorDevice.cpp
    OnStepXPowerDevice.cpp
    OnStepXSite.cpp
    OnStepXStatus.cpp
    OnStepXTracking.cpp
    OnStepXWeather.cpp
    OnStepXUsbPlugin.cpp)

target_link_libraries(indi_onstepx indidriver)

install(TARGETS indi_onstepx RUNTIME DESTINATION bin)

# ########## OnStepX Controller (auxiliary, no mount) ##########
add_executable(indi_onstepx_aux
    main_aux.cpp
    OnStepXAuxFeatures.cpp
    OnStepXCore.cpp
    OnStepXComm.cpp
    OnStepXConfig.cpp
    OnStepXDriverStatus.cpp
    OnStepXFocuser.cpp
    OnStepXAux.cpp
    OnStepXRotator.cpp
    OnStepXRotatorDevice.cpp
    OnStepXPowerDevice.cpp
    OnStepXStatus.cpp
    OnStepXWeather.cpp
    OnStepXUsbPlugin.cpp)

target_link_libraries(indi_onstepx_aux indidriver)

install(TARGETS indi_onstepx_aux RUNTIME DESTINATION bin)
