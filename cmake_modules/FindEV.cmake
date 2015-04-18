# - Try to find libev
# Once done this will define
#
#  EV_FOUND - system has EV
#  EV_INCLUDE_DIR - the EV include directory
#  EV_LIBRARIES - Link these to use EV

# Copyright (c) 2008, Jasem Mutlaq <mutlaqja@ikarustech.com>
# Based on FindLibfacile by Carsten Niehaus, <cniehaus@gmx.de>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (EV_INCLUDE_DIR AND EV_LIBRARIES)

  # in cache already
  set(EV_FOUND TRUE)
  message(STATUS "Found libev: ${EV_LIBRARIES}")

else (EV_INCLUDE_DIR AND EV_LIBRARIES)

  find_path(EV_INCLUDE_DIR ev++.h
    ${_obIncDir}
    ${GNUWIN32_DIR}/include
  )

  find_library(EV_LIBRARIES NAMES ev
    PATHS
    ${_obLinkDir}
    ${GNUWIN32_DIR}/lib
  )

  if(EV_INCLUDE_DIR AND EV_LIBRARIES)
    set(EV_FOUND TRUE)
  else (EV_INCLUDE_DIR AND EV_LIBRARIES)
    set(EV_FOUND FALSE)
  endif(EV_INCLUDE_DIR AND EV_LIBRARIES)


  if (EV_FOUND)
    if (NOT EV_FIND_QUIETLY)
      message(STATUS "Found libev: ${EV_LIBRARIES}")
    endif (NOT EV_FIND_QUIETLY)
  else (EV_FOUND)
    if (EV_FIND_REQUIRED)
      message(FATAL_ERROR "libev not found. Please install libev-dev.")
    endif (EV_FIND_REQUIRED)
  endif (EV_FOUND)

  mark_as_advanced(EV_INCLUDE_DIR EV_LIBRARIES)
  
endif (EV_INCLUDE_DIR AND EV_LIBRARIES)
