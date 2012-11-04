cmake_minimum_required(VERSION 2.4.7)
PROJECT(fliccd CXX C)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake_modules/")
set(BIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/bin")

find_package(CFITSIO REQUIRED)
find_package(INDI REQUIRED)
find_package(FLI REQUIRED)
find_package(ZLIB REQUIRED)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${CFITSIO_INCLUDE_DIR})
include_directories( ${FLI_INCLUDE_DIR})

###### FLI Precision Digital Focuser ######
set(flipdf_SRCS
        ${CMAKE_SOURCE_DIR}/fli_pdf.cpp
)

add_executable(indi_fli_focus ${flipdf_SRCS})

target_link_libraries(indi_fli_focus ${INDI_DRIVER_LIBRARIES} ${FLI_LIBRARIES} m z)

install(TARGETS indi_fli_focus RUNTIME DESTINATION bin )

###### FLI Filter wheel ######
set(flicfw_SRCS
        ${CMAKE_SOURCE_DIR}/fli_cfw.cpp
)

add_executable(indi_fli_wheel ${flicfw_SRCS})

target_link_libraries(indi_fli_wheel ${INDI_DRIVER_LIBRARIES} ${FLI_LIBRARIES} m z)

install(TARGETS indi_fli_wheel RUNTIME DESTINATION bin )
############# FLI CCD ###############
set(fliccd_SRCS
	${CMAKE_SOURCE_DIR}/fli_ccd.cpp
)

add_executable(indi_fli_ccd ${fliccd_SRCS})

target_link_libraries(indi_fli_ccd ${INDI_DRIVER_LIBRARIES} ${FLI_LIBRARIES} ${CFITSIO_LIBRARIES} m z)

install(TARGETS indi_fli_ccd RUNTIME DESTINATION bin)

install(FILES indi_fli.xml DESTINATION ${INDI_DATA_DIR})
