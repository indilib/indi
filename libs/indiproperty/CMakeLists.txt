cmake_minimum_required(VERSION 3.18)
project(indiproperty CXX)

add_library(${PROJECT_NAME} OBJECT "")

# Headers
list(APPEND ${PROJECT_NAME}_HEADERS
    indiproperty.h
    indiproperties.h
    indipropertybasic.h
    indipropertytext.h
    indipropertynumber.h
    indipropertyswitch.h
    indipropertylight.h
    indipropertyblob.h
    indipropertyview.h
    indiwidgetview.h
    indiwidgettraits.h
)

# Private Headers
list(APPEND ${PROJECT_NAME}_PRIVATE_HEADERS
    indiproperty_p.h
    indiproperties_p.h
    indipropertybasic_p.h
    indipropertytext_p.h
    indipropertynumber_p.h
    indipropertyswitch_p.h
    indipropertylight_p.h
    indipropertyblob_p.h
)

# Sources
list(APPEND ${PROJECT_NAME}_SOURCES
    indiproperty.cpp
    indiproperties.cpp
    indipropertybasic.cpp
    indipropertytext.cpp
    indipropertynumber.cpp
    indipropertyswitch.cpp
    indipropertylight.cpp
    indipropertyblob.cpp
    indipropertyview.cpp
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
    PUBLIC ./
)

target_link_libraries(${PROJECT_NAME} indicore)

install(FILES
    ${${PROJECT_NAME}_HEADERS}
    DESTINATION
    ${INCLUDE_INSTALL_DIR}/libindi
    COMPONENT Devel
)
