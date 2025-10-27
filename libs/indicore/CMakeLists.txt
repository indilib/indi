cmake_minimum_required(VERSION 3.18)
project(indicore C CXX)
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.31)
    cmake_policy(SET CMP0177 NEW)
endif()

if(NOT WIN32)
    find_package(Nova)
endif()

add_library(${PROJECT_NAME} OBJECT "")

set(CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)
check_symbol_exists(mremap sys/mman.h HAVE_MREMAP)
check_symbol_exists(timespec_get time.h HAVE_TIMESPEC_GET)
check_symbol_exists(clock_gettime time.h HAVE_CLOCK_GETTIME)

# Headers
list(APPEND ${PROJECT_NAME}_HEADERS
    indiapi.h
    indidevapi.h
    indiutility.h
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
    indiutility.cpp
    base64.c
    userio.c
    indicom.c
    indidevapi.c
    lilxml.cpp
    indiuserio.c
    sharedblob.c
)

if(UNIX)
    list(APPEND ${PROJECT_NAME}_PRIVATE_HEADERS
        sharedblob_parse.h
        shm_open_anon.h)
    list(APPEND ${PROJECT_NAME}_SOURCES        
        sharedblob_parse.cpp
        shm_open_anon.c)
    if(UNIX AND NOT APPLE AND NOT CMAKE_SYSTEM_NAME MATCHES "OpenBSD")
        target_link_libraries(${PROJECT_NAME} rt)
    endif()
endif()

target_compile_definitions(${PROJECT_NAME}
  PUBLIC
    $<$<BOOL:${NOVA_FOUND}>:HAVE_LIBNOVA>
    $<$<BOOL:${HAVE_TIMESPEC_GET}>:HAVE_TIMESPEC_GET>
    $<$<BOOL:${HAVE_CLOCK_GETTIME}>:HAVE_CLOCK_GETTIME>
)
if(NOVA_FOUND)
    target_link_libraries(${PROJECT_NAME} ${NOVA_LIBRARIES})
    target_include_directories(${PROJECT_NAME} PRIVATE ${NOVA_INCLUDE_DIR})

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
    PUBLIC
    .
    ${CMAKE_CURRENT_BINARY_DIR}/../.. # config.h
)

install(FILES
    ${${PROJECT_NAME}_HEADERS}
    DESTINATION
    ${INCLUDE_INSTALL_DIR}/libindi
    COMPONENT Devel
)
