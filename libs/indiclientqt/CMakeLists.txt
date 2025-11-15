cmake_minimum_required(VERSION 3.13)
project(indiclientqt CXX)

if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.31)
    cmake_policy(SET CMP0177 NEW)
endif()

set(QT_ANDROID "" CACHE STRING "Qt Android path")

if(ANDROID)
    set(Qt5Core_DIR "${QT_ANDROID}/lib/cmake/Qt5Core")
    include(${QT_ANDROID}/lib/cmake/Qt5/Qt5Config.cmake)
    include(${QT_ANDROID}/lib/cmake/Qt5Core/Qt5CoreConfig.cmake)
    include(${QT_ANDROID}/lib/cmake/Qt5Network/Qt5NetworkConfig.cmake)

    if(NOT CFITSIO_DIR)
        message(FATAL_ERROR CFITSIO_DIR must be set)
    else()
        set(CFITSIO_INCLUDE_DIR ${CFITSIO_DIR})
        set(CFITSIO_LIBRARIES ${CFITSIO_DIR}/libcfitsio.a)
    endif()
    
    set(QT_VERSION_MAJOR 5)
else()
    # Try Qt6 first, fall back to Qt5 if not found
    find_package(QT NAMES Qt6 Qt5 COMPONENTS Core Network)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Network)
    find_package(ZLIB REQUIRED)
    find_package(CFITSIO REQUIRED)
    
    message(STATUS "Using Qt${QT_VERSION_MAJOR} for indiclientqt")
endif()

list(APPEND ${PROJECT_NAME}_LIBS
    indicore
    indidevice
    indiabstractclient
    Qt${QT_VERSION_MAJOR}::Network
    ${ZLIB_LIBRARY}
)

# Sources
list(APPEND ${PROJECT_NAME}_SOURCES
    baseclientqt.cpp
)

# Headers
list(APPEND ${PROJECT_NAME}_HEADERS
    baseclientqt.h
)

# Private Headers
list(APPEND ${PROJECT_NAME}_PRIVATE_HEADERS
    baseclientqt_p.h
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
