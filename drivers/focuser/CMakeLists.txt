# ############### Focuser Simulator ################
SET(focussimulator_SRC
    focus_simulator.cpp)

add_executable(indi_simulator_focus ${focussimulator_SRC})
target_link_libraries(indi_simulator_focus indidriver)
install(TARGETS indi_simulator_focus RUNTIME DESTINATION bin)

# ############### Robo Focuser ################
SET(robofocus_SRC
    robofocus.cpp)

add_executable(indi_robo_focus ${robofocus_SRC})
target_link_libraries(indi_robo_focus indidriver)
install(TARGETS indi_robo_focus RUNTIME DESTINATION bin)

# ############### Robo Focuser ################
# ############### iEAF Focuser ################
SET(ieaffocus_SRC
    ieaffocus.cpp)

add_executable(indi_ieaf_focus ${ieaffocus_SRC})
target_link_libraries(indi_ieaf_focus indidriver)
install(TARGETS indi_ieaf_focus RUNTIME DESTINATION bin)


# ############### iEAF Focuser ################
SET(fcusb_SRC
    fcusb.cpp)

add_executable(indi_fcusb_focus ${fcusb_SRC})
target_link_libraries(indi_fcusb_focus indidriver ${HIDAPILIB})
install(TARGETS indi_fcusb_focus RUNTIME DESTINATION bin)

# ############### Rigelsys NFocus Focuser ################
SET(nfocus_SRC
    nfocus.cpp)

add_executable(indi_nfocus ${nfocus_SRC})
target_link_libraries(indi_nfocus indidriver)
install(TARGETS indi_nfocus RUNTIME DESTINATION bin)

# ############### Rigelsys NStep Focuser ################
SET(nstep_SRC
    nstep.cpp)

add_executable(indi_nstep_focus ${nstep_SRC})
target_link_libraries(indi_nstep_focus indidriver)
install(TARGETS indi_nstep_focus RUNTIME DESTINATION bin)

# ############### PlaneWave EFA ################
SET(efa_SRC
    planewave_efa.cpp)

add_executable(indi_efa_focus ${efa_SRC})
target_link_libraries(indi_efa_focus indidriver)
install(TARGETS indi_efa_focus RUNTIME DESTINATION bin)

# ############### Celestron SCT Focuser ################
SET(celestronsct_SRC
    celestronauxpacket.cpp
    celestron.cpp)

add_executable(indi_celestron_sct_focus ${celestronsct_SRC})
target_link_libraries(indi_celestron_sct_focus indidriver)
install(TARGETS indi_celestron_sct_focus RUNTIME DESTINATION bin)

# ############### AAF2 Focuser ################
SET(aaf2_SRC
    aaf2.cpp)

add_executable(indi_aaf2_focus ${aaf2_SRC})
target_link_libraries(indi_aaf2_focus indidriver)
install(TARGETS indi_aaf2_focus RUNTIME DESTINATION bin)

# ############### AAF2 Focuser ################
SET(rbfocus_SRC
    rbfocus.cpp)

add_executable(indi_rbfocus_focus ${rbfocus_SRC})
target_link_libraries(indi_rbfocus_focus indidriver)
install(TARGETS indi_rbfocus_focus RUNTIME DESTINATION bin)

# ############### Astromech Focuser ################
SET(astromech_focuser_SRC
    astromech_focuser.cpp)

add_executable(indi_astromechfoc ${astromech_focuser_SRC})
target_link_libraries(indi_astromechfoc indidriver)
install(TARGETS indi_astromechfoc RUNTIME DESTINATION bin)

# ############### Pinefeat CEF Focuser ################
SET(pinefeat_cef_SRC
    pinefeat_cef.cpp)

add_executable(indi_pinefeat_cef_focus ${pinefeat_cef_SRC})
target_link_libraries(indi_pinefeat_cef_focus indidriver)
install(TARGETS indi_pinefeat_cef_focus RUNTIME DESTINATION bin)

# ############### Moonlite Focuser ################
SET(moonlite_SRC
    moonlite.cpp)

add_executable(indi_moonlite_focus ${moonlite_SRC})
target_link_libraries(indi_moonlite_focus indidriver)
install(TARGETS indi_moonlite_focus RUNTIME DESTINATION bin)

