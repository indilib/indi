# - Try to find Toupcam Camera Library
# Once done this will define
#
#  TOUPCAM_FOUND - system has Toupcam
#  TOUPCAM_INCLUDE_DIR - the Toupcam include directory
#  TOUPCAM_LIBRARIES - Link these to use Toupcam

# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (TOUPCAM_INCLUDE_DIR AND TOUPCAM_LIBRARIES)

  # in cache already
  set(TOUPCAM_FOUND TRUE)
  message(STATUS "Found libsbig: ${TOUPCAM_LIBRARIES}")

else (TOUPCAM_INCLUDE_DIR AND TOUPCAM_LIBRARIES)

  find_path(TOUPCAM_INCLUDE_DIR toupcam.h
    PATH_SUFFIXES libtoupcam
    ${_obIncDir}
    ${GNUWIN32_DIR}/include
  )

  find_library(TOUPCAM_LIBRARIES NAMES toupcam
    PATHS
    ${_obLinkDir}
    ${GNUWIN32_DIR}/lib
  )

  if(TOUPCAM_INCLUDE_DIR AND TOUPCAM_LIBRARIES)
    set(TOUPCAM_FOUND TRUE)
  else (TOUPCAM_INCLUDE_DIR AND TOUPCAM_LIBRARIES)
    set(TOUPCAM_FOUND FALSE)
  endif(TOUPCAM_INCLUDE_DIR AND TOUPCAM_LIBRARIES)


  if (TOUPCAM_FOUND)
    if (NOT TOUPCAM_FIND_QUIETLY)
      message(STATUS "Found Toupcam: ${TOUPCAM_LIBRARIES}")
    endif (NOT TOUPCAM_FIND_QUIETLY)
  else (TOUPCAM_FOUND)
    if (TOUPCAM_FIND_REQUIRED)
      message(FATAL_ERROR "Toupcam not found. Please install Toupcam Library http://www.indilib.org")
    endif (TOUPCAM_FIND_REQUIRED)
  endif (TOUPCAM_FOUND)

  mark_as_advanced(TOUPCAM_INCLUDE_DIR TOUPCAM_LIBRARIES)
  
endif (TOUPCAM_INCLUDE_DIR AND TOUPCAM_LIBRARIES)
