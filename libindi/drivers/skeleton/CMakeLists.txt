cmake_minimum_required(VERSION 3.0)
PROJECT(libindi-skeletons C CXX)

include_directories( ../../)
include_directories( ../../libs)
include_directories( ../../libs/indibase)

################ Sample Focuser Driver ################

SET(focuserdriver_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/focuser_driver.cpp)

add_executable(indi_focuser_driver_focus ${focuserdriver_SRC})
target_link_libraries(indi_focuser_driver_focus indidriver)
install(TARGETS indi_focuser_driver_focus RUNTIME DESTINATION bin)
