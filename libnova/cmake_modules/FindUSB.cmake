# - Try to find LIBUSB
# Once done this will define
#
#  LIBUSB_FOUND - system has LIBUSB
#  LIBUSB_INCLUDE_DIR - the LIBUSB include directory
#  LIBUSB_LIBRARIES - Link these to use LIBUSB

# Copyright (c) 2006, Jasem Mutlaq <mutlaqja@ikarustech.com>
# Based on FindLibfacile by Carsten Niehaus, <cniehaus@gmx.de>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (LIBUSB_INCLUDE_DIR AND LIBUSB_LIBRARIES)

  # in cache already
  set(LIBUSB_FOUND TRUE)
  message(STATUS "Found LIBUSB: ${LIBUSB_LIBRARIES}")


else (LIBUSB_INCLUDE_DIR AND LIBUSB_LIBRARIES)

  find_path(LIBUSB_INCLUDE_DIR usb.h
    ${_obIncDir}
    ${GNUWIN32_DIR}/include
  )

  find_library(LIBUSB_LIBRARIES NAMES usb
    PATHS
    ${_obLinkDir}
    ${GNUWIN32_DIR}/lib
  )

  if(LIBUSB_INCLUDE_DIR AND LIBUSB_LIBRARIES)
    set(LIBUSB_FOUND TRUE)
  else (LIBUSB_INCLUDE_DIR AND LIBUSB_LIBRARIES)
    set(LIBUSB_FOUND FALSE)
  endif(LIBUSB_INCLUDE_DIR AND LIBUSB_LIBRARIES)


  if (LIBUSB_FOUND)
    if (NOT USB_FIND_QUIETLY)
      message(STATUS "Found LIBUSB: ${LIBUSB_LIBRARIES}")
    endif (NOT USB_FIND_QUIETLY)
  else (LIBUSB_FOUND)
    if (USB_FIND_REQUIRED)
      message(FATAL_ERROR "LIBUSB not found. Please install libusb-devel and try again.")
    endif (USB_FIND_REQUIRED)
  endif (LIBUSB_FOUND)
   
mark_as_advanced(LIBUSB_INCLUDE_DIR LIBUSB_LIBRARIES)

endif (LIBUSB_INCLUDE_DIR AND LIBUSB_LIBRARIES)
