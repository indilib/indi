find_path(LibXISF_INCLUDE_DIR
  NAMES libxisf.h
)

find_library(LibXISF_LIBRARY
  NAMES libXISF XISF
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibXISF
  FOUND_VAR LibXISF_FOUND
  REQUIRED_VARS
    LibXISF_LIBRARY
    LibXISF_INCLUDE_DIR
)

if(LibXISF_FOUND AND NOT TARGET LibXISF::LibXISF)
  add_library(LibXISF::LibXISF UNKNOWN IMPORTED)
  set_target_properties(LibXISF::LibXISF PROPERTIES
    IMPORTED_LOCATION "${LibXISF_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${LibXISF_INCLUDE_DIR}"
  )
endif()
