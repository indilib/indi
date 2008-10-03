# - Try to find SBIG
# Once done this will define
#
#  SBIG_FOUND - system has SBIG
#  SBIG_LIBRARIES - Link these to use SBIG

# Copyright (c) 2006, Jasem Mutlaq <mutlaqja@ikarustech.com>
# Based on FindSBIG by Carsten Niehaus, <cniehaus@gmx.de>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (SBIG_LIBRARIES)

  # in cache already
  set(SBIG_FOUND TRUE)
 message(STATUS "Found SBIG: ${SBIG_LIBRARIES}")

else (SBIG_LIBRARIES)

  find_library(SBIG_LIBRARIES NAMES sbigudrv
    PATHS
    ${_obLinkDir}
    ${GNUWIN32_DIR}/lib
  )

  include(FindPackageHandleStandardArgs)
  FIND_PACKAGE_HANDLE_STANDARD_ARGS(SBIG DEFAULT_MSG SBIG_LIBRARIES )
   
  mark_as_advanced(SBIG_LIBRARIES)

endif (SBIG_LIBRARIES)
