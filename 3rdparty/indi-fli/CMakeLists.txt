cmake_minimum_required(VERSION 2.4.7)
PROJECT(fliccd CXX C)

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
find_package(FLI REQUIRED)
find_package(ZLIB REQUIRED)

macro_optional_find_package(Nova)
macro_bool_to_01(NOVA_FOUND HAVE_NOVA_H)
macro_log_feature(NOVA_FOUND "libnova" "A general purpose, double precision, Celestial Mechanics, Astrometry and Astrodynamics library" "http://libnova.sourceforge.net" FALSE "0.12.1" "Needed by INDI.")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${CFITSIO_INCLUDE_DIR})
include_directories( ${FLI_INCLUDE_DIR})

if (NOVA_FOUND)
    include_directories(${NOVA_INCLUDE_DIR})
endif (NOVA_FOUND)

###### FLI Precision Digital Focuser ######
if (FLI_FOUND)
set(flipdf_SRCS
        ${CMAKE_SOURCE_DIR}/fli_pdf.cpp
)

add_executable(indi_fli_focus ${flipdf_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_fli_focus ${INDI_LIBRARIES} ${INDI_DRIVER_LIBRARIES} ${FLI_LIBRARIES} m z)

if (NOVA_FOUND)
  target_link_libraries(indi_fli_focus ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

install(TARGETS indi_fli_focus RUNTIME DESTINATION bin )
endif(FLI_FOUND)

###### FLI Filter wheel ######
if (FLI_FOUND)
set(flicfw_SRCS
        ${CMAKE_SOURCE_DIR}/fli_cfw.cpp
)

add_executable(indi_fli_wheel ${flicfw_SRCS} ${liblilxml_SRCS} ${libindicom_SRCS})

target_link_libraries(indi_fli_wheel ${INDI_LIBRARIES} ${INDI_DRIVER_LIBRARIES} ${FLI_LIBRARIES} m z)

if (NOVA_FOUND)
  target_link_libraries(indi_fli_wheel ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

install(TARGETS indi_fli_wheel RUNTIME DESTINATION bin )
endif(FLI_FOUND)

############# FLI CCD ###############
if (CFITSIO_FOUND AND FLI_FOUND)

set(fliccd_SRCS
	${CMAKE_SOURCE_DIR}/fli_ccd.cpp
)

add_executable(indi_fli_ccd ${fliccd_SRCS})

target_link_libraries(indi_fli_ccd ${INDI_LIBRARIES} ${INDI_DRIVER_LIBRARIES} ${FLI_LIBRARIES} ${CFITSIO_LIBRARIES} m z)

if (NOVA_FOUND)
  target_link_libraries(indi_fli_ccd ${NOVA_LIBRARIES})
endif (NOVA_FOUND)

install(TARGETS indi_fli_ccd RUNTIME DESTINATION bin)

endif (CFITSIO_FOUND AND FLI_FOUND)

install(FILES indi_fli.xml DESTINATION ${INDI_DATA_DIR})
