# ########## INDI::CCD V4L Driver ###############

IF(${CMAKE_SYSTEM_NAME} MATCHES "Linux|FreeBSD|OpenBSD")
    SET(v4l2driverccd_SRC
        lx/Lx.cpp
        v4l2driver.cpp
        indi_v4l2driver.cpp)

    IF(UNITY_BUILD)
        ENABLE_UNITY_BUILD(v4l2driverccd v4l2driverccd_SRC 10 cpp)
    ENDIF()

    add_executable(indi_v4l2_ccd ${v4l2driverccd_SRC} ${libwebcam_C_SRC} ${libwebcam_CXX_SRC})
    target_link_libraries(indi_v4l2_ccd ${JPEG_LIBRARY} indidriver)
    install(TARGETS indi_v4l2_ccd RUNTIME DESTINATION bin)

    # For DBK21 camera
    IF(${CMAKE_SYSTEM_NAME} MATCHES "Linux" AND INDI_INSTALL_UDEV_RULES)
        install(FILES 80-dbk21-camera.rules DESTINATION ${UDEVRULES_INSTALL_DIR})
    ENDIF()
ENDIF()
