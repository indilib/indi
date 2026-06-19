cmake_minimum_required(VERSION 3.18)
project(indiclient CXX)
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.31)
    cmake_policy(SET CMP0177 NEW)
endif()

# Dependency
find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)
include_directories(${ZLIB_INCLUDE_DIR})

list(APPEND ${PROJECT_NAME}_LIBS
    indicore
    indidevice
    indiabstractclient
    sockets
    ${CMAKE_THREAD_LIBS_INIT}
    ${ZLIB_LIBRARY}
)

# Sources
list(APPEND ${PROJECT_NAME}_SOURCES
    baseclient.cpp
)

# Headers
list(APPEND ${PROJECT_NAME}_HEADERS
    baseclient.h
)

# Private Headers
list(APPEND ${PROJECT_NAME}_PRIVATE_HEADERS
    baseclient_p.h
)

# Build Object Library
add_library(${PROJECT_NAME}_OBJECT OBJECT)
set_property(TARGET ${PROJECT_NAME}_OBJECT PROPERTY POSITION_INDEPENDENT_CODE 1)

target_include_directories(${PROJECT_NAME}_OBJECT
    PUBLIC .
)

target_sources(${PROJECT_NAME}_OBJECT
    PUBLIC
    ${${PROJECT_NAME}_HEADERS}
    PRIVATE
    ${${PROJECT_NAME}_SOURCES}
    ${${PROJECT_NAME}_PRIVATE_HEADERS}
)

target_link_libraries(${PROJECT_NAME}_OBJECT ${${PROJECT_NAME}_LIBS})

install(FILES
    ${${PROJECT_NAME}_HEADERS}
    DESTINATION
    ${INCLUDE_INSTALL_DIR}/libindi
    COMPONENT Devel
)

# Build Static Library
if(INDI_BUILD_STATIC)
    add_library(${PROJECT_NAME}static STATIC)

    target_link_libraries(${PROJECT_NAME}static ${PROJECT_NAME}_OBJECT ${${PROJECT_NAME}_LIBS})
    target_include_directories(${PROJECT_NAME}static
        PUBLIC .
    )

    set_target_properties(${PROJECT_NAME}static PROPERTIES
        VERSION ${CMAKE_INDI_VERSION_STRING}
        SOVERSION ${INDI_SOVERSION}
        OUTPUT_NAME ${PROJECT_NAME} # this same name like shared library - backwards compatibility
    )

    install(TARGETS ${PROJECT_NAME}static
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
endif()

# Build Shared Library
if(INDI_BUILD_SHARED)
    add_library(${PROJECT_NAME} SHARED)

    target_link_libraries(${PROJECT_NAME} ${PROJECT_NAME}_OBJECT ${${PROJECT_NAME}_LIBS})
    target_include_directories(${PROJECT_NAME}
        PUBLIC .
    )

    set_target_properties(${PROJECT_NAME} PROPERTIES
        VERSION ${CMAKE_INDI_VERSION_STRING}
        SOVERSION ${INDI_SOVERSION}
        OUTPUT_NAME ${PROJECT_NAME}
    )

    install(TARGETS ${PROJECT_NAME}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
endif()
