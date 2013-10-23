PROJECT(indi-eqmod C CXX)
cmake_minimum_required(VERSION 2.4.7)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake_modules/")
set(BIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/bin")

Include (CheckCXXSourceCompiles)

#SET(CMAKE_CXX_FLAGS "-Wall" )
#SET(CMAKE_CXX_FLAGS_DEBUG "-Werror -O0 -g -DDEBUG_FITS" )
#SET(CMAKE_C_FLAGS "-Wall" )
#SET(CMAKE_C_FLAGS_DEBUG "-Werror -O0 -g -DDEBUG_FITS" )


find_package(INDI REQUIRED)
find_package(Nova REQUIRED)
find_package(ZLIB REQUIRED)
#find_package(Threads REQUIRED)

set(EQMOD_VERSION_MAJOR 0)
set(EQMOD_VERSION_MINOR 1)
option(WITH_ALIGN_GEEHALEL "Enable Alignment Test" ON)
option(WITH_SIMULATOR "Enable Simulator" ON)
option(WITH_LOGGER "Enable File Logging" OFF)
option(WITH_NOFMANY "Use NOFMANY switches" ON)
option(STOP_WHEN_MOTION_CHANGED "Stop motors when motion mode changes" ON)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${NOVA_INCLUDE_DIR})

########### EQMod ###############
set(eqmod_SRCS
   ${CMAKE_SOURCE_DIR}/eqmod.cpp ${CMAKE_SOURCE_DIR}/eqmoderror.cpp ${CMAKE_SOURCE_DIR}/skywatcher.cpp 
   ${CMAKE_SOURCE_DIR}/logger/Logger.cpp)

if(WITH_SIMULATOR)
  set(eqmod_SRCS ${eqmod_SRCS}
   ${CMAKE_SOURCE_DIR}/simulator/simulator.cpp    ${CMAKE_SOURCE_DIR}/simulator/skywatcher-simulator.cpp)
endif(WITH_SIMULATOR)
if(WITH_ALIGN_GEEHALEL)
  set(eqmod_SRCS ${eqmod_SRCS}
   ${CMAKE_SOURCE_DIR}/align/align.cpp ${CMAKE_SOURCE_DIR}/align/pointset.cpp ${CMAKE_SOURCE_DIR}/align/htm.c
   ${CMAKE_SOURCE_DIR}/align/triangulate.cpp ${CMAKE_SOURCE_DIR}/align/triangulate_chull.cpp ${CMAKE_SOURCE_DIR}/align/chull/chull.c)
endif(WITH_ALIGN_GEEHALEL)

add_executable(indi_eqmod_telescope ${eqmod_SRCS})

target_link_libraries(indi_eqmod_telescope ${INDI_LIBRARIES} ${INDI_DRIVER_LIBRARIES} ${NOVA_LIBRARIES} z)

install(TARGETS indi_eqmod_telescope RUNTIME DESTINATION bin )

install( FILES  indi_eqmod.xml indi_eqmod_sk.xml DESTINATION ${INDI_DATA_DIR})

if (WITH_SIMULATOR)
  install( FILES  simulator/indi_eqmod_simulator_sk.xml DESTINATION ${INDI_DATA_DIR})
endif(WITH_SIMULATOR)
if(WITH_ALIGN_GEEHALEL)
  install( FILES  align/indi_align_sk.xml DESTINATION ${INDI_DATA_DIR})
endif(WITH_ALIGN_GEEHALEL)
