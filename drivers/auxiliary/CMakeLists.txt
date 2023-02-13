IF(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    INSTALL(FILES 99-indi_auxiliary.rules DESTINATION ${UDEVRULES_INSTALL_DIR})
ENDIF()

# ########## Wanderer Cover V3 ###############
SET(wandererCover_SRC
    wanderer_cover.cpp)

add_executable(indi_wanderer_cover ${wandererCover_SRC})
target_link_libraries(indi_wanderer_cover indidriver)
install(TARGETS indi_wanderer_cover RUNTIME DESTINATION bin)

# ########## SkySafari Middleware ##############
IF(INDI_BUILD_CLIENT)
    SET(skysafari_SRC
        skysafari.cpp
        skysafariclient.cpp)

    IF(UNITY_BUILD)
        ENABLE_UNITY_BUILD(skysafari skysafari_SRC 10 cpp)
    ENDIF()

    add_executable(indi_skysafari ${skysafari_SRC})
    target_link_libraries(indi_skysafari indidriver indiclient)
    install(TARGETS indi_skysafari RUNTIME DESTINATION bin)
ELSE()
    MESSAGE(WARNING "Skipping build of INDI SkySafari driver since INDI POSIX Client is not built")
ENDIF()

# ########## Watch dog ###############
if(INDI_BUILD_CLIENT)
    SET(watchdog_SRC
        watchdog.cpp
        watchdogclient.cpp)

    IF(UNITY_BUILD)
        ENABLE_UNITY_BUILD(watchdog watchdog_SRC 10 cpp)
    ENDIF()

    add_executable(indi_watchdog ${watchdog_SRC})

    target_link_libraries(indi_watchdog indidriver indiclient)
    install(TARGETS indi_watchdog RUNTIME DESTINATION bin)
ELSE()
    MESSAGE(WARNING "Skipping build of INDI WatchDog driver since INDI POSIX Client is not built")
ENDIF()

# ########## Flip Flat & Flip Man Driver ###############
SET(flipflat_SRC
    flip_flat.cpp)

add_executable(indi_flipflat ${flipflat_SRC})
target_link_libraries(indi_flipflat indidriver)
install(TARGETS indi_flipflat RUNTIME DESTINATION bin)

# ########## Light Panel Simulator ###############
SET(lightpanelsimulator_SRC
    light_panel_simulator.cpp)

add_executable(indi_simulator_lightpanel ${lightpanelsimulator_SRC})
target_link_libraries(indi_simulator_lightpanel indidriver)
install(TARGETS indi_simulator_lightpanel RUNTIME DESTINATION bin)

# ########## Pegasus Ultimate Power Box Driver ###############
SET(pegasus_upb_SRC
    pegasus_upb.cpp)

add_executable(indi_pegasus_upb ${pegasus_upb_SRC})
target_link_libraries(indi_pegasus_upb indidriver)
install(TARGETS indi_pegasus_upb RUNTIME DESTINATION bin)

# ########## Pegasus USB Control Hub  ###############
SET(pegasus_uch_SRC
    pegasus_uch.cpp)

add_executable(indi_pegasus_uch ${pegasus_uch_SRC})
target_link_libraries(indi_pegasus_uch indidriver)
install(TARGETS indi_pegasus_uch RUNTIME DESTINATION bin)

# ########## Pegasus FlatMaster ###############
SET(pegasus_fm_SRC
    pegasus_flatmaster.cpp)

add_executable(indi_pegasus_flatmaster ${pegasus_fm_SRC})
target_link_libraries(indi_pegasus_flatmaster indidriver)
install(TARGETS indi_pegasus_flatmaster RUNTIME DESTINATION bin)

# ########## Pegasus Pocket Power Box Driver ###############
SET(pegasus_ppb_SRC
    pegasus_ppb.cpp)

add_executable(indi_pegasus_ppb ${pegasus_ppb_SRC})
target_link_libraries(indi_pegasus_ppb indidriver)
install(TARGETS indi_pegasus_ppb RUNTIME DESTINATION bin)

# ########## Pegasus Pocket Power Box Advance Driver ###############
SET(pegasus_ppba_SRC
    pegasus_ppba.cpp)

add_executable(indi_pegasus_ppba ${pegasus_ppba_SRC})
target_link_libraries(indi_pegasus_ppba indidriver)
install(TARGETS indi_pegasus_ppba RUNTIME DESTINATION bin)

# ########## RB Focus Excalibur ###############
SET(Excalibur_SRC
    Excalibur.cpp)

add_executable(indi_Excalibur ${Excalibur_SRC})
target_link_libraries(indi_Excalibur indidriver)
install(TARGETS indi_Excalibur RUNTIME DESTINATION bin)

# ########## SnapCap Driver ###############
SET(snapcap_SRC
    snapcap.cpp)

add_executable(indi_snapcap ${snapcap_SRC})
target_link_libraries(indi_snapcap indidriver)
install(TARGETS indi_snapcap RUNTIME DESTINATION bin)

# ########## Light Pollution Meter #############
SET(lpm_SRC
    lpm.cpp)

add_executable(indi_astromech_lpm ${lpm_SRC})
target_link_libraries(indi_astromech_lpm indidriver)
install(TARGETS indi_astromech_lpm RUNTIME DESTINATION bin)

# ########## Sky Quality Meter ###############
SET(sqm_SRC
    sqm.cpp)

add_executable(indi_sqm_weather ${sqm_SRC})
target_link_libraries(indi_sqm_weather indidriver)
install(TARGETS indi_sqm_weather RUNTIME DESTINATION bin)

# ########## Sky Quality Meter Simulator ###############
SET(sqm_simulator_SRC
    sqm_simulator.cpp)

add_executable(indi_simulator_sqm ${sqm_simulator_SRC})
target_link_libraries(indi_simulator_sqm indidriver)
install(TARGETS indi_simulator_sqm RUNTIME DESTINATION bin)

# ########## Astrometry Driver ###############
SET(astrometry_SRC
    astrometrydriver.cpp)

add_executable(indi_astrometry ${astrometry_SRC})
target_link_libraries(indi_astrometry indidriver)
install(TARGETS indi_astrometry RUNTIME DESTINATION bin)

# ########## STAR2000 Driver ###############
SET(STAR2000_SRC
    STAR2kdriver.c
    STAR2000.cpp)

add_executable(indi_star2000 ${STAR2000_SRC})
target_link_libraries(indi_star2000 indidriver)
install(TARGETS indi_star2000 RUNTIME DESTINATION bin)

# ########## Arduino ST4 Driver ###############
SET(arduinost4_SRC arduino_st4.cpp)

IF(UNITY_BUILD)
    ENABLE_UNITY_BUILD(arduinost4 arduinost4_SRC 10 cpp)
ENDIF()

add_executable(indi_arduinost4 ${arduinost4_SRC})
target_link_libraries(indi_arduinost4 indidriver)
install(TARGETS indi_arduinost4 RUNTIME DESTINATION bin)

# ########## PlaneWave Delta-T Driver ###############
SET(planewave_deltat_SRC planewave_delta.cpp)

IF(UNITY_BUILD)
    ENABLE_UNITY_BUILD(indi_planewave_deltat planewave_deltat_SRC 10 cpp)
ENDIF()

add_executable(indi_planewave_deltat ${planewave_deltat_SRC})
target_link_libraries(indi_planewave_deltat indidriver)
install(TARGETS indi_planewave_deltat RUNTIME DESTINATION bin)

# ########## GPUSB Driver ###############
SET(gpusb_SRC
    gpdriver.cpp
    gpusb.cpp)

IF(UNITY_BUILD)
    ENABLE_UNITY_BUILD(gpusb gpusb_SRC 10 cpp)
ENDIF()

add_executable(indi_gpusb ${gpusb_SRC})
target_link_libraries(indi_gpusb indidriver)
install(TARGETS indi_gpusb RUNTIME DESTINATION bin)

# ########## Joystick Driver ###############
IF(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    SET(joystick_SRC
        joystickdriver.cpp
        joystick.cpp)

    IF(UNITY_BUILD)
        ENABLE_UNITY_BUILD(joystick joystick_SRC 10 cpp)
    ENDIF()

    add_executable(indi_joystick ${joystick_SRC})
    target_link_libraries(indi_joystick indidriver)
    install(TARGETS indi_joystick RUNTIME DESTINATION bin)
ENDIF()

# ########## GPS Simulator Driver ###############
SET(gpssimulator_SRC
    gps_simulator.cpp)

add_executable(indi_simulator_gps ${gpssimulator_SRC})
target_link_libraries(indi_simulator_gps indidriver)
install(TARGETS indi_simulator_gps RUNTIME DESTINATION bin)

# ########## USB_Dewpoint Driver ###############
SET(usb_dewpoint_SRC
    usb_dewpoint.cpp)

add_executable(indi_usbdewpoint ${usb_dewpoint_SRC})
target_link_libraries(indi_usbdewpoint indidriver)
install(TARGETS indi_usbdewpoint RUNTIME DESTINATION bin)

# ########## myDewController Pro Driver ###############
SET(myDewControllerPro_SRC
    myDewControllerPro.cpp)

add_executable(indi_myDewControllerPro ${myDewControllerPro_SRC})
target_link_libraries(indi_myDewControllerPro indidriver)
install(TARGETS indi_myDewControllerPro RUNTIME DESTINATION bin)


# ########## Deep Sky Dad FP1 ###############
SET(deepskydad_fp1_SRC
    deepskydad_fp1.cpp)

add_executable(indi_deepskydad_fp1 ${deepskydad_fp1_SRC})
target_link_libraries(indi_deepskydad_fp1 indidriver)
install(TARGETS indi_deepskydad_fp1 RUNTIME DESTINATION bin)

# ########## GIOTTO ###############
SET(giotto_SRC
    giotto.cpp
    ../focuser/primalucacommandset.cpp
)

add_executable(indi_giotto ${giotto_SRC})
target_link_libraries(indi_giotto indidriver)
install(TARGETS indi_giotto RUNTIME DESTINATION bin)