# ############### Moonlite DRO Dual Focuser ################
SET(moonlitedro_SRC
    moonlite_dro.cpp)

add_executable(indi_moonlitedro_focus ${moonlitedro_SRC})
target_link_libraries(indi_moonlitedro_focus indidriver)
install(TARGETS indi_moonlitedro_focus RUNTIME DESTINATION bin)

# ############### MyFocuserPro2 Focuser ################
SET(myfocuserpro2_SRC
    myfocuserpro2.cpp)

add_executable(indi_myfocuserpro2_focus ${myfocuserpro2_SRC})
target_link_libraries(indi_myfocuserpro2_focus indidriver)
install(TARGETS indi_myfocuserpro2_focus RUNTIME DESTINATION bin)

# ############### OnFocus Focuser ################
SET(onfocus_SRC
    onfocus.cpp)

add_executable(indi_onfocus_focus ${onfocus_SRC})
target_link_libraries(indi_onfocus_focus indidriver)
install(TARGETS indi_onfocus_focus RUNTIME DESTINATION bin)

# ############### TeenAstro Focuser ################
SET(teenastro_focus_SRC
    teenastro.cpp)

add_executable(indi_teenastro_focus ${teenastro_focus_SRC})
target_link_libraries(indi_teenastro_focus indidriver)
install(TARGETS indi_teenastro_focus RUNTIME DESTINATION bin)

# ############### Sesto Senso Focuser ################
SET(sesto_SRC
    sestosenso.cpp)

add_executable(indi_sestosenso_focus ${sesto_SRC})
target_link_libraries(indi_sestosenso_focus indidriver)
install(TARGETS indi_sestosenso_focus RUNTIME DESTINATION bin)

# ############### Sesto Senso 2 Focuser ################
SET(sesto2_SRC
    sestosenso2.cpp
    primalucacommandset.cpp
)

add_executable(indi_sestosenso2_focus ${sesto2_SRC})
target_link_libraries(indi_sestosenso2_focus indidriver ${JSONLIB})
install(TARGETS indi_sestosenso2_focus RUNTIME DESTINATION bin)

# ############### Esatto Focuser ################
SET(esatto_SRC
    esatto.cpp
    primalucacommandset.cpp
)

add_executable(indi_esatto_focus ${esatto_SRC})
target_link_libraries(indi_esatto_focus indidriver ${JSONLIB})
install(TARGETS indi_esatto_focus RUNTIME DESTINATION bin)

# ############### Esatto with Arco Focuser ################
SET(esarc_SRC
    esattoarco.cpp
    primalucacommandset.cpp
)

add_executable(indi_esattoarco_focus ${esarc_SRC})
target_link_libraries(indi_esattoarco_focus indidriver ${JSONLIB})
install(TARGETS indi_esattoarco_focus RUNTIME DESTINATION bin)

# ############### Televue FocusMaster ################
# SET(focusmaster_SRC
# focusmaster.cpp)

# add_executable(indi_focusmaster_focus ${focusmaster_SRC})
# target_link_libraries(indi_focusmaster_focus indidriver ${HIDAPILIB})
# install(TARGETS indi_focusmaster_focus RUNTIME DESTINATION bin)

# ############### RainbowAstro RSF Focuser ################
SET(RSF_SRC
    rainbowRSF.cpp)

add_executable(indi_rainbowrsf_focus ${RSF_SRC})
target_link_libraries(indi_rainbowrsf_focus indidriver)
install(TARGETS indi_rainbowrsf_focus RUNTIME DESTINATION bin)

# ############### Dream Focuser ################
SET(dreamfocuser_SRC
    dreamfocuser.cpp)

add_executable(indi_dreamfocuser_focus ${dreamfocuser_SRC})
target_link_libraries(indi_dreamfocuser_focus indidriver)
install(TARGETS indi_dreamfocuser_focus RUNTIME DESTINATION bin)

# ########## Lakeside ###########
set(indilakeside_SRCS lakeside.cpp)

add_executable(indi_lakeside_focus ${indilakeside_SRCS})

