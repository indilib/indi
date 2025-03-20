# ########## Waveshare Relay ###############
SET(wavesharerelay_SRC
    waveshare_modbus_relay.cpp
    ../../libs/modbus/nanomodbus.c
    )

add_executable(indi_wavesharemodbus_relay ${wavesharerelay_SRC})
target_link_libraries(indi_wavesharemodbus_relay indidriver)
install(TARGETS indi_wavesharemodbus_relay RUNTIME DESTINATION bin)

# ########## IPX800 Controller ###############
SET(ipx800v4_SRC
    ipx800v4.cpp)

add_executable(indi_ipx800v4 ${ipx800v4_SRC})
target_link_libraries(indi_ipx800v4 indidriver ${HTTPLIB_LIBRARY})
install(TARGETS indi_ipx800v4 RUNTIME DESTINATION bin)

# ########## Simulator IO ###############
SET(simulator_io_SRC
    simulator_io.cpp)

add_executable(indi_simulator_io ${simulator_io_SRC})
target_link_libraries(indi_simulator_io indidriver)
install(TARGETS indi_simulator_io RUNTIME DESTINATION bin)
