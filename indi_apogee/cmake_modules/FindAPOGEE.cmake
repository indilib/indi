# - Try to find Apogee Instruments Library
# Once done this will define
#
#  APOGEE_FOUND - system has APOGEE
#  APOGEE_INCLUDE_DIR - the APOGEE include directory
#  APOGEE_LIBRARIES - Link these to use APOGEE

# Copyright (c) 2008, Jasem Mutlaq <mutlaqja@ikarustech.com>
# Based on FindLibfacile by Carsten Niehaus, <cniehaus@gmx.de>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (APOGEE_INCLUDE_DIR AND APOGEE_LIBRARIES)

  # in cache already
  set(APOGEE_FOUND TRUE)
  message(STATUS "Found libapogee: ${APOGEE_LIBRARIES}")

else (APOGEE_INCLUDE_DIR AND APOGEE_LIBRARIES)

  find_path(APOGEE_INCLUDE_DIR libapogee.h
    PATH_SUFFIXES libapogee
    ${_obIncDir}
    ${GNUWIN32_DIR}/include
  )

  find_library(APOGEE_LIBRARIES NAMES apogeeu
    PATHS
    ${_obLinkDir}
    ${GNUWIN32_DIR}/lib
  )

 set(CMAKE_REQUIRED_INCLUDES ${APOGEE_INCLUDE_DIR})
 set(CMAKE_REQUIRED_LIBRARIES ${APOGEE_LIBRARIES})
 
 include(FindPackageHandleStandardArgs)
 FIND_PACKAGE_HANDLE_STANDARD_ARGS(APOGEE DEFAULT_MSG APOGEE_LIBRARIES APOGEE_INCLUDE_DIR)
 mark_as_advanced(APOGEE_INCLUDE_DIR APOGEE_LIBRARIES)

endif (APOGEE_INCLUDE_DIR AND APOGEE_LIBRARIES)
