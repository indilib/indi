cmake_minimum_required(VERSION 3.13)
project(indiabstractclient CXX)
cmake_policy(SET CMP0177 NEW)

add_library(${PROJECT_NAME} OBJECT)

# Headers
list(APPEND ${PROJECT_NAME}_HEADERS
    abstractbaseclient.h
)

list(APPEND ${PROJECT_NAME}_PRIVATE_HEADERS
    abstractbaseclient_p.h
)

# Sources
list(APPEND ${PROJECT_NAME}_SOURCES
    abstractbaseclient.cpp
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
    PUBLIC .
)

target_link_libraries(${PROJECT_NAME} indicore indidevice)

install(FILES
    ${${PROJECT_NAME}_HEADERS}
    DESTINATION
    ${INCLUDE_INSTALL_DIR}/libindi
    COMPONENT Devel
)
