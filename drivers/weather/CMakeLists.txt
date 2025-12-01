# ########## Weather Simulator ###############

SET(weathersimulator_SRC
    weathersimulator.cpp)

add_executable(indi_simulator_weather ${weathersimulator_SRC})
target_link_libraries(indi_simulator_weather indidriver)
install(TARGETS indi_simulator_weather RUNTIME DESTINATION bin)

# ########## Weather Meta Driver ###############
SET(weathermeta_SRC
    weathermeta.cpp)

add_executable(indi_meta_weather ${weathermeta_SRC})
target_link_libraries(indi_meta_weather indidriver)
install(TARGETS indi_meta_weather RUNTIME DESTINATION bin)

# ########## Weather File Watcher ###############
SET(weatherwatcher_SRC
    weatherwatcher.cpp)

add_executable(indi_watcher_weather ${weatherwatcher_SRC})
target_link_libraries(indi_watcher_weather indidriver ${CURL})
install(TARGETS indi_watcher_weather RUNTIME DESTINATION bin)

# ########## AAGSolo Weather Station ###############
SET(aagsolo_SRC
    aagsolo.cpp)

add_executable(indi_aagsolo_weather ${aagsolo_SRC})
target_link_libraries(indi_aagsolo_weather indidriver ${CURL})
install(TARGETS indi_aagsolo_weather RUNTIME DESTINATION bin)

# ########## Weather Safety Proxy ###############
SET(weathersafetyproxy_SRC
    weather_safety_proxy.cpp)
add_executable(indi_weather_safety_proxy ${weathersafetyproxy_SRC})
target_link_libraries(indi_weather_safety_proxy indidriver ${CURL} ${JSONLIB})
install(TARGETS indi_weather_safety_proxy RUNTIME DESTINATION bin)

# ########## MBox Driver ###############
SET(mbox_SRC
    mbox.cpp)

add_executable(indi_mbox_weather ${mbox_SRC})
target_link_libraries(indi_mbox_weather indidriver)
install(TARGETS indi_mbox_weather RUNTIME DESTINATION bin)

# ########## Pegasus Uranus Meteo Driver ###############
SET(uranusmeteo_SRC
    uranusmeteo.cpp)

add_executable(indi_uranus_weather ${uranusmeteo_SRC})
target_link_libraries(indi_uranus_weather indidriver)
install(TARGETS indi_uranus_weather RUNTIME DESTINATION bin)

# ########## Vantage Driver ###############
SET(vantage_SRC
    vantage.cpp)

add_executable(indi_vantage_weather ${vantage_SRC})
target_link_libraries(indi_vantage_weather indidriver)
install(TARGETS indi_vantage_weather RUNTIME DESTINATION bin)

# ########## OpenWeatherMap Driver ###############
SET(OpenWeatherMap_SRC
    openweathermap.cpp)

IF(UNITY_BUILD)
    ENABLE_UNITY_BUILD(OpenWeatherMap OpenWeatherMap_SRC 10 cpp)
ENDIF()

add_executable(indi_openweathermap_weather ${OpenWeatherMap_SRC})
target_link_libraries(indi_openweathermap_weather indidriver ${CURL} ${JSONLIB})
install(TARGETS indi_openweathermap_weather RUNTIME DESTINATION bin)

# ########## Weather safety alpaca ###############
SET(WeatherSafetyAlpaca_SRC
    weather_safety_alpaca.cpp)

add_executable(indi_weather_safety_alpaca ${WeatherSafetyAlpaca_SRC})
target_link_libraries(indi_weather_safety_alpaca indidriver indiclient ${HTTPLIB_LIBRARY})
install(TARGETS indi_weather_safety_alpaca RUNTIME DESTINATION bin)

# ########## WeatherFlow Tempest ###############
SET(WeatherFlow_SRC
    weatherflow.cpp)

add_executable(indi_weatherflow_weather ${WeatherFlow_SRC})
target_link_libraries(indi_weatherflow_weather indidriver ${HTTPLIB_LIBRARY} ${JSONLIB})
install(TARGETS indi_weatherflow_weather RUNTIME DESTINATION bin)

# ########## HiTech Weather Driver ###############
SET(hitechweather_SRC
    hitechweather.cpp)

add_executable(indi_hitech_weather ${hitechweather_SRC})
target_link_libraries(indi_hitech_weather indidriver ${HIDAPILIB})
install(TARGETS indi_hitech_weather RUNTIME DESTINATION bin)
