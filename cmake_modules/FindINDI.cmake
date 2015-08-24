# - Try to find INDI
# Once done this will define
#
#  INDI_FOUND - system has INDI
#  INDI_INCLUDE_DIR - the INDI include directory
#  INDI_LIBRARIES - Link these to use INDI
#  INDI_DRIVER_LIBRARIES - Link to these to build INDI drivers with indibase support
#  INDI_CLIENT_LIBRARIES - Link to these to build INDI clients
#  INDI_DATA_DIR - INDI shared data dir.

# Copyright (c) 2015, Jasem Mutlaq <mutlaqja@ikarustech.com>
# Copyright (c) 2012, Pino Toscano <pino@kde.org>
# Based on FindLibfacile by Carsten Niehaus, <cniehaus@gmx.de>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (INDI_INCLUDE_DIR AND INDI_DATA_DIR AND INDI_LIBRARIES AND INDI_DRIVER_LIBRARIES AND INDI_CLIENT_LIBRARIES)

  # in cache already
  set(INDI_FOUND TRUE)
  message(STATUS "Found INDI: ${INDI_LIBRARIES}")


else (INDI_INCLUDE_DIR AND INDI_DATA_DIR AND INDI_LIBRARIES AND INDI_DRIVER_LIBRARIES AND INDI_CLIENT_LIBRARIES)

  find_package(PkgConfig)

  if (PKG_CONFIG_FOUND)
    if (INDI_FIND_VERSION)
      set(version_string ">=${INDI_FIND_VERSION}")
    endif()
    pkg_check_modules(PC_INDI libindi${version_string})
  else()
    # assume it was found
    set(PC_INDI_FOUND TRUE)
  endif()

  if (PC_INDI_FOUND)
    find_path(INDI_INCLUDE_DIR indidevapi.h
      PATH_SUFFIXES libindi
      HINTS ${PC_INDI_INCLUDE_DIRS}
    )

    find_library(INDI_LIBRARIES NAMES indi
      HINTS ${PC_INDI_LIBRARY_DIRS}
    )

    find_library(INDI_DRIVER_LIBRARIES NAMES indidriver
      HINTS ${PC_INDI_LIBRARY_DIRS}
    )
    
    find_library(INDI_CLIENT_LIBRARIES NAMES indiclient
      HINTS ${PC_INDI_LIBRARY_DIRS}
    )

    find_path(INDI_DATA_DIR drivers.xml
      PATH_SUFFIXES share/indi
    )

    set(INDI_VERSION "${PC_INDI_VERSION}")

  endif()

  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(INDI
                                    REQUIRED_VARS INDI_INCLUDE_DIR INDI_LIBRARIES INDI_DRIVER_LIBRARIES INDI_CLIENT_LIBRARIES
                                    VERSION_VAR INDI_VERSION
  )

  mark_as_advanced(INDI_INCLUDE_DIR INDI_DATA_DIR INDI_LIBRARIES INDI_DRIVER_LIBRARIES INDI_CLIENT_LIBRARIES)

endif (INDI_INCLUDE_DIR AND INDI_DATA_DIR AND INDI_LIBRARIES AND INDI_DRIVER_LIBRARIES AND INDI_CLIENT_LIBRARIES)
