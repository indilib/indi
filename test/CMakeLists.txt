cmake_minimum_required(VERSION 3.13)

MESSAGE (STATUS "GTEST_BOTH_LIBRARIES ${GTEST_BOTH_LIBRARIES}")
MESSAGE (STATUS "GMOCK_LIBRARIES ${GMOCK_LIBRARIES}")

INCLUDE_DIRECTORIES ( ${CMAKE_SOURCE_DIR} )

# Workaround for fixing a linking error caused by "-pie" flag in CMakeCommon
STRING(REPLACE "-pie" "" CMAKE_EXE_LINKER_FLAGS ${CMAKE_EXE_LINKER_FLAGS})

ADD_SUBDIRECTORY(core)
ADD_SUBDIRECTORY(celestrondriver)
# JM 2021-05-29: Disable LX200 Drivers test until Eric can solve the issue.
#ADD_SUBDIRECTORY(lx200drivers)
ADD_SUBDIRECTORY(drivers)
ADD_SUBDIRECTORY(scopesim_helper)
ADD_SUBDIRECTORY(alignment)
