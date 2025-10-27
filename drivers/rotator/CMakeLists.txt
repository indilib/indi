# ############### Rigelsys nFrame Rotator ################
SET(nframe_SRC
    nframe.cpp)

add_executable(indi_nframe_rotator ${nframe_SRC})
target_link_libraries(indi_nframe_rotator indidriver)
install(TARGETS indi_nframe_rotator RUNTIME DESTINATION bin)

# ############### Rotator Simulator ########
SET(rotatorsim_SRC
    rotator_simulator.cpp)

IF(UNITY_BUILD)
    ENABLE_UNITY_BUILD(rotatorsim rotatorsim_SRC 10 cpp)
ENDIF()

add_executable(indi_simulator_rotator ${rotatorsim_SRC})
target_link_libraries(indi_simulator_rotator indidriver)
install(TARGETS indi_simulator_rotator RUNTIME DESTINATION bin)

# ############### Optec Gemini Focusing Rotator ########
SET(gemini_SRC
    gemini.cpp)

IF(UNITY_BUILD)
    ENABLE_UNITY_BUILD(gemini gemini_SRC 10 cpp)
ENDIF()

add_executable(indi_gemini_focus ${gemini_SRC})
target_link_libraries(indi_gemini_focus indidriver)
install(TARGETS indi_gemini_focus RUNTIME DESTINATION bin)

# ############### iOptron Focusing Rotator ########
SET(iafscaa_SRC
    iafscaa.cpp)

IF(UNITY_BUILD)
    ENABLE_UNITY_BUILD(iafscaa iafscaa_SRC 10 cpp)
ENDIF()

add_executable(indi_iafscaa_focus ${iafscaa_SRC})
target_link_libraries(indi_iafscaa_focus indidriver)
install(TARGETS indi_iafscaa_focus RUNTIME DESTINATION bin)
# ############### NightCrawler Focusing Rotator ################
SET(nightcrawler_SRC
    nightcrawler.cpp)

add_executable(indi_nightcrawler_focus ${nightcrawler_SRC})
target_link_libraries(indi_nightcrawler_focus indidriver)
install(TARGETS indi_nightcrawler_focus RUNTIME DESTINATION bin)

# ############### Pyxis Rotator ################
SET(pyxis_SRC
    pyxis.cpp)

add_executable(indi_pyxis_rotator ${pyxis_SRC})
target_link_libraries(indi_pyxis_rotator indidriver)
install(TARGETS indi_pyxis_rotator RUNTIME DESTINATION bin)

# ############### Falcon Rotator ################
SET(falcon_SRC
    pegasus_falcon.cpp)

add_executable(indi_falcon_rotator ${falcon_SRC})
target_link_libraries(indi_falcon_rotator indidriver)
install(TARGETS indi_falcon_rotator RUNTIME DESTINATION bin)

# ############### Camelot Rotator ################
SET(camelot_SRC
    camelot.cpp)

add_executable(indi_camelot_rotator ${camelot_SRC})
target_link_libraries(indi_camelot_rotator indidriver)
install(TARGETS indi_camelot_rotator RUNTIME DESTINATION bin)

# ############### Falcon V2 Rotator ################
SET(falconv2_SRC
    pegasus_falconv2.cpp)

add_executable(indi_falconv2_rotator ${falconv2_SRC})
target_link_libraries(indi_falconv2_rotator indidriver)
install(TARGETS indi_falconv2_rotator RUNTIME DESTINATION bin)

# ############### Wanderer Rotator Lite ################
SET(WandererRotatorLite_SRC
    wanderer_rotator_base.cpp
    wanderer_rotator_lite.cpp)

add_executable(indi_wanderer_lite_rotator ${WandererRotatorLite_SRC})
target_link_libraries(indi_wanderer_lite_rotator indidriver)
install(TARGETS indi_wanderer_lite_rotator RUNTIME DESTINATION bin)

# ############### Wanderer Rotator Mini ################
SET(WandererRotatorMini_SRC
    wanderer_rotator_base.cpp
    wanderer_rotator_mini.cpp)

add_executable(indi_wanderer_rotator_mini ${WandererRotatorMini_SRC})
target_link_libraries(indi_wanderer_rotator_mini indidriver)
install(TARGETS indi_wanderer_rotator_mini RUNTIME DESTINATION bin)

# ############### Wanderer Rotator Lite V2 ################
SET(WandererRotatorLiteV2_SRC
    wanderer_rotator_base.cpp
    wanderer_rotator_lite_v2.cpp)

add_executable(indi_wanderer_rotator_lite_v2 ${WandererRotatorLiteV2_SRC})
target_link_libraries(indi_wanderer_rotator_lite_v2 indidriver)
install(TARGETS indi_wanderer_rotator_lite_v2 RUNTIME DESTINATION bin)


# ############### Integra85 Focusing Rotator ################
SET(integra_SRC
    integra.cpp)

add_executable(indi_integra_focus ${integra_SRC})
target_link_libraries(indi_integra_focus indidriver)
install(TARGETS indi_integra_focus RUNTIME DESTINATION bin)

# ########## Deep Sky Dad FR1 ###############
SET(deepskydad_fr1_SRC
    deepskydad_fr1.cpp)

add_executable(indi_deepskydad_fr1 ${deepskydad_fr1_SRC})
target_link_libraries(indi_deepskydad_fr1 indidriver)
install(TARGETS indi_deepskydad_fr1 RUNTIME DESTINATION bin)
