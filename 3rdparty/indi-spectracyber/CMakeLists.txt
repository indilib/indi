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
find_package(Nova REQUIRED)

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})



########### SpectraCyber ###########
set(indispectracyber_SRCS
   ${CMAKE_SOURCE_DIR}/spectracyber.cpp
   )

add_executable(indi_spectracyber ${indispectracyber_SRCS})

target_link_libraries(indi_spectracyber ${INDI_LIBRARIES} ${INDI_DRIVER_LIBRARIES} ${NOVA_LIBRARIES} z pthread)

install(TARGETS indi_spectracyber RUNTIME DESTINATION bin )

install(FILES indi_spectracyber.xml DESTINATION ${DATA_INSTALL_DIR})

