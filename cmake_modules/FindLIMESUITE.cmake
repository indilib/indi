# - Try to find LIMESUITE
# Once done this will define
#
#  LIMESUITE_FOUND - system has LIMESUITE
#  LIMESUITE_INCLUDE_DIR - the LIMESUITE include directory
#  LIMESUITE_LIBRARIES - Link these to use LIMESUITE
#  LIMESUITE_VERSION_STRING - Human readable version number of rtlsdr
#  LIMESUITE_VERSION_MAJOR  - Major version number of rtlsdr
#  LIMESUITE_VERSION_MINOR  - Minor version number of rtlsdr

# Copyright (c) 2017, Ilia Platone, <info@iliaplatone.com>
# Based on FindLibfacile by Carsten Niehaus, <cniehaus@gmx.de>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (LIMESUITE_LIBRARIES)

  # in cache already
  set(LIMESUITE_FOUND TRUE)
  message(STATUS "Found LIMESUITE: ${LIMESUITE_LIBRARIES}")


else (LIMESUITE_LIBRARIES)

  find_library(LIMESUITE_LIBRARIES NAMES LimeSuite
    PATHS
    ${_obLinkDir}
    ${GNUWIN32_DIR}/lib
    /usr/local/lib
  )

  if(LIMESUITE_LIBRARIES)
    set(LIMESUITE_FOUND TRUE)
  else (LIMESUITE_LIBRARIES)
    set(LIMESUITE_FOUND FALSE)
  endif(LIMESUITE_LIBRARIES)


  if (LIMESUITE_FOUND)
    if (NOT LIMESUITE_FIND_QUIETLY)
      message(STATUS "Found LIMESUITE: ${LIMESUITE_LIBRARIES}")
    endif (NOT LIMESUITE_FIND_QUIETLY)
  else (LIMESUITE_FOUND)
    if (LIMESUITE_FIND_REQUIRED)
      message(FATAL_ERROR "LIMESUITE not found. Please install libLimeSuite-dev")
    endif (LIMESUITE_FIND_REQUIRED)
  endif (LIMESUITE_FOUND)

  mark_as_advanced(LIMESUITE_LIBRARIES)
  
endif (LIMESUITE_LIBRARIES)
