# - Find udev
# Find the native udev includes and library
#
# UDEV_INCLUDE_DIRS - where to find libudev.h
# UDEV_LIBRARIES    - List of libraries when using udev.
# UDEV_FOUND        - True if udev found.

# Look for the installation of this package
pkg_check_modules(PC_UDEV udev)

set(UDEV_DEFINITIONS ${PC_UDEV_CFLAGS_OTHER})

find_path(UDEV_INCLUDE_DIR libudev.h
          HINTS ${PC_UDEV_INCLUDEDIR} ${PC_UDEV_INCLUDE_DIRS}
          PATH_SUFFIXES libudev
         )

find_library(UDEV_LIBRARY NAMES udev
             HINTS ${PC_UDEV_LIBDIR} ${PC_UDEV_LIBRARY_DIRS}
            )

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(UDEV DEFAULT_MSG
                                  UDEV_LIBRARY UDEV_INCLUDE_DIR)

if(UDEV_FOUND)
  set(UDEV_LIBRARIES ${UDEV_LIBRARY})
  set(UDEV_INCLUDE_DIRS ${UDEV_INCLUDE_DIR})
  add_library(UDEV::udev UNKNOWN IMPORTED)
  set_target_properties(UDEV::udev PROPERTIES
    IMPORTED_LOCATION "${UDEV_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${UDEV_INCLUDE_DIR}"
  )
endif()

mark_as_advanced(UDEV_INCLUDE_DIR UDEV_LIBRARY)
