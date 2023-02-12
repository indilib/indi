cmake_minimum_required(VERSION 3.13)
project(indixisf C CXX)

include_directories(../indibase)
include_directories(../indicore)

find_package(LibXISF REQUIRED)
find_package(Qt5Core REQUIRED)

add_library(${PROJECT_NAME} SHARED
    indixisf.cpp
    indixisf.h
    )

target_link_libraries(${PROJECT_NAME} indicore Qt5::Core XISF)

set_target_properties(${PROJECT_NAME} PROPERTIES
    VERSION ${CMAKE_INDI_VERSION_STRING}
    SOVERSION ${INDI_SOVERSION}
    OUTPUT_NAME ${PROJECT_NAME}
)

install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
