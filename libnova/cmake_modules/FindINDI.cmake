# - Try to find INDI
# Once done this will define
#
#  INDI_FOUND - system has INDI
#  INDI_INCLUDE_DIR - the INDI include directory
#  INDI_LIBRARIES - Link these to use INDI

# Copyright (c) 2006, Jasem Mutlaq <mutlaqja@ikarustech.com>
# Based on FindLibfacile by Carsten Niehaus, <cniehaus@gmx.de>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (INDI_INCLUDE_DIR AND INDI_LIBRARIES)

  # in cache already
  set(INDI_FOUND TRUE)
  message(STATUS "Found INDI: ${INDI_LIBRARIES}")


else (INDI_INCLUDE_DIR AND INDI_LIBRARIES)

  find_path(INDI_INCLUDE_DIR indidevapi.h
    PATH_SUFFIXES libindi
    ${_obIncDir}
    ${GNUWIN32_DIR}/include
  )

  find_library(INDI_LIBRARIES NAMES libindi.a
    PATHS
    ${_obLinkDir}
    ${GNUWIN32_DIR}/lib
  )

  if(INDI_INCLUDE_DIR AND INDI_LIBRARIES)
    set(INDI_FOUND TRUE)
  else (INDI_INCLUDE_DIR AND INDI_LIBRARIES)
    set(INDI_FOUND FALSE)
  endif(INDI_INCLUDE_DIR AND INDI_LIBRARIES)


  if (INDI_FOUND)
    if (NOT INDI_FIND_QUIETLY)
      message(STATUS "Found INDI: ${INDI_LIBRARIES}")
    endif (NOT INDI_FIND_QUIETLY)
  else (INDI_FOUND)
    if (INDI_FIND_REQUIRED)
      message(FATAL_ERROR "INDI not found. Cannot compile SBIG CCD Driver.")
    endif (INDI_FIND_REQUIRED)
  endif (INDI_FOUND)

  mark_as_advanced(INDI_INCLUDE_DIR INDI_LIBRARIES)

endif (INDI_INCLUDE_DIR AND INDI_LIBRARIES)
