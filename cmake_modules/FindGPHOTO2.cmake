# - Try to find libgphoto2
# Once done this will define
#
#  LIBGPHOTO2_FOUND - system has libgphoto2
#  LIBGPHOTO2_INCLUDE_DIR - the libgphoto2 include directory
#  LIBGPHOTO2_LIBRARIES - Link these to use libghoto2

# Copyright (c) 2009, Geoffrey Hausheer
# Based on FindINDI by Jasem Mutlaq <mutlaqja@ikarustech.com>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (LIBGPHOTO2_INCLUDE_DIR AND LIBGPHOTO2_LIBRARIES)

  # in cache already
  set(LIBGPHOTO2_FOUND TRUE)
  message(STATUS "Found libgphoto2: ${LIBGPHOTO2_LIBRARIES}")


else (LIBGPHOTO2_INCLUDE_DIR AND LIBGPHOTO2_LIBRARIES)

  find_path(LIBGPHOTO2_INCLUDE_DIR gphoto2.h
    PATH_SUFFIXES gphoto2
    ${_obIncDir}
    ${GNUWIN32_DIR}/include
  )

  find_library(LIBGPHOTO2_LIBRARIES NAMES gphoto2
    PATHS
    ${_obLinkDir}
    ${GNUWIN32_DIR}/lib
  )

  if(LIBGPHOTO2_INCLUDE_DIR AND LIBGPHOTO2_LIBRARIES)
    set(LIBGPHOTO2_FOUND TRUE)
  else (LIBGPHOTO2_INCLUDE_DIR AND LIBGPHOTO2_LIBRARIES)
    set(LIBGPHOTO2_FOUND FALSE)
  endif(LIBGPHOTO2_INCLUDE_DIR AND LIBGPHOTO2_LIBRARIES)


  if (LIBGPHOTO2_FOUND)
    if (NOT LIBGPHOTO2_FIND_QUIETLY)
      message(STATUS "Found libgphoto2: ${LIBGPHOTO2_LIBRARIES}")
    endif (NOT LIBGPHOTO2_FIND_QUIETLY)
  else (LIBGPHOTO2_FOUND)
    if (LIBGPHOTO2_FIND_REQUIRED)
      message(FATAL_ERROR "libgphoto2 not found.")
    endif (LIBGPHOTO2_FIND_REQUIRED)
  endif (LIBGPHOTO2_FOUND)

  mark_as_advanced(LIBGPHOTO2_INCLUDE_DIR LIBGPHOTO2_LIBRARIES)

endif (LIBGPHOTO2_INCLUDE_DIR AND LIBGPHOTO2_LIBRARIES)
