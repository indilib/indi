cmake_minimum_required(VERSION 2.4.7)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake_modules/")
set(BIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/bin")
set(INCLUDE_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/include")

Include (CheckCXXSourceCompiles)
include (MacroOptionalFindPackage)
include (MacroLogFeature)
include (MacroBoolTo01)
include (CheckIncludeFiles)

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_SOURCE_DIR})
include_directories( ${CMAKE_SOURCE_DIR}/libnova)

########### libanova ###########
set(libnova_SRCS
	${CMAKE_SOURCE_DIR}/julian_day.c
	${CMAKE_SOURCE_DIR}/dynamical_time.c
	${CMAKE_SOURCE_DIR}/sidereal_time.c
	${CMAKE_SOURCE_DIR}/transform.c
	${CMAKE_SOURCE_DIR}/nutation.c
	${CMAKE_SOURCE_DIR}/aberration.c
	${CMAKE_SOURCE_DIR}/apparent_position.c
	${CMAKE_SOURCE_DIR}/solar.c
	${CMAKE_SOURCE_DIR}/precession.c
	${CMAKE_SOURCE_DIR}/proper_motion.c
	${CMAKE_SOURCE_DIR}/mercury.c
	${CMAKE_SOURCE_DIR}/venus.c
	${CMAKE_SOURCE_DIR}/earth.c
	${CMAKE_SOURCE_DIR}/mars.c
	${CMAKE_SOURCE_DIR}/jupiter.c
	${CMAKE_SOURCE_DIR}/saturn.c
	${CMAKE_SOURCE_DIR}/uranus.c
	${CMAKE_SOURCE_DIR}/neptune.c
	${CMAKE_SOURCE_DIR}/pluto.c
	${CMAKE_SOURCE_DIR}/vsop87.c
	${CMAKE_SOURCE_DIR}/lunar.c
	${CMAKE_SOURCE_DIR}/elliptic_motion.c
	${CMAKE_SOURCE_DIR}/asteroid.c
	${CMAKE_SOURCE_DIR}/comet.c
	${CMAKE_SOURCE_DIR}/parabolic_motion.c
	${CMAKE_SOURCE_DIR}/utility.c
	${CMAKE_SOURCE_DIR}/refraction.c
	${CMAKE_SOURCE_DIR}/rise_set.c
	${CMAKE_SOURCE_DIR}/angular_separation.c
	${CMAKE_SOURCE_DIR}/hyperbolic_motion.c
	${CMAKE_SOURCE_DIR}/parallax.c
	${CMAKE_SOURCE_DIR}/airmass.c
)

add_library(nova SHARED ${libnova_SRCS})

set_target_properties(nova PROPERTIES VERSION 0.12.2 SOVERSION 0)

target_link_libraries(nova m)

install(TARGETS nova LIBRARY DESTINATION lib${LIB_POSTFIX} )

FILE(GLOB files "${CMAKE_SOURCE_DIR}/libnova/*.h")

install( FILES ${files} DESTINATION ${INCLUDE_INSTALL_DIR}/libnova COMPONENT Devel)
