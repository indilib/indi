# - Try to find D2XX
# Once done this will define
#
#  D2XX_FOUND - system has FTDI
#  D2XX_INCLUDE_DIR - the FTDI include directory
#  D2XX_LIBRARIES - Link these to use FTDI
#
# N.B. You must include the file as following:
#
#include <ftd2xx.h>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (D2XX_INCLUDE_DIR AND D2XX_LIBRARIES)

  # in cache already
  set(D2XX_FOUND TRUE)
  message(STATUS "Found libfd2xx: ${D2XX_LIBRARIES}")

else (D2XX_INCLUDE_DIR AND D2XX_LIBRARIES)

  find_path(D2XX_INCLUDE_DIR ftd2xx.h
    #PATH_SUFFIXES libD2XX
    ${_obIncDir}
    ${GNUWIN32_DIR}/include
    /usr/local/include
  )

  find_library(D2XX_LIBRARIES NAMES ftd2xx
    PATHS
    ${_obLinkDir}
    ${GNUWIN32_DIR}/lib
    /usr/local/lib
  )

  if(D2XX_INCLUDE_DIR AND D2XX_LIBRARIES)
    set(D2XX_FOUND TRUE)
  else (D2XX_INCLUDE_DIR AND D2XX_LIBRARIES)
    set(D2XX_FOUND FALSE)
  endif(D2XX_INCLUDE_DIR AND D2XX_LIBRARIES)


  if (D2XX_FOUND)
    if (NOT D2XX_FIND_QUIETLY)
      message(STATUS "Found D2XX: ${D2XX_LIBRARIES}")
    endif (NOT D2XX_FIND_QUIETLY)
  else (D2XX_FOUND)
    if (D2XX_FIND_REQUIRED)
      message(FATAL_ERROR "D2XX not found. Please install libd2xx")
    endif (D2XX_FIND_REQUIRED)
  endif (D2XX_FOUND)

  mark_as_advanced(D2XX_INCLUDE_DIR D2XX_LIBRARIES)
  
endif (D2XX_INCLUDE_DIR AND D2XX_LIBRARIES)
