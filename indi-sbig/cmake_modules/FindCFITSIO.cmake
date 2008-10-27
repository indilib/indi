# - Try to find CFITSIO
# Once done this will define
#
#  CFITSIO_FOUND - system has CFITSIO
#  CFITSIO_INCLUDE_DIR - the CFITSIO include directory
#  CFITSIO_LIBRARIES - Link these to use CFITSIO

# Copyright (c) 2006, Jasem Mutlaq <mutlaqja@ikarustech.com>
# Based on FindLibfacile by Carsten Niehaus, <cniehaus@gmx.de>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (CFITSIO_INCLUDE_DIR AND CFITSIO_LIBRARIES)

  # in cache already
  set(CFITSIO_FOUND TRUE)
  message(STATUS "Found CFITSIO: ${CFITSIO_LIBRARIES}")


else (CFITSIO_INCLUDE_DIR AND CFITSIO_LIBRARIES)

  find_path(CFITSIO_INCLUDE_DIR fitsio.h
    PATH_SUFFIXES libcfitsio3
    ${_obIncDir}
    ${GNUWIN32_DIR}/include
  )

  find_library(CFITSIO_LIBRARIES NAMES cfitsio
    PATHS
    ${_obLinkDir}
    ${GNUWIN32_DIR}/lib
  )

  if(CFITSIO_INCLUDE_DIR AND CFITSIO_LIBRARIES)
    set(CFITSIO_FOUND TRUE)
  else (CFITSIO_INCLUDE_DIR AND CFITSIO_LIBRARIES)
    set(CFITSIO_FOUND FALSE)
  endif(CFITSIO_INCLUDE_DIR AND CFITSIO_LIBRARIES)


  if (CFITSIO_FOUND)
    if (NOT CFITSIO_FIND_QUIETLY)
      message(STATUS "Found CFITSIO: ${CFITSIO_LIBRARIES}")
    endif (NOT CFITSIO_FIND_QUIETLY)
  else (CFITSIO_FOUND)
    if (CFITSIO_FIND_REQUIRED)
      message(FATAL_ERROR "CFITSIO not found. Please install libcfitsio3 and try again. http://indi.sf.net")
    endif (CFITSIO_FIND_REQUIRED)
  endif (CFITSIO_FOUND)

  mark_as_advanced(CFITSIO_INCLUDE_DIR CFITSIO_LIBRARIES)

endif (CFITSIO_INCLUDE_DIR AND CFITSIO_LIBRARIES)
