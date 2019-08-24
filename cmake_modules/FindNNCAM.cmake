# - Try to find NNCAM Camera Library
# Once done this will define
#
#  NNCAM_FOUND - system has Levenhuk
#  NNCAM_INCLUDE_DIR - the Levenhuk include directory
#  NNCAM_LIBRARIES - Link these to use Levenhuk

# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (NNCAM_INCLUDE_DIR AND NNCAM_LIBRARIES)

  # in cache already
  set(NNCAM_FOUND TRUE)
  message(STATUS "Found libnncam: ${NNCAM_LIBRARIES}")

else (NNCAM_INCLUDE_DIR AND NNCAM_LIBRARIES)

  find_path(NNCAM_INCLUDE_DIR nncam.h
    PATH_SUFFIXES libnncam
    ${_obIncDir}
    ${GNUWIN32_DIR}/include
  )

  find_library(NNCAM_LIBRARIES NAMES nncam
    PATHS
    ${_obLinkDir}
    ${GNUWIN32_DIR}/lib
  )

  if(NNCAM_INCLUDE_DIR AND NNCAM_LIBRARIES)
    set(NNCAM_FOUND TRUE)
  else (NNCAM_INCLUDE_DIR AND NNCAM_LIBRARIES)
    set(NNCAM_FOUND FALSE)
  endif(NNCAM_INCLUDE_DIR AND NNCAM_LIBRARIES)

  if (NNCAM_FOUND)
    if (NOT NNCAM_FIND_QUIETLY)
      message(STATUS "Found NNCAM: ${NNCAM_LIBRARIES}")
    endif (NOT NNCAM_FIND_QUIETLY)
  else (NNCAM_FOUND)
    if (NNCAM_FIND_REQUIRED)
      message(FATAL_ERROR "NNCAM not found. Please install NNCAM Library http://www.indilib.org")
    endif (NNCAM_FIND_REQUIRED)
  endif (NNCAM_FOUND)

  mark_as_advanced(NNCAM_INCLUDE_DIR NNCAM_LIBRARIES)

endif (NNCAM_INCLUDE_DIR AND NNCAM_LIBRARIES)
