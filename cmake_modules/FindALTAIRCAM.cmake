# - Try to find Altair Camera Library
# Once done this will define
#
#  ALTAIRCAM_FOUND - system has Altair
#  ALTAIRCAM_INCLUDE_DIR - the Altair include directory
#  ALTAIRCAM_LIBRARIES - Link these to use Altair

# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (ALTAIRCAM_INCLUDE_DIR AND ALTAIRCAM_LIBRARIES)

  # in cache already
  set(ALTAIRCAM_FOUND TRUE)
  message(STATUS "Found libaltaircam: ${ALTAIRCAM_LIBRARIES}")

else (ALTAIRCAM_INCLUDE_DIR AND ALTAIRCAM_LIBRARIES)

  find_path(ALTAIRCAM_INCLUDE_DIR altaircam.h
    PATH_SUFFIXES libaltaircam
    ${_obIncDir}
    ${GNUWIN32_DIR}/include
  )

  find_library(ALTAIRCAM_LIBRARIES NAMES altaircam
    PATHS
    ${_obLinkDir}
    ${GNUWIN32_DIR}/lib
  )

  if(ALTAIRCAM_INCLUDE_DIR AND ALTAIRCAM_LIBRARIES)
    set(ALTAIRCAM_FOUND TRUE)
  else (ALTAIRCAM_INCLUDE_DIR AND ALTAIRCAM_LIBRARIES)
    set(ALTAIRCAM_FOUND FALSE)
  endif(ALTAIRCAM_INCLUDE_DIR AND ALTAIRCAM_LIBRARIES)

  if (ALTAIRCAM_FOUND)
    if (NOT ALTAIRCAM_FIND_QUIETLY)
      message(STATUS "Found Altaircam: ${ALTAIRCAM_LIBRARIES}")
    endif (NOT ALTAIRCAM_FIND_QUIETLY)
  else (ALTAIRCAM_FOUND)
    if (ALTAIRCAM_FIND_REQUIRED)
      message(FATAL_ERROR "Altaircam not found. Please install Altaircam Library http://www.indilib.org")
    endif (ALTAIRCAM_FIND_REQUIRED)
  endif (ALTAIRCAM_FOUND)

  mark_as_advanced(ALTAIRCAM_INCLUDE_DIR ALTAIRCAM_LIBRARIES)
  
endif (ALTAIRCAM_INCLUDE_DIR AND ALTAIRCAM_LIBRARIES)
