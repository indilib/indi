cmake_minimum_required(VERSION 2.4.7)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake_modules/")
set(DATA_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/share/indi/")
set(BIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/bin")

Include (CheckCXXSourceCompiles)
include (MacroOptionalFindPackage)
include (MacroLogFeature)
include (MacroBoolTo01)
include (CheckIncludeFiles)

find_package(USB REQUIRED)
find_package(CFITSIO REQUIRED)
find_package(INDI REQUIRED)
find_package(ZLIB REQUIRED)
find_package(MODBUS REQUIRED)
find_package(Nova REQUIRED)
find_package(ALUT REQUIRED)

if (ALUT_FOUND STREQUAL "NO")
 message (FATAL_ERROR "Alut library was not found on this system. Please install Alut development files.")
else (ALUT_FOUND STREQUAL "NO")
 message (STATUS "Found Alut: " ${ALUT_LIBRARY})
endif(ALUT_FOUND STREQUAL "NO")

find_package(Vorbis REQUIRED)

if (NOT VORBIS_FOUND)
 message (FATAL_ERROR "Vorbis library was not found on this system. Please install Vorbis development files.")
endif (NOT VORBIS_FOUND)

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${CFITSIO_INCLUDE_DIR})
include_directories( ${ALUT_INCLUDE_DIR})
include_directories( ${MODBUS_INCLUDE_DIR})
include_directories(${NOVA_INCLUDE_DIR})

########### KNRO ###########
set(indiknro_SRCS
   ${CMAKE_SOURCE_DIR}/knro.cpp
   ${CMAKE_SOURCE_DIR}/encoder.cpp
   ${CMAKE_SOURCE_DIR}/inverter.cpp
   ${CMAKE_SOURCE_DIR}/polling.cpp
   ${CMAKE_SOURCE_DIR}/ogg_util.cpp
   )

add_executable(indi_knro ${indiknro_SRCS})

# openal alut vorbis vorbisfile

target_link_libraries(indi_knro ${INDI_LIBRARIES} ${INDI_DRIVER_LIBRARIES} ${MODBUS_LIBRARIES} ${CFITSIO_LIBRARIES} ${ALUT_LIBRARY} ${VORBIS_LIBRARY} ${VORBISFILE_LIBRARY} z pthread)

if (NOVA_FOUND)
  target_link_libraries(indi_knro ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

install(TARGETS indi_knro RUNTIME DESTINATION bin )

install(FILES indi_knro.xml DESTINATION ${DATA_INSTALL_DIR})
install(FILES sounds/alarm1.ogg DESTINATION ${DATA_INSTALL_DIR})
install(FILES sounds/alarm2.ogg DESTINATION ${DATA_INSTALL_DIR})
install(FILES sounds/calibration_error.ogg DESTINATION ${DATA_INSTALL_DIR})
install(FILES sounds/slew_error.ogg DESTINATION ${DATA_INSTALL_DIR})
install(FILES sounds/slew_complete.ogg DESTINATION ${DATA_INSTALL_DIR})
install(FILES sounds/slew_busy.ogg DESTINATION ${DATA_INSTALL_DIR})
