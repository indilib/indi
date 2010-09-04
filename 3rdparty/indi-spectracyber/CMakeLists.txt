cmake_minimum_required(VERSION 2.4.7)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake_modules/")
set(DATA_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/share/indi/")
set(BIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/bin")

Include (CheckCXXSourceCompiles)
include (MacroOptionalFindPackage)
include (MacroLogFeature)
include (MacroBoolTo01)
include (CheckIncludeFiles)

find_package(CFITSIO REQUIRED)
find_package(INDI REQUIRED)
find_package(ZLIB REQUIRED)

macro_optional_find_package(Nova)
macro_bool_to_01(NOVA_FOUND HAVE_NOVA_H)
macro_log_feature(NOVA_FOUND "libnova" "A general purpose, double precision, Celestial Mechanics, Astrometry and Astrodynamics library" "http://libnova.sourceforge.net" FALSE "0.12.1" "Needed by INDI.")

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})

if (NOVA_FOUND)
    include_directories(${NOVA_INCLUDE_DIR})
endif (NOVA_FOUND)

########### SpectraCyber ###########
set(indispectracyber_SRCS
   ${CMAKE_SOURCE_DIR}/spectracyber.cpp
   )

add_executable(indi_spectracyber ${indispectracyber_SRCS})

target_link_libraries(indi_spectracyber ${INDI_LIBRARIES} ${INDI_DRIVER_LIBRARIES} z pthread)

if (NOVA_FOUND)
  target_link_libraries(indi_spectracyber ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

install(TARGETS indi_spectracyber RUNTIME DESTINATION bin )

install(FILES indi_spectracyber.xml DESTINATION ${DATA_INSTALL_DIR})

