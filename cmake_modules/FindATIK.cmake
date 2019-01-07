# - Try to find Atik Camera Library
# Once done this will define
#
#  ATIK_FOUND - system has ATIK
#  ATIK_INCLUDE_DIR - the ATIK include directory
#  ATIK_LIBRARIES - Link these to use ATIK

# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (ATIK_INCLUDE_DIR AND ATIK_LIBRARIES)

  # in cache already
  set(ATIK_FOUND TRUE)
  message(STATUS "Found libatik: ${ATIK_LIBRARIES}")

else (ATIK_INCLUDE_DIR AND ATIK_LIBRARIES)

  find_path(ATIK_INCLUDE_DIR AtikCameras.h
    PATH_SUFFIXES libatik
    ${_obIncDir}
    ${GNUWIN32_DIR}/include
  )

  find_library(ATIK_LIBRARIES NAMES atikcameras
    PATHS
    ${_obLinkDir}
    ${GNUWIN32_DIR}/lib
  )

  if(ATIK_INCLUDE_DIR AND ATIK_LIBRARIES)
    set(ATIK_FOUND TRUE)
  else (ATIK_INCLUDE_DIR AND ATIK_LIBRARIES)
    set(ATIK_FOUND FALSE)
  endif(ATIK_INCLUDE_DIR AND ATIK_LIBRARIES)

  if (ATIK_FOUND)
    if (NOT ATIK_FIND_QUIETLY)
      message(STATUS "Found Atik Library: ${ATIK_LIBRARIES}")
    endif (NOT ATIK_FIND_QUIETLY)
  else (ATIK_FOUND)
    if (ATIK_FIND_REQUIRED)
      message(FATAL_ERROR "Atik Library not found. Please install Atik Library http://www.indilib.org")
    endif (ATIK_FIND_REQUIRED)
  endif (ATIK_FOUND)

  mark_as_advanced(ATIK_INCLUDE_DIR ATIK_LIBRARIES)
  
endif (ATIK_INCLUDE_DIR AND ATIK_LIBRARIES)
