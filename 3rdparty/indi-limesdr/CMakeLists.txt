cmake_minimum_required(VERSION 2.7)
PROJECT(indi_limesdr CXX C)

set (LIMESDR_VERSION_MAJOR 1)
set (LIMESDR_VERSION_MINOR 0)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake_modules/")
set(BIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/bin")

find_package(CFITSIO REQUIRED)
find_package(INDI REQUIRED)
find_package(ZLIB REQUIRED)
find_package(DSPAU REQUIRED)
find_package(LIMESUITE REQUIRED)
find_package(Threads REQUIRED)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_limesdr.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_limesdr.xml)

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${CFITSIO_INCLUDE_DIR})

include(CMakeCommon)

############# LIME-SDR RECEIVER ###############
if (CFITSIO_FOUND)

set(limesdr_SRCS
	${CMAKE_CURRENT_SOURCE_DIR}/indi_limesdr_detector.cpp
)

add_executable(indi_limesdr_detector ${limesdr_SRCS})

target_link_libraries(indi_limesdr_detector ${LIMESUITE_LIBRARIES} ${DSPAU_LIBRARIES} ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${M_LIB} ${CMAKE_THREAD_LIBS_INIT})

install(TARGETS indi_limesdr_detector RUNTIME DESTINATION bin)

endif (CFITSIO_FOUND)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_limesdr.xml DESTINATION ${INDI_DATA_DIR})
