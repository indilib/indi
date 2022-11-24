cmake_minimum_required(VERSION 3.16)
project(indicore C CXX)

if(NOT WIN32)
    find_package(Nova)
endif()

add_library(${PROJECT_NAME} OBJECT "")

CHECK_FUNCTION_EXISTS(mremap HAVE_MREMAP)

# Headers
list(APPEND ${PROJECT_NAME}_HEADERS
    indiapi.h
    indidevapi.h
    lilxml.h
    base64.h
    indicom.h
    sharedblob.h
)

list(APPEND ${PROJECT_NAME}_PRIVATE_HEADERS
    base64_luts.h
    indililxml.h
    indiuserio.h
    userio.h
)

# Sources
list(APPEND ${PROJECT_NAME}_SOURCES
    base64.c
    userio.c
    indicom.c
    indidevapi.c
    lilxml.cpp
    indiuserio.c
)

if(NOT WIN32)
    list(APPEND ${PROJECT_NAME}_PRIVATE_HEADERS
        sharedblob_parse.h
        shm_open_anon.h)
    list(APPEND ${PROJECT_NAME}_SOURCES
        sharedblob.c
        sharedblob_parse.cpp
        shm_open_anon.c)
endif()

if(NOVA_FOUND)
    target_compile_definitions(${PROJECT_NAME} PUBLIC "-DHAVE_LIBNOVA")
    target_link_libraries(${PROJECT_NAME} ${NOVA_LIBRARIES})
    target_include_directories(${PROJECT_NAME} ${NOVA_INCLUDE_DIR})

    list(APPEND ${PROJECT_NAME}_HEADERS
        libastro.h
    )

    list(APPEND ${PROJECT_NAME}_SOURCES
        libastro.cpp
    )
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
