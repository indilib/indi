# - Find PNG
# Find the native PNG includes and library
# This module defines
#  PNG_INCLUDE_DIR, where to find PNGlib.h, etc.
#  PNG_LIBRARIES, the libraries needed to use PNG.
#  PNG_FOUND, If false, do not try to use PNG.
# also defined, but not for general use are
#  PNG_LIBRARY, where to find the PNG library.

FIND_PATH(PNG_INCLUDE_DIR png.h)

SET(PNG_NAMES ${PNG_NAMES} png)
FIND_LIBRARY(PNG_LIBRARY NAMES ${PNG_NAMES} )

# handle the QUIETLY and REQUIRED arguments and set PNG_FOUND to TRUE if 
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(PNG DEFAULT_MSG PNG_LIBRARY PNG_INCLUDE_DIR)

IF(PNG_FOUND)
  SET(PNG_LIBRARIES ${PNG_LIBRARY})
ENDIF(PNG_FOUND)

# Deprecated declarations.
SET (NATIVE_PNG_INCLUDE_PATH ${PNG_INCLUDE_DIR} )
GET_FILENAME_COMPONENT (NATIVE_PNG_LIB_PATH ${PNG_LIBRARY} PATH)

MARK_AS_ADVANCED(PNG_LIBRARY PNG_INCLUDE_DIR )
