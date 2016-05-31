PROJECT(indi-eqmod C CXX)
cmake_minimum_required(VERSION 2.4.7)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake_modules/")
include(GNUInstallDirs)

set(BIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/bin")
set(CMAKE_CXX_FLAGS "-std=c++0x ${CMAKE_CXX_FLAGS}")

find_package(INDI REQUIRED)
find_package(Nova REQUIRED)
find_package(ZLIB REQUIRED)

set(EQMOD_VERSION_MAJOR 0)
set(EQMOD_VERSION_MINOR 3)

option(WITH_ALIGN "Enable Alignment Subsystem" ON)
option(WITH_ALIGN_GEEHALEL "Enable EQMod Alignment" ON)
option(WITH_SIMULATOR "Enable Simulator" ON)
option(WITH_NOFMANY "Use NOFMANY switches" ON)
option(WITH_SCOPE_LIMITS "Enable Scope limits" ON)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )
set(INDI_DATA_DIR "${CMAKE_INSTALL_PREFIX}/share/indi")

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${NOVA_INCLUDE_DIR})
include_directories( ${NOVA_INCLUDE_DIR}/..)

########### EQMod ###############
set(eqmod_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/eqmod.cpp ${CMAKE_CURRENT_SOURCE_DIR}/eqmoderror.cpp ${CMAKE_CURRENT_SOURCE_DIR}/skywatcher.cpp)

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  set(eqmod_SRCS ${eqmod_SRCS}
    ${CMAKE_CURRENT_SOURCE_DIR}/mach_gettime.cpp)
endif()

if(WITH_SIMULATOR)
  set(eqmod_SRCS ${eqmod_SRCS}
   ${CMAKE_CURRENT_SOURCE_DIR}/simulator/simulator.cpp    ${CMAKE_CURRENT_SOURCE_DIR}/simulator/skywatcher-simulator.cpp)
endif(WITH_SIMULATOR)
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
  find_library(INDI_ALIGN_LIBRARIES NAMES indiAlignmentDriver
      HINTS ${PC_INDI_LIBRARY_DIRS}
  )
  target_link_libraries(indi_eqmod_telescope ${INDI_LIBRARIES} ${INDI_DRIVER_LIBRARIES} ${NOVA_LIBRARIES} ${INDI_ALIGN_LIBRARIES})
else(WITH_ALIGN)
  target_link_libraries(indi_eqmod_telescope ${INDI_LIBRARIES} ${INDI_DRIVER_LIBRARIES} ${NOVA_LIBRARIES})
endif(WITH_ALIGN)

if (CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "arm*")
target_link_libraries(indi_eqmod_telescope rt)
endif (CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "arm*")

install(TARGETS indi_eqmod_telescope RUNTIME DESTINATION bin )

install( FILES  indi_eqmod.xml indi_eqmod_sk.xml DESTINATION ${INDI_DATA_DIR})

if (WITH_SIMULATOR)
  install( FILES  simulator/indi_eqmod_simulator_sk.xml DESTINATION ${INDI_DATA_DIR})
endif(WITH_SIMULATOR)
if(WITH_ALIGN_GEEHALEL)
  install( FILES  align/indi_align_sk.xml DESTINATION ${INDI_DATA_DIR})
endif(WITH_ALIGN_GEEHALEL)
if(WITH_SCOPE_LIMITS)
  install( FILES  scope-limits/indi_eqmod_scope_limits_sk.xml DESTINATION ${INDI_DATA_DIR})
endif(WITH_SCOPE_LIMITS)
