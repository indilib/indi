# ########## XAGYL Wheel ##############
SET(xagylwheel_SRC
    xagyl_wheel.cpp)

add_executable(indi_xagyl_wheel ${xagylwheel_SRC})
target_link_libraries(indi_xagyl_wheel indidriver)
install(TARGETS indi_xagyl_wheel RUNTIME DESTINATION bin)

# ########## Filter Simulator ##############
SET(filtersimulator_SRC
    filter_simulator.cpp)

add_executable(indi_simulator_wheel ${filtersimulator_SRC})
target_link_libraries(indi_simulator_wheel indidriver)
install(TARGETS indi_simulator_wheel RUNTIME DESTINATION bin)

# ########## Manual Filter ##############
SET(manualfilter_SRC
    manual_filter.cpp)

add_executable(indi_manual_wheel ${manualfilter_SRC})
target_link_libraries(indi_manual_wheel indidriver)
install(TARGETS indi_manual_wheel RUNTIME DESTINATION bin)

# ######### Optec Wheel IFW ############
SET(optecwheel_SRC
    ifwoptec.cpp)

add_executable(indi_optec_wheel ${optecwheel_SRC})
target_link_libraries(indi_optec_wheel indidriver)
install(TARGETS indi_optec_wheel RUNTIME DESTINATION bin)

# ######### Quantum Wheel ############
SET(quantum_wheel_SRC
    quantum_wheel.cpp)

add_executable(indi_quantum_wheel ${quantum_wheel_SRC})
target_link_libraries(indi_quantum_wheel indidriver)
install(TARGETS indi_quantum_wheel RUNTIME DESTINATION bin)

# ######### TruTech Wheel ############
SET(trutechwheel_SRC
    trutech_wheel.cpp)

add_executable(indi_trutech_wheel ${trutechwheel_SRC})
target_link_libraries(indi_trutech_wheel indidriver)
install(TARGETS indi_trutech_wheel RUNTIME DESTINATION bin)

# ######### QHYCFW Wheel 1 ############
SET(qhycfw1_SRC
    qhycfw1.cpp)

add_executable(indi_qhycfw1_wheel ${qhycfw1_SRC})
target_link_libraries(indi_qhycfw1_wheel indidriver)
install(TARGETS indi_qhycfw1_wheel RUNTIME DESTINATION bin)

# ######### QHYCFW Wheel 2 ############
SET(qhycfw2_SRC
    qhycfw2.cpp)

add_executable(indi_qhycfw2_wheel ${qhycfw2_SRC})
target_link_libraries(indi_qhycfw2_wheel indidriver)
install(TARGETS indi_qhycfw2_wheel RUNTIME DESTINATION bin)

# ######### QHYCFW Wheel 3 ############
SET(qhycfw3_SRC
    qhycfw3.cpp)

add_executable(indi_qhycfw3_wheel ${qhycfw3_SRC})
target_link_libraries(indi_qhycfw3_wheel indidriver)
install(TARGETS indi_qhycfw3_wheel RUNTIME DESTINATION bin)

# ############### Pegasus INDIGO Filter Wheel ################
SET(pegasus_indigo_SRC
    pegasus_indigo.cpp)

add_executable(indi_pegasusindigo_wheel ${pegasus_indigo_SRC})

target_link_libraries(indi_pegasusindigo_wheel indidriver)
install(TARGETS indi_pegasusindigo_wheel RUNTIME DESTINATION bin)