IF(NOT "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" AND NOT "${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
    set_target_properties(indi_lakeside_focus PROPERTIES COMPILE_FLAGS "-Wno-format-overflow")
ENDIF()

target_link_libraries(indi_lakeside_focus indidriver)
install(TARGETS indi_lakeside_focus RUNTIME DESTINATION bin)

# ############### Pegasus DMFC Focuser ################
SET(pegasus_SRC
    dmfc.cpp)

add_executable(indi_dmfc_focus ${pegasus_SRC})
target_link_libraries(indi_dmfc_focus indidriver)
install(TARGETS indi_dmfc_focus RUNTIME DESTINATION bin)

# ############### Pegasus FocusCube Focuser ################
SET(pegasusfocuscube_SRC
    pegasus_focuscube.cpp)

add_executable(indi_pegasus_focuscube ${pegasusfocuscube_SRC})
target_link_libraries(indi_pegasus_focuscube indidriver)
install(TARGETS indi_pegasus_focuscube RUNTIME DESTINATION bin)

# ############### Pegasus FocusCube3 Focuser ################
SET(pegasusfocuscube3_SRC
    pegasus_focuscube3.cpp)

add_executable(indi_pegasus_focuscube3 ${pegasusfocuscube3_SRC})
target_link_libraries(indi_pegasus_focuscube3 indidriver)
install(TARGETS indi_pegasus_focuscube3 RUNTIME DESTINATION bin)

# ############### Pegasus ProdigyMF Focuser ################
SET(pegasusprodigyMF_SRC
    pegasus_prodigyMF.cpp)

add_executable(indi_pegasus_prodigyMF ${pegasusprodigyMF_SRC})
target_link_libraries(indi_pegasus_prodigyMF indidriver)
install(TARGETS indi_pegasus_prodigyMF RUNTIME DESTINATION bin)

# ############### Pegasus ScopsOAG Focuser ################
SET(pegasusScopsoag_SRC
    pegasus_scopsoag.cpp)

add_executable(indi_pegasus_scopsoag ${pegasusScopsoag_SRC})
target_link_libraries(indi_pegasus_scopsoag indidriver)
install(TARGETS indi_pegasus_scopsoag RUNTIME DESTINATION bin)

# ################# USB Focus V3 ##################
SET(usbfocusv3_SRC
    usbfocusv3.cpp)

add_executable(indi_usbfocusv3_focus ${usbfocusv3_SRC})
target_link_libraries(indi_usbfocusv3_focus indidriver)
install(TARGETS indi_usbfocusv3_focus RUNTIME DESTINATION bin)

# ############### Microtouch Focuser ################
SET(microtouch_SRC
    microtouch.cpp)

add_executable(indi_microtouch_focus ${microtouch_SRC})
target_link_libraries(indi_microtouch_focus indidriver)
install(TARGETS indi_microtouch_focus RUNTIME DESTINATION bin)

# ############### Baader SteelDrive II Focuser ################
SET(steeldrive2_SRC
    steeldrive2.cpp)

add_executable(indi_steeldrive2_focus ${steeldrive2_SRC})
target_link_libraries(indi_steeldrive2_focus indidriver)
install(TARGETS indi_steeldrive2_focus RUNTIME DESTINATION bin)

# ############### Baader SteelDrive Focuser ################
SET(steeldrive_SRC
    steeldrive.cpp)

add_executable(indi_steeldrive_focus ${steeldrive_SRC})
target_link_libraries(indi_steeldrive_focus indidriver)
install(TARGETS indi_steeldrive_focus RUNTIME DESTINATION bin)

################ Alluna TCS2 Focuser ################

SET(allunatcs2_SRC
    alluna_tcs2.cpp)

add_executable(indi_alluna_tcs2 ${allunatcs2_SRC})
target_link_libraries(indi_alluna_tcs2 indidriver)
install(TARGETS indi_alluna_tcs2 RUNTIME DESTINATION bin)

# ############### FocusLynx Focuser ################
SET(focuslynx_SRC
    focuslynxbase.cpp
    focuslynx.cpp)

IF(UNITY_BUILD)
    ENABLE_UNITY_BUILD(focuslynx focuslynx_SRC 10 cpp)
ENDIF()

add_executable(indi_lynx_focus ${focuslynx_SRC})
target_link_libraries(indi_lynx_focus indidriver)
install(TARGETS indi_lynx_focus RUNTIME DESTINATION bin)

# ############### PerfectStar Focuser ################
SET(perfectstar_SRC
    perfectstar.cpp)

add_executable(indi_perfectstar_focus ${perfectstar_SRC})
target_link_libraries(indi_perfectstar_focus indidriver ${HIDAPILIB})
install(TARGETS indi_perfectstar_focus RUNTIME DESTINATION bin)

# ############### Starlight Instruments EFS ################
SET(siefs_SRC
    si_efs.cpp)

add_executable(indi_siefs_focus ${siefs_SRC})
target_link_libraries(indi_siefs_focus indidriver ${HIDAPILIB})
install(TARGETS indi_siefs_focus RUNTIME DESTINATION bin)

# ############### hitechfocus Focuser ################
SET(hitecastrodcfocuser_SRC
    hitecastrodcfocuser.cpp)

add_executable(indi_hitecastrodc_focus ${hitecastrodcfocuser_SRC})
target_link_libraries(indi_hitecastrodc_focus indidriver ${HIDAPILIB})
install(TARGETS indi_hitecastrodc_focus RUNTIME DESTINATION bin)

# ############### DeepSky Dad AF1 ################
SET(deepskydad_af1_SRC
    deepskydad_af1.cpp)

add_executable(indi_deepskydad_af1_focus ${deepskydad_af1_SRC})
target_link_libraries(indi_deepskydad_af1_focus indidriver)
install(TARGETS indi_deepskydad_af1_focus RUNTIME DESTINATION bin)

# ############### DeepSky Dad AF2 ################
SET(deepskydad_af2_SRC
    deepskydad_af2.cpp)

add_executable(indi_deepskydad_af2_focus ${deepskydad_af2_SRC})
target_link_libraries(indi_deepskydad_af2_focus indidriver)
install(TARGETS indi_deepskydad_af2_focus RUNTIME DESTINATION bin)

# ############### DeepSky Dad AF3 ################
SET(deepskydad_af3_SRC
    deepskydad_af3.cpp)

add_executable(indi_deepskydad_af3_focus ${deepskydad_af3_SRC})
target_link_libraries(indi_deepskydad_af3_focus indidriver)
install(TARGETS indi_deepskydad_af3_focus RUNTIME DESTINATION bin)

# ############### JMI Smart Focus Focuser ################
SET(smartfocus_SRC
    smartfocus.cpp)

add_executable(indi_smartfocus_focus ${smartfocus_SRC})
target_link_libraries(indi_smartfocus_focus indidriver)
install(TARGETS indi_smartfocus_focus RUNTIME DESTINATION bin)

# ############### Optec TCF-S ################
SET(tcfs_SRC
    tcfs.cpp)

add_executable(indi_tcfs_focus ${tcfs_SRC})

target_link_libraries(indi_tcfs_focus indidriver)
install(TARGETS indi_tcfs_focus RUNTIME DESTINATION bin)

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/make_tcfs_symlink.cmake
    "exec_program(\"${CMAKE_COMMAND}\" ARGS -E create_symlink indi_tcfs_focus \$ENV{DESTDIR}${BIN_INSTALL_DIR}/indi_tcfs3_focus)\n")
