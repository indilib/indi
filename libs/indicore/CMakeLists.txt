cmake_minimum_required(VERSION 3.16)
project(indicore C CXX)

add_library(${PROJECT_NAME} OBJECT "")

# Headers
list(APPEND ${PROJECT_NAME}_HEADERS
    indiutility.h
    indimacros.h
    indibasetypes.h
    indiapi.h
    base64.h
    lilxml.h
    indililxml.h
)

# Private Headers
list(APPEND ${PROJECT_NAME}_PRIVATE_HEADERS
    locale_compat.h
    userio.h
    indiuserio.h
    base64_luts.h
)

# Sources
list(APPEND ${PROJECT_NAME}_SOURCES
    indiutility.cpp
    indiapi.c
    userio.c
    indiuserio.c
    base64.c
    lilxml.cpp
)

# Setup Target
target_sources(${PROJECT_NAME}
    PUBLIC
    ${${PROJECT_NAME}_HEADERS}
    PRIVATE
    ${${PROJECT_NAME}_SOURCES}
)

target_include_directories(${PROJECT_NAME}
    PUBLIC ./
)

install(FILES
    ${${PROJECT_NAME}_HEADERS}
    DESTINATION
    ${INCLUDE_INSTALL_DIR}/libindi
    COMPONENT Devel
)
