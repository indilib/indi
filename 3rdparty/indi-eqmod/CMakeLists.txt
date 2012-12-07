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
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${NOVA_INCLUDE_DIR})

########### EQMod ###############
set(eqmod_SRCS
   ${CMAKE_SOURCE_DIR}/eqmod.cpp ${CMAKE_SOURCE_DIR}/eqmoderror.cpp ${CMAKE_SOURCE_DIR}/skywatcher.cpp ${CMAKE_SOURCE_DIR}/align/align.cpp
   ${CMAKE_SOURCE_DIR}/align/pointset.cpp ${CMAKE_SOURCE_DIR}/align/htm.c)

add_executable(indi_eqmod_telescope ${eqmod_SRCS})

target_link_libraries(indi_eqmod_telescope ${INDI_LIBRARIES} ${INDI_DRIVER_LIBRARIES} ${NOVA_LIBRARIES} z)

install(TARGETS indi_eqmod_telescope RUNTIME DESTINATION bin )

install( FILES  indi_eqmod.xml indi_eqmod_sk.xml align/indi_align_sk.xml DESTINATION ${INDI_DATA_DIR})
