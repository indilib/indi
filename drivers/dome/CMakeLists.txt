# ############### Dome Simulator ################
SET(domesimulator_SRC
    dome_simulator.cpp)

add_executable(indi_simulator_dome ${domesimulator_SRC})
target_link_libraries(indi_simulator_dome indidriver)
install(TARGETS indi_simulator_dome RUNTIME DESTINATION bin)

# ############### Roll Off ################
SET(rolloff_SRC
    roll_off.cpp)

add_executable(indi_rolloff_dome ${rolloff_SRC})
target_link_libraries(indi_rolloff_dome indidriver)
install(TARGETS indi_rolloff_dome RUNTIME DESTINATION bin)

# ############### Baader Dome ################
SET(baaderdome_SRC
    baader_dome.cpp)

add_executable(indi_baader_dome ${baaderdome_SRC})
target_link_libraries(indi_baader_dome indidriver)
install(TARGETS indi_baader_dome RUNTIME DESTINATION bin)

# ############### Dome Pro 2 ##$##############
SET(domepro2_SRC
    domepro2.cpp)

add_executable(indi_domepro2_dome ${domepro2_SRC})
target_link_libraries(indi_domepro2_dome indidriver)
install(TARGETS indi_domepro2_dome RUNTIME DESTINATION bin)

# ############### Rigel Dome ################
SET(rigeldome_SRC
    rigel_dome.cpp)

add_executable(indi_rigel_dome ${rigeldome_SRC})
target_link_libraries(indi_rigel_dome indidriver)
install(TARGETS indi_rigel_dome RUNTIME DESTINATION bin)

# ############### ScopeDome Dome ################
set(scopedome_SRCS
    scopedome_dome.cpp
    scopedome_usb21.cpp
    scopedome_arduino.cpp
    scopedome_sim.cpp
)

add_executable(indi_scopedome_dome ${scopedome_SRCS})
target_link_libraries(indi_scopedome_dome indidriver ${CURL})
install(TARGETS indi_scopedome_dome RUNTIME DESTINATION bin)

# ############### Technical Innovations DDW Dome ################
set(ddw_dome_SRCS
    ddw_dome.cpp
)

add_executable(indi_ddw_dome ${ddw_dome_SRCS})
target_link_libraries(indi_ddw_dome indidriver)
install(TARGETS indi_ddw_dome RUNTIME DESTINATION bin)

# ########## Dome Scripting Gateway ##############
SET(domescript_SRC
    dome_script.cpp)

add_executable(indi_script_dome ${domescript_SRC})
target_link_libraries(indi_script_dome indidriver)
install(TARGETS indi_script_dome RUNTIME DESTINATION bin)

# ########## NexDome Beaver Controller ##############
SET(nexdome_beaver_SRC
    nexdome_beaver.cpp)

add_executable(indi_nexdome_beaver ${nexdome_beaver_SRC})
target_link_libraries(indi_nexdome_beaver indidriver)
install(TARGETS indi_nexdome_beaver RUNTIME DESTINATION bin)

# ############### DragonLAIR ################
SET(dragonlair_SRC
    dragonlair.cpp)

add_executable(indi_dragonlair_dome ${dragonlair_SRC})
target_link_libraries(indi_dragonlair_dome indidriver ${HTTPLIB_LIBRARY})
install(TARGETS indi_dragonlair_dome RUNTIME DESTINATION bin)

# ############### Universal ROR ################
SET(universalror_SRC
    universal_ror.cpp
    universal_ror_client.cpp
    )

add_executable(indi_universalror_dome ${universalror_SRC})
target_link_libraries(indi_universalror_dome indidriver indiclient)
install(TARGETS indi_universalror_dome RUNTIME DESTINATION bin)

# ############### Alpaca Dome ################
SET(alpaca_dome_SRC
    alpaca_dome.cpp
    )

add_executable(indi_alpaca_dome ${alpaca_dome_SRC})
target_link_libraries(indi_alpaca_dome indidriver indiclient ${HTTPLIB_LIBRARY})
install(TARGETS indi_alpaca_dome RUNTIME DESTINATION bin)