set_target_properties(indi_tcfs_focus PROPERTIES POST_INSTALL_SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/make_tcfs_symlink.cmake)

# ############### Lacerta MFOC Focuser ################
SET(lacerta_mfoc_SRC
    lacerta_mfoc.cpp)

add_executable(indi_lacerta_mfoc_focus ${lacerta_mfoc_SRC})
target_link_libraries(indi_lacerta_mfoc_focus indidriver)
install(TARGETS indi_lacerta_mfoc_focus RUNTIME DESTINATION bin)

# ############### Lacerta MFOC & FMC Focuser ################
SET(lacerta_mfoc_fmc_SRC
    lacerta_mfoc_fmc.cpp)

add_executable(indi_lacerta_mfoc_fmc_focus ${lacerta_mfoc_fmc_SRC})
target_link_libraries(indi_lacerta_mfoc_fmc_focus indidriver)
install(TARGETS indi_lacerta_mfoc_fmc_focus RUNTIME DESTINATION bin)

# ############### Active Focuser ################
SET(activefocuser_SRC
    activefocuser.cpp
    activefocuser_utils.cpp
    activefocuser_utils.h)

add_executable(indi_activefocuser_focus ${activefocuser_SRC})

target_link_libraries(indi_activefocuser_focus indidriver ${HIDAPILIB})
install(TARGETS indi_activefocuser_focus RUNTIME DESTINATION bin)
