# - RTK Realtime Toolkit Precise GNSS positioning
# Once done this will define
#
#  RTK_FOUND - system has RTK
#  RTK_INCLUDE_DIR - the RTK include directory
#  RTK_LIBRARIES - Link these to use RTK

# Copyright (c) 2008, Ilia Platone <info@iliaplatone.com>
# Based on FindLibfacile by Carsten Niehaus, <cniehaus@gmx.de>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (RTK_INCLUDE_DIR AND RTK_LIBRARIES)

  # in cache already
  set(RTK_FOUND TRUE)
  message(STATUS "Found librtk: ${RTK_LIBRARIES}")

else (RTK_INCLUDE_DIR AND RTK_LIBRARIES)

  find_path(RTK_INCLUDE_DIR librtk.h
    PATH_SUFFIXES rtk
    ${_obIncDir}
    ${GNUWIN32_DIR}/include/librtk
  )

  find_library(RTK_LIBRARIES NAMES rtk
    PATHS
    ${_obLinkDir}
    ${GNUWIN32_DIR}/lib
  )

  if(RTK_INCLUDE_DIR AND RTK_LIBRARIES)
    set(RTK_FOUND TRUE)
  else (RTK_INCLUDE_DIR AND RTK_LIBRARIES)
    set(RTK_FOUND FALSE)
  endif(RTK_INCLUDE_DIR AND RTK_LIBRARIES)


  if (RTK_FOUND)
    if (NOT RTK_FIND_QUIETLY)
      message(STATUS "Found RTK: ${RTK_LIBRARIES}")
    endif (NOT RTK_FIND_QUIETLY)
  else (RTK_FOUND)
    if (RTK_FIND_REQUIRED)
      message(FATAL_ERROR "RTK not found. Please install librtk-dev. http://www.indilib.org")
    endif (RTK_FIND_REQUIRED)
  endif (RTK_FOUND)

  mark_as_advanced(RTK_INCLUDE_DIR RTK_LIBRARIES)
  
endif (RTK_INCLUDE_DIR AND RTK_LIBRARIES)
