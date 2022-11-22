cmake_minimum_required(VERSION 3.16)
project(indicore C CXX)

find_package(Nova)

add_library(${PROJECT_NAME} OBJECT "")

# Headers
list(APPEND ${PROJECT_NAME}_HEADERS
    indiapi.h
    indidevapi.h
    lilxml.h
    base64.h
    indicom.h
    libastro.h
    sharedblob.h
)

list(APPEND ${PROJECT_NAME}_PRIVATE_HEADERS
    base64_luts.h
    indililxml.h
    indiuserio.h
    locale_compat.h
    sharedblob_parse.h
    userio.h
)

# Sources
list(APPEND ${PROJECT_NAME}_SOURCES
    base64.c
    userio.c
    indicom.c
    lilxml.cpp
    libastro.cpp
    sharedblob.c
    sharedblob_parse.cpp
    indiuserio.c
)

if(INDI_SHARED_MEMORY)
    list(APPEND ${PROJECT_NAME}_PRIVATE_HEADERS
        shm_open_anon.h)
    list(APPEND ${PROJECT_NAME}_SOURCES
        shm_open_anon.c)
endif()

if(NOVA_FOUND)
    target_compile_definitions(${PROJECT_NAME} PUBLIC "-DHAVE_LIBNOVA")
    target_link_libraries(${PROJECT_NAME} ${NOVA_LIBRARIES})
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
    PUBLIC . ${CMAKE_CURRENT_BINARY_DIR}/../..
)

install(FILES
    ${${PROJECT_NAME}_HEADERS}
    DESTINATION
    ${INCLUDE_INSTALL_DIR}/libindi
    COMPONENT Devel
)
