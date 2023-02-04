cmake_minimum_required(VERSION 3.13)
project(sockets CXX)

find_package(Threads REQUIRED)
add_library(${PROJECT_NAME} OBJECT "")

# Headers
list(APPEND ${PROJECT_NAME}_HEADERS
    tcpsocket.h
)

list(APPEND ${PROJECT_NAME}_PRIVATE_HEADERS
    tcpsocket_p.h
)

# Sources
list(APPEND ${PROJECT_NAME}_SOURCES
    tcpsocket.cpp
)

if(UNIX)
    list(APPEND ${PROJECT_NAME}_SOURCES
        tcpsocket_unix.cpp
    )
else()
    list(APPEND ${PROJECT_NAME}_SOURCES
        tcpsocket_win.cpp
    )
    target_link_libraries(${PROJECT_NAME} -lws2_32)
endif()

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

target_link_libraries(${PROJECT_NAME} Threads::Threads)
