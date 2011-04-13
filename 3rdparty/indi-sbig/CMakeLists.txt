cmake_minimum_required(VERSION 2.4.7)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake_modules/")
set(BIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/bin")

Include (CheckCXXSourceCompiles)
include (MacroOptionalFindPackage)
include (MacroLogFeature)
include (MacroBoolTo01)
include (CheckIncludeFiles)

find_package(USB REQUIRED)
find_package(CFITSIO REQUIRED)
find_package(INDI REQUIRED)
find_package(SBIG REQUIRED)
find_package(ZLIB REQUIRED)

macro_optional_find_package(Nova)
macro_bool_to_01(NOVA_FOUND HAVE_NOVA_H)
macro_log_feature(NOVA_FOUND "libnova" "A general purpose, double precision, Celestial Mechanics, Astrometry and Astrodynamics library" "http://libnova.sourceforge.net" FALSE "0.12.1" "Needed by INDI.")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${CFITSIO_INCLUDE_DIR})

if (NOVA_FOUND)
    include_directories(${NOVA_INCLUDE_DIR})
endif (NOVA_FOUND)

########### SBIG ###########
set(indisbig_SRCS
   ${CMAKE_SOURCE_DIR}/sbigcam.cpp
   )
   
add_executable(indi_sbig_ccd ${indisbig_SRCS})

target_link_libraries(indi_sbig_ccd ${INDI_LIBRARIES} ${INDI_MAIN_LIBRARIES} ${CFITSIO_LIBRARIES} ${SBIG_LIBRARIES} z)

if (NOVA_FOUND)
  target_link_libraries(indi_sbig_ccd ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

install(TARGETS indi_sbig_ccd RUNTIME DESTINATION bin )

install(FILES indi_sbig.xml DESTINATION ${INDI_DATA_DIR})
