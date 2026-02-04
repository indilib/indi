# ########## CCD Simulator ##############
SET(ccdsimulator_SRC
    ccd_simulator.cpp)

add_executable(indi_simulator_ccd ${ccdsimulator_SRC})
target_link_libraries(indi_simulator_ccd indidriver)
install(TARGETS indi_simulator_ccd RUNTIME DESTINATION bin)

# ########## Guide Simulator ##############
SET(guidesimulator_SRC
    guide_simulator.cpp)

add_executable(indi_simulator_guide ${guidesimulator_SRC})
target_link_libraries(indi_simulator_guide indidriver)
install(TARGETS indi_simulator_guide RUNTIME DESTINATION bin)

# ########## Alpaca CCD ##############
SET(alpacaccd_SRC
    indi_alpaca_ccd.cpp)

add_executable(indi_alpaca_ccd ${alpacaccd_SRC})
target_link_libraries(indi_alpaca_ccd indidriver ${HTTPLIB_LIBRARY})
install(TARGETS indi_alpaca_ccd RUNTIME DESTINATION bin)
