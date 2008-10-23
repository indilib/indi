# - Try to find NOVA
# Once done this will define
#
#  NOVA_FOUND - system has NOVA
#  NOVA_INCLUDE_DIR - the NOVA include directory
#  NOVA_LIBRARIES - Link these to use NOVA

# Copyright (c) 2006, Jasem Mutlaq <mutlaqja@ikarustech.com>
# Based on FindLibfacile by Carsten Niehaus, <cniehaus@gmx.de>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (NOVA_INCLUDE_DIR AND NOVA_LIBRARIES AND NOVA_FUNCTION_COMPILE)

  # in cache already
  set(NOVA_FOUND TRUE)
  message(STATUS "Found libnova: ${NOVA_LIBRARIES}")

else (NOVA_INCLUDE_DIR AND NOVA_LIBRARIES)

  find_path(NOVA_INCLUDE_DIR libnova.h
    PATH_SUFFIXES libnova
    ${_obIncDir}
    ${GNUWIN32_DIR}/include
  )

  find_library(NOVA_LIBRARIES NAMES nova
    PATHS
    ${_obLinkDir}
    ${GNUWIN32_DIR}/lib
  )

 set(CMAKE_REQUIRED_INCLUDES ${NOVA_INCLUDE_DIR})
 set(CMAKE_REQUIRED_LIBRARIES ${NOVA_LIBRARIES})
 check_cxx_source_compiles("#include <libnova.h> 
 int main() { ln_get_date_from_tm(NULL, NULL); return 0; }" NOVA_FUNCTION_COMPILE)

 include(FindPackageHandleStandardArgs)
 FIND_PACKAGE_HANDLE_STANDARD_ARGS(Nova DEFAULT_MSG NOVA_LIBRARIES NOVA_INCLUDE_DIR NOVA_FUNCTION_COMPILE)
 mark_as_advanced(NOVA_INCLUDE_DIR NOVA_LIBRARIES)

endif (NOVA_INCLUDE_DIR AND NOVA_LIBRARIES AND NOVA_FUNCTION_COMPILE)
