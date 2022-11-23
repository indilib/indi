cmake_minimum_required(VERSION 3.16)
project(indiclient CXX)

find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)

if(INDI_BUILD_STATIC)
    add_library(${PROJECT_NAME} STATIC)
else()
    add_library(${PROJECT_NAME} SHARED)
endif()

# Headers
list(APPEND ${PROJECT_NAME}_HEADERS
    baseclient.h
)

list(APPEND ${PROJECT_NAME}_PRIVATE_HEADERS
    baseclient_p.h
)

# Sources
list(APPEND ${PROJECT_NAME}_SOURCES
    baseclient.cpp
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

target_link_libraries(${PROJECT_NAME}
    indicore
    indidevice
    indiabstractclient
    sockets
    ${CMAKE_THREAD_LIBS_INIT}
    ${ZLIB_LIBRARY}
)

install(TARGETS ${PROJECT_NAME}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(FILES
    ${${PROJECT_NAME}_HEADERS}
    DESTINATION
    ${INCLUDE_INSTALL_DIR}/libindi
    COMPONENT Devel
)
