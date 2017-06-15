PROJECT(indi-eqmod C CXX)
cmake_minimum_required(VERSION 2.4.7)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake_modules/")
include(GNUInstallDirs)

set(BIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/bin")

find_package(INDI REQUIRED)
find_package(Nova REQUIRED)
find_package(ZLIB REQUIRED)
find_package(GSL REQUIRED)

set(EQMOD_VERSION_MAJOR 0)
set(EQMOD_VERSION_MINOR 3)

if (CYGWIN)
add_definitions(-U__STRICT_ANSI__)
endif(CYGWIN)

option(WITH_ALIGN "Enable Alignment Subsystem" ON)
option(WITH_ALIGN_GEEHALEL "Enable EQMod Alignment" ON)
option(WITH_SCOPE_LIMITS "Enable Scope limits" ON)

set(INDI_DATA_DIR "${CMAKE_INSTALL_PREFIX}/share/indi")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_eqmod.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_eqmod.xml )

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${NOVA_INCLUDE_DIR})
include_directories( ${NOVA_INCLUDE_DIR}/..)
include_directories( ${GSL_INCLUDE_DIR} )

IF ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++11")
ELSE ()
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
ENDIF ()

########### EQMod ###############
set(eqmod_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/eqmod.cpp ${CMAKE_CURRENT_SOURCE_DIR}/eqmoderror.cpp ${CMAKE_CURRENT_SOURCE_DIR}/skywatcher.cpp)

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  set(eqmod_SRCS ${eqmod_SRCS}
    ${CMAKE_CURRENT_SOURCE_DIR}/mach_gettime.cpp)
endif()

set(eqmod_SRCS ${eqmod_SRCS}
   ${CMAKE_CURRENT_SOURCE_DIR}/simulator/simulator.cpp    ${CMAKE_CURRENT_SOURCE_DIR}/simulator/skywatcher-simulator.cpp)
if(WITH_ALIGN_GEEHALEL)
  set(eqmod_SRCS ${eqmod_SRCS}
   ${CMAKE_CURRENT_SOURCE_DIR}/align/align.cpp ${CMAKE_CURRENT_SOURCE_DIR}/align/pointset.cpp ${CMAKE_CURRENT_SOURCE_DIR}/align/htm.c
   ${CMAKE_CURRENT_SOURCE_DIR}/align/triangulate.cpp ${CMAKE_CURRENT_SOURCE_DIR}/align/triangulate_chull.cpp ${CMAKE_CURRENT_SOURCE_DIR}/align/chull/chull.c)
endif(WITH_ALIGN_GEEHALEL)
if(WITH_SCOPE_LIMITS)
  set(eqmod_SRCS ${eqmod_SRCS}
    ${CMAKE_CURRENT_SOURCE_DIR}/scope-limits/scope-limits.cpp)
endif(WITH_SCOPE_LIMITS)

add_executable(indi_eqmod_telescope ${eqmod_SRCS})

if(WITH_ALIGN)
if (CYGWIN)
target_link_libraries(indi_eqmod_telescope ${INDI_LIBRARIES} ${NOVA_LIBRARIES} ${INDI_ALIGN_LIBRARIES} ${GSL_LIBRARIES} ${ZLIB_LIBRARY})
else()
target_link_libraries(indi_eqmod_telescope ${INDI_LIBRARIES} ${NOVA_LIBRARIES} ${INDI_ALIGN_LIBRARIES})
endif(CYGWIN)
else(WITH_ALIGN)
  target_link_libraries(indi_eqmod_telescope ${INDI_LIBRARIES} ${NOVA_LIBRARIES})
endif(WITH_ALIGN)

if (CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "arm*")
target_link_libraries(indi_eqmod_telescope rt)
endif (CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "arm*")

install(TARGETS indi_eqmod_telescope RUNTIME DESTINATION bin )

install( FILES  ${CMAKE_CURRENT_BINARY_DIR}/indi_eqmod.xml indi_eqmod_sk.xml DESTINATION ${INDI_DATA_DIR})

install( FILES  simulator/indi_eqmod_simulator_sk.xml DESTINATION ${INDI_DATA_DIR})
if(WITH_ALIGN_GEEHALEL)
  install( FILES  align/indi_align_sk.xml DESTINATION ${INDI_DATA_DIR})
endif(WITH_ALIGN_GEEHALEL)
if(WITH_SCOPE_LIMITS)
  install( FILES  scope-limits/indi_eqmod_scope_limits_sk.xml DESTINATION ${INDI_DATA_DIR})
endif(WITH_SCOPE_LIMITS)
