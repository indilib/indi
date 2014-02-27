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

  find_path(APOGEE_INCLUDE_DIR ApogeeCam.h
    PATH_SUFFIXES libapogee
    ${_obIncDir}
    ${GNUWIN32_DIR}/include
  )

  # Find Apogee Library
  find_library(APOGEE_LIBRARIES NAMES apogee
    PATHS
    ${_obLinkDir}
    ${GNUWIN32_DIR}/lib
  )
  
  if(APOGEE_INCLUDE_DIR AND APOGEE_LIBRARIES)
    set(APOGEE_FOUND TRUE)
  else (APOGEE_INCLUDE_DIR AND APOGEE_LIBRARIES)
    set(APOGEE_FOUND FALSE)
  endif(APOGEE_INCLUDE_DIR AND APOGEE_LIBRARIES)

  if (APOGEE_FOUND)
    if (NOT APOGEE_FIND_QUIETLY)
      message(STATUS "Found APOGEE: ${APOGEE_LIBRARIES}")
    endif (NOT APOGEE_FIND_QUIETLY)
  else (APOGEE_FOUND)
    if (APOGEE_FIND_REQUIRED)
      message(FATAL_ERROR "libapogee not found. Cannot compile Apogee CCD Driver. Please install libapogee and try again. http://www.indilib.org")
    endif (APOGEE_FIND_REQUIRED)
  endif (APOGEE_FOUND) 

 mark_as_advanced(APOGEE_INCLUDE_DIR APOGEE_LIBRARIES)

endif (APOGEE_INCLUDE_DIR AND APOGEE_LIBRARIES)
