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

if (APOGEE_INCLUDE_DIR AND APOGEEU_LIBRARIES AND APOGEEE_LIBRARIES)

  # in cache already
  set(APOGEE_FOUND TRUE)
  message(STATUS "Found libapogee: ${APOGEEU_LIBRARIES}, ${APOGEEE_LIBRARIES}")

else (APOGEE_INCLUDE_DIR AND APOGEEU_LIBRARIES AND APOGEEE_LIBRARIES)

  find_path(APOGEE_INCLUDE_DIR libapogee.h
    PATH_SUFFIXES libapogee
    ${_obIncDir}
    ${GNUWIN32_DIR}/include
  )

  # Find Apogee Alta-U Library
  find_library(APOGEEU_LIBRARIES NAMES apogeeu
    PATHS
    ${_obLinkDir}
    ${GNUWIN32_DIR}/lib
  )

  # Find Apogee Alta-U Library
  find_library(APOGEEE_LIBRARIES NAMES apogeee
    PATHS
    ${_obLinkDir}
    ${GNUWIN32_DIR}/lib
  )

  if(APOGEE_INCLUDE_DIR AND APOGEEU_LIBRARIES AND APOGEEE_LIBRARIES)
    set(APOGEE_FOUND TRUE)
  else (APOGEE_INCLUDE_DIR AND APOGEEU_LIBRARIES AND APOGEEE_LIBRARIES)
    set(APOGEE_FOUND FALSE)
  endif(APOGEE_INCLUDE_DIR AND APOGEEU_LIBRARIES AND APOGEEE_LIBRARIES)

  if (APOGEE_FOUND)
    if (NOT APOGEE_FIND_QUIETLY)
      message(STATUS "Found APOGEE: ${APOGEEU_LIBRARIES}, ${APOGEEE_LIBRARIES}")
    endif (NOT APOGEE_FIND_QUIETLY)
  else (APOGEE_FOUND)
    if (APOGEE_FIND_REQUIRED)
      message(FATAL_ERROR "libapogee not found. Cannot compile Apogee CCD Driver. Please install libapogee and try again. http://APOGEE.sf.net")
    endif (APOGEE_FIND_REQUIRED)
  endif (APOGEE_FOUND) 

 mark_as_advanced(APOGEE_INCLUDE_DIR APOGEEU_LIBRARIES APOGEEE_LIBRARIES)

endif (APOGEE_INCLUDE_DIR AND APOGEEU_LIBRARIES AND APOGEEE_LIBRARIES)
