cmake_minimum_required(VERSION 2.8)
PROJECT(libapogee C CXX)

set(CMAKE_CXX_STANDARD 11)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake_modules/")
include(GNUInstallDirs)

IF(APPLE)
set(APOGEE_VERSION "3.0.3.2.3.4")
set(CONF_DIR "/usr/local/etc" CACHE STRING "Base configuration directory")
ELSE(APPLE)
set(APOGEE_VERSION "3.0.3234")
set(CONF_DIR "/etc" CACHE STRING "Base configuration directory")
set(UDEVRULES_INSTALL_DIR "/lib/udev/rules.d" CACHE STRING "Base directory for udev rules")
ENDIF()

# IMPORTANT: When update libapogee code to upstream. Do not forget to save apgHelper.cpp as the configuration file
# is hardcoded in the upstream version while it is used from configuration file here.
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )

set(APOGEE_SOVERSION "3")

find_package(USB1 REQUIRED)
find_package(CURL REQUIRED)

set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost REQUIRED COMPONENTS regex)

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/linux)
include_directories( ${Boost_INCLUDE_DIRS})
include_directories( ${CURL_INCLUDE_DIR})

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-error")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error")

# Less error prone with libapogee upstream updates since they do not use CMake
file(GLOB libapogee_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/linux/*.cpp)

# Windows file
list(REMOVE_ITEM libapogee_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/COMHelper.cpp")
# Missing headers
list(REMOVE_ITEM libapogee_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/AspenFx2.cpp")

add_library(apogee SHARED ${libapogee_SRCS})

set_target_properties(apogee PROPERTIES VERSION ${APOGEE_VERSION} SOVERSION ${APOGEE_SOVERSION})

target_link_libraries(apogee ${USB1_LIBRARIES} ${CURL_LIBRARY} ${Boost_LIBRARIES})

install(TARGETS apogee LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

file(GLOB libapogee_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
install( FILES ${libapogee_HEADERS} DESTINATION include/libapogee COMPONENT Devel)

install(
   CODE "
   file(GLOB APOGEE_CONF ${CMAKE_CURRENT_SOURCE_DIR}/conf/*) \n
   file(INSTALL DESTINATION ${CONF_DIR}/Apogee/camera TYPE FILE FILES \${APOGEE_CONF})"
 )

IF (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
install(FILES 99-apogee.rules DESTINATION ${UDEVRULES_INSTALL_DIR})
ENDIF()

