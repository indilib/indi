# - Try to find Finger Lakes Instruments Library
# Once done this will define
#
#  FLI_FOUND - system has FLI
#  FLI_INCLUDE_DIR - the FLI include directory
#  FLI_LIBRARIES - Link these to use FLI

# Copyright (c) 2008, Jasem Mutlaq <mutlaqja@ikarustech.com>
# Based on FindLibfacile by Carsten Niehaus, <cniehaus@gmx.de>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (FLI_INCLUDE_DIR AND FLI_LIBRARIES)

  # in cache already
  set(FLI_FOUND TRUE)
  message(STATUS "Found libfli: ${FLI_LIBRARIES}")

else (FLI_INCLUDE_DIR AND FLI_LIBRARIES)

  find_path(FLI_INCLUDE_DIR libfli.h
    PATH_SUFFIXES fli
    ${_obIncDir}
    ${GNUWIN32_DIR}/include
  )

  find_library(FLI_LIBRARIES NAMES fli
    PATHS
    ${_obLinkDir}
    ${GNUWIN32_DIR}/lib
  )

 set(CMAKE_REQUIRED_INCLUDES ${FLI_INCLUDE_DIR})
 set(CMAKE_REQUIRED_LIBRARIES ${FLI_LIBRARIES})
 
 include(FindPackageHandleStandardArgs)
 FIND_PACKAGE_HANDLE_STANDARD_ARGS(FLI DEFAULT_MSG FLI_LIBRARIES FLI_INCLUDE_DIR)
 mark_as_advanced(FLI_INCLUDE_DIR FLI_LIBRARIES)

endif (FLI_INCLUDE_DIR AND FLI_LIBRARIES)
