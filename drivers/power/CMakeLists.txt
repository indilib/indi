# CMakeLists.txt for INDI Power Drivers

# ########## Terrans PowerBoxGo V2 ###############
SET(terranspowerboxgov2_SRC
    Terrans_PowerBoxGo_V2.cpp)

add_executable(indi_terrans_powerboxgo_v2 ${terranspowerboxgov2_SRC})
target_link_libraries(indi_terrans_powerboxgo_v2 indidriver)
install(TARGETS indi_terrans_powerboxgo_v2 RUNTIME DESTINATION bin)

# ########## Terrans PowerBoxPro V2 ###############
SET(terranspowerboxprov2_SRC
    Terrans_PowerBoxPro_V2.cpp)

add_executable(indi_terrans_powerboxpro_v2 ${terranspowerboxprov2_SRC})
target_link_libraries(indi_terrans_powerboxpro_v2 indidriver)
install(TARGETS indi_terrans_powerboxpro_v2 RUNTIME DESTINATION bin)

# ########## Pegasus Ultimate Power Box Driver ###############
SET(pegasus_upb_SRC
    pegasus_upb.cpp)

add_executable(indi_pegasus_upb ${pegasus_upb_SRC})
target_link_libraries(indi_pegasus_upb indidriver)
install(TARGETS indi_pegasus_upb RUNTIME DESTINATION bin)

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

# ########## Pegasus Saddle Power Box  Driver ###############
SET(pegasus_spb_SRC
    pegasus_spb.cpp)

add_executable(indi_pegasus_spb ${pegasus_spb_SRC})
target_link_libraries(indi_pegasus_spb indidriver)
install(TARGETS indi_pegasus_spb RUNTIME DESTINATION bin)

# ########## UPS ###############
SET(ups_SRC
    ups.cpp)

add_executable(indi_ups_safety ${ups_SRC})
target_link_libraries(indi_ups_safety indidriver)
install(TARGETS indi_ups_safety RUNTIME DESTINATION bin)

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

# ########## CheapoDC Driver ###############
SET(cheapodc_SRC
    cheapodc.cpp)

add_executable(indi_cheapodc ${cheapodc_SRC})
target_link_libraries(indi_cheapodc indidriver)
install(TARGETS indi_cheapodc RUNTIME DESTINATION bin)

# ########## MYDCP4ESP32 Driver ###############
SET(mydcp4esp32_SRC
    mydcp4esp32.cpp)

add_executable(indi_mydcp4esp32 ${mydcp4esp32_SRC})
target_link_libraries(indi_mydcp4esp32 indidriver)
install(TARGETS indi_mydcp4esp32 RUNTIME DESTINATION bin)

# ########## Celestron Dew/Power Controller Driver ###############
SET(celestron_dewpower_SRC
    indi-celestron-dewpower/celestron_dewpower.cpp
    indi-celestron-dewpower/celestron_dewpower_auxproto.cpp)

add_executable(indi_celestron_dewpower ${celestron_dewpower_SRC})
target_link_libraries(indi_celestron_dewpower indidriver)
install(TARGETS indi_celestron_dewpower RUNTIME DESTINATION bin)
