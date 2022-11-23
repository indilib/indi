cmake_minimum_required(VERSION 3.16)
project(indiclientqt CXX)

set(QT_ANDROID "" CACHE path "Qt Android path")

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
else()
    find_package(Qt5Core REQUIRED)
    find_package(Qt5Network REQUIRED)
    find_package(ZLIB REQUIRED)
    find_package(CFITSIO REQUIRED)
endif()

if(INDI_BUILD_STATIC)
    add_library(${PROJECT_NAME} STATIC)
else()
    add_library(${PROJECT_NAME} SHARED)
endif()

# Headers
list(APPEND ${PROJECT_NAME}_HEADERS
    baseclientqt.h
)

list(APPEND ${PROJECT_NAME}_PRIVATE_HEADERS
    baseclientqt_p.h
)

# Sources
list(APPEND ${PROJECT_NAME}_SOURCES
    baseclientqt.cpp
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
    Qt5::Network
    ${ZLIB_LIBRARY})

if(WIN32 OR ANDROID)
    install(TARGETS ${PROJECT_NAME}
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
    )
else()
    install(TARGETS ${PROJECT_NAME}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
endif()

install(FILES
    ${${PROJECT_NAME}_HEADERS}
    DESTINATION
    ${INCLUDE_INSTALL_DIR}/libindi
    COMPONENT Devel
)
