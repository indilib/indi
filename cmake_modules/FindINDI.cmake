# - Try to find INDI
# Once done this will define
#
#  INDI_FOUND - system has INDI
#  INDI_INCLUDE_DIR - the INDI include directory
#  INDI_LIBRARIES - Link these to use INDI
#  INDI_MAIN_LIBRARIES - Link to these to build INDI drivers with main()
#  INDI_DRIVER_LIBRARIES - Link to these to build INDI drivers with indibase support
#  INDI_CLIENT_LIBRARIES - Link to these to build INDI clients
#  INDI_DATA_DIR - INDI shared data dir.

# Copyright (c) 2011, Jasem Mutlaq <mutlaqja@ikarustech.com>
# Based on FindLibfacile by Carsten Niehaus, <cniehaus@gmx.de>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (INDI_INCLUDE_DIR AND INDI_DATA_DIR AND INDI_LIBRARIES AND INDI_DRIVER_LIBRARIES AND INDI_MAIN_LIBRARIES)

  # in cache already
  set(INDI_FOUND TRUE)
  message(STATUS "Found INDI: ${INDI_LIBRARIES}")


else (INDI_INCLUDE_DIR AND INDI_DATA_DIR AND INDI_LIBRARIES AND INDI_DRIVER_LIBRARIES AND INDI_MAIN_LIBRARIES)

  find_path(INDI_INCLUDE_DIR indidevapi.h
    PATH_SUFFIXES libindi
    ${_obIncDir}
    ${GNUWIN32_DIR}/include
  )

 find_path(INDI_DATA_DIR drivers.xml
    PATHS /usr/share /usr/local/share /opt ${GNUWIN32_DIR}/share
    PATH_SUFFIXES indi
  )

  find_library(INDI_LIBRARIES NAMES indi
    PATHS
    ${_obLinkDir}
    ${GNUWIN32_DIR}/lib
  )

  find_library(INDI_DRIVER_LIBRARIES NAMES indidriver
    PATHS
    ${_obLinkDir}
    ${GNUWIN32_DIR}/lib
  )

  find_library(INDI_MAIN_LIBRARIES NAMES indimain
    PATHS
    ${_obLinkDir}
    ${GNUWIN32_DIR}/lib
  )

  find_library(INDI_CLIENT_LIBRARIES NAMES indiclient
    PATHS
    ${_obLinkDir}
    ${GNUWIN32_DIR}/lib
  )

  if(INDI_INCLUDE_DIR AND INDI_DATA_DIR AND INDI_LIBRARIES AND INDI_DRIVER_LIBRARIES AND INDI_MAIN_LIBRARIES)
    set(INDI_FOUND TRUE)
  else (INDI_INCLUDE_DIR AND INDI_DATA_DIR AND INDI_LIBRARIES AND INDI_DRIVER_LIBRARIES AND INDI_MAIN_LIBRARIES)
    set(INDI_FOUND FALSE)
  endif(INDI_INCLUDE_DIR AND INDI_DATA_DIR AND INDI_LIBRARIES AND INDI_DRIVER_LIBRARIES AND INDI_MAIN_LIBRARIES)


  if (INDI_FOUND)
    if (NOT INDI_FIND_QUIETLY)
      message(STATUS "Found INDI: ${INDI_LIBRARIES}, ${INDI_BASE_DRIVER_LIBRARIES}")
      message(STATUS "INDI Include: ${INDI_INCLUDE_DIR}, INDI Data: ${INDI_DATA_DIR}")
    endif (NOT INDI_FIND_QUIETLY)
  else (INDI_FOUND)
    if (INDI_FIND_REQUIRED)
      message(FATAL_ERROR "indi-dev not found. Cannot compile INDI drivers. Please install indi-dev and try again. http://www.indilib.org")
    endif (INDI_FIND_REQUIRED)
  endif (INDI_FOUND)

  mark_as_advanced(INDI_INCLUDE_DIR INDI_DATA_DIR INDI_LIBRARIES INDI_DRIVER_LIBRARIES INDI_MAIN_LIBRARIES INDI_CLIENT_LIBRARIES)

endif (INDI_INCLUDE_DIR AND INDI_DATA_DIR AND INDI_LIBRARIES AND INDI_DRIVER_LIBRARIES AND INDI_MAIN_LIBRARIES)
