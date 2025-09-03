if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    # ############### BNO055 Driver ################
    SET(bno055_SRC
        bno055.cpp
    )

    add_executable(indi_bno055_imu ${bno055_SRC})
    target_link_libraries(indi_bno055_imu indidriver)
    install(TARGETS indi_bno055_imu RUNTIME DESTINATION bin)

    # ############### BNO085 Driver ################
    SET(bno085_SRC
        bno085.cpp
    )

    add_executable(indi_bno085_imu ${bno085_SRC})
    target_link_libraries(indi_bno085_imu indidriver)
    install(TARGETS indi_bno085_imu RUNTIME DESTINATION bin)
endif()
