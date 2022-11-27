# ############### Receiver Simulator ################
SET(receiversimulator_SRC
    receiver_simulator.cpp)

add_executable(indi_simulator_receiver ${receiversimulator_SRC})
target_link_libraries(indi_simulator_receiver indidriver)
install(TARGETS indi_simulator_receiver RUNTIME DESTINATION bin)

# ################ RTL-SDR Receiver #################
find_package(RTLSDR)

if(RTLSDR_FOUND)
    include_directories(${RTLSDR_INCLUDE_DIR})
    SET(rtlsdr_SRC
        indi_rtlsdr.cpp)

    add_executable(indi_rtlsdr ${rtlsdr_SRC})
    target_link_libraries(indi_rtlsdr indidriver ${RTLSDR_LIBRARIES})
    install(TARGETS indi_rtlsdr RUNTIME DESTINATION bin)
endif()
