cmake_minimum_required(VERSION 3.13)
project(indixisf C CXX)

include_directories(../indibase)
include_directories(../indicore)

find_package(LibXISF)
find_package(Qt5Core)

if(LibXISF_FOUND AND Qt5Core_FOUND)

    add_library(${PROJECT_NAME} SHARED
        indixisf.cpp
        indixisf.h
        )

    target_link_libraries(${PROJECT_NAME} indicore Qt5::Core XISF)

    set_target_properties(${PROJECT_NAME} PROPERTIES
        VERSION ${CMAKE_INDI_VERSION_STRING}
        SOVERSION ${INDI_SOVERSION}
        OUTPUT_NAME ${PROJECT_NAME}
    )

    install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

else(LibXISF_FOUND AND Qt5Core_FOUND)
    message(WARNING "Could not find LibXISF or Qt5Core, skipping building INDI XISF wrapper")
endif(LibXISF_FOUND AND Qt5Core_FOUND)
