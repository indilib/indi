# - Try to find TIFFXX Library
# Once done this will define
#
#  TIFXX_LIBRARY - Link these to use TIFFXX

# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (TIFFXX_LIBRARY)

  # in cache already
  set(TIFFXX_FOUND TRUE)
  message(STATUS "Found libtiffxx: ${TIFFXX_LIBRARY}")

else (TIFFXX_LIBRARY)

  find_library(TIFFXX_LIBRARY NAMES tiffxx
    PATHS
    ${_obLinkDir}
    ${GNUWIN32_DIR}/lib
  )

  if(TIFFXX_LIBRARY)
    set(TIFFXX_FOUND TRUE)
  else (TIFFXX_LIBRARY)
    set(TIFFXX_FOUND FALSE)
  endif(TIFFXX_LIBRARY)

  if (TIFFXX_FOUND)
    if (NOT TIFFXX_FIND_QUIETLY)
      message(STATUS "Found tiffxx: ${TIFFXX_LIBRARY}")
    endif (NOT TIFFXX_FIND_QUIETLY)
  else (TIFFXX_FOUND)
    if (TIFFXX_FIND_REQUIRED)
      message(FATAL_ERROR "tiffxx is not found. Please install it first.")
    endif (TIFFXX_FIND_REQUIRED)
  endif (TIFFXX_FOUND)

  mark_as_advanced(TIFFXX_LIBRARY)
  
endif (TIFFXX_LIBRARY)
