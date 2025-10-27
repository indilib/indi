cmake_minimum_required(VERSION 3.18)
project(indidevice C CXX)
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.31)
    cmake_policy(SET CMP0177 NEW)
endif()

# Dependency
find_package(ZLIB REQUIRED)
include_directories(${ZLIB_INCLUDE_DIR})

add_library(${PROJECT_NAME} OBJECT "")

# Headers
list(APPEND ${PROJECT_NAME}_HEADERS
    indibase.h
    indibasetypes.h

    basedevice.h
    parentdevice.h

    indistandardproperty.h

    property/indiproperties.h
    property/indiproperty.h
    property/indipropertybasic.h
    property/indipropertyview.h
    property/indipropertytext.h
    property/indipropertynumber.h
    property/indipropertyswitch.h
    property/indipropertylight.h
    property/indipropertyblob.h

    property/indiwidgetview.h
    property/indiwidgettraits.h
)

list(APPEND ${PROJECT_NAME}_PRIVATE_HEADERS
    parentdevice_p.h
    basedevice_p.h

    watchdeviceproperty.h

    property/indiproperty_p.h
    property/indiproperties_p.h
    property/indipropertyblob_p.h
    property/indipropertyswitch_p.h
    property/indipropertylight_p.h
    property/indipropertytext_p.h
    property/indipropertynumber_p.h
    property/indipropertybasic_p.h
)

# Sources
list(APPEND ${PROJECT_NAME}_SOURCES
    indibase.cpp

    parentdevice.cpp
    basedevice.cpp
    watchdeviceproperty.cpp

    indistandardproperty.cpp

    property/indiproperties.cpp
    property/indiproperty.cpp
    property/indipropertybasic.cpp
    property/indipropertyview.cpp
    property/indipropertytext.cpp
    property/indipropertynumber.cpp
    property/indipropertyswitch.cpp
    property/indipropertylight.cpp
    property/indipropertyblob.cpp
)

# Setup Target
target_sources(${PROJECT_NAME}
    PUBLIC
    ${${PROJECT_NAME}_HEADERS}
    PRIVATE
    ${${PROJECT_NAME}_SOURCES}
    ${${PROJECT_NAME}_PRIVATE_HEADERS}
)

target_include_directories(${PROJECT_NAME}
    PUBLIC . property ${CMAKE_CURRENT_BINARY_DIR}/../..
)

target_link_libraries(${PROJECT_NAME} indicore)

install(FILES
    ${${PROJECT_NAME}_HEADERS}
    DESTINATION
    ${INCLUDE_INSTALL_DIR}/libindi
    COMPONENT Devel
)
