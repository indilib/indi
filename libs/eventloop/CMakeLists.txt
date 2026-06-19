cmake_minimum_required(VERSION 3.13)
project(eventloop C)
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.31)
    cmake_policy(SET CMP0177 NEW)
endif()

add_library(${PROJECT_NAME} OBJECT "")

# Headers
list(APPEND ${PROJECT_NAME}_HEADERS
    eventloop.h
)

list(APPEND ${PROJECT_NAME}_PRIVATE_HEADERS
)

# Sources
list(APPEND ${PROJECT_NAME}_SOURCES
    eventloop.c
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

install(FILES
    ${${PROJECT_NAME}_HEADERS}
    DESTINATION
    ${INCLUDE_INSTALL_DIR}/libindi
    COMPONENT Devel
)
