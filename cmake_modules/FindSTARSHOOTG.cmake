# - Try to find Starshoot Camera Library
# Once done this will define
#
#  STARSHOOTG_FOUND - system has Starshoot
#  STARSHOOTG_INCLUDE_DIR - the Starshoot include directory
#  STARSHOOTG_LIBRARIES - Link these to use Starshoot

# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (STARSHOOTG_INCLUDE_DIR AND STARSHOOTG_LIBRARIES)

  # in cache already
  set(STARSHOOTG_FOUND TRUE)
  message(STATUS "Found libstarshootg: ${STARSHOOTG_LIBRARIES}")

else (STARSHOOTG_INCLUDE_DIR AND STARSHOOTG_LIBRARIES)

  find_path(STARSHOOTG_INCLUDE_DIR starshootg.h
    PATH_SUFFIXES libstarshootg
    ${_obIncDir}
    ${GNUWIN32_DIR}/include
  )

  find_library(STARSHOOTG_LIBRARIES NAMES starshootg
    PATHS
    ${_obLinkDir}
    ${GNUWIN32_DIR}/lib
  )

  if(STARSHOOTG_INCLUDE_DIR AND STARSHOOTG_LIBRARIES)
    set(STARSHOOTG_FOUND TRUE)
  else (STARSHOOTG_INCLUDE_DIR AND STARSHOOTG_LIBRARIES)
    set(STARSHOOTG_FOUND FALSE)
  endif(STARSHOOTG_INCLUDE_DIR AND STARSHOOTG_LIBRARIES)

  if (STARSHOOTG_FOUND)
    if (NOT STARSHOOTG_FIND_QUIETLY)
      message(STATUS "Found StarshootG: ${STARSHOOTG_LIBRARIES}")
    endif (NOT STARSHOOTG_FIND_QUIETLY)
  else (STARSHOOTG_FOUND)
    if (STARSHOOTG_FIND_REQUIRED)
      message(FATAL_ERROR "StarshootG not found. Please install StarshootG Library http://www.indilib.org")
    endif (STARSHOOTG_FIND_REQUIRED)
  endif (STARSHOOTG_FOUND)

  mark_as_advanced(STARSHOOTG_INCLUDE_DIR STARSHOOTG_LIBRARIES)

endif (STARSHOOTG_INCLUDE_DIR AND STARSHOOTG_LIBRARIES)
