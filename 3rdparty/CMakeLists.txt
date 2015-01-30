cmake_minimum_required(VERSION 2.6)

PROJECT(libindi-3rdparty CXX C)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
set(LIBRARIES_FOUND TRUE)

option(WITH_EQMOD "Install EQMod Driver" On)
option(WITH_SX "Install StarLight Xpress Driver" On)
option(WITH_MAXDOME "Install MaxDomeII Driver" On)
option(WITH_SPECTRACYBER "Install Spectracyber Driver" On)
option(WITH_CLOUDWATCHER "Install AAG Cloud Watcher Driver" On)

if (WITH_EQMOD)
add_subdirectory(indi-eqmod)
endif(WITH_EQMOD)

if (WITH_SX)
add_subdirectory(indi-sx)
endif(WITH_SX)

#if (WITH_QHY)
#add_subdirectory(indi-qhy)
#endif(WITH_QHY)

if (WITH_MAXDOME)
add_subdirectory(indi-maxdomeii)
endif(WITH_MAXDOME)

if (WITH_SPECTRACYBER)
add_subdirectory(indi-spectracyber)
endif(WITH_SPECTRACYBER)

if (WITH_CLOUDWATCHER)
add_subdirectory(indi-aagcloudwatcher)
endif (WITH_CLOUDWATCHER)


if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
option(WITH_QSI "Install QSI Driver" On)
option(WITH_SBIG "Install SBIG Driver" On)
option(WITH_FLI "Install FLI Driver" On)
option(WITH_DUINO "Install Ariduino Driver" On)
option(WITH_GPHOTO "Install GPhoto Driver" On)
option(WITH_APOGEE "Install Apogee Driver" On)
option(WITH_FISHCAMP "Install Fishcamp Driver" On)
option(WITH_ASICAM "Install ZWO Optics ASI Driver" On)
option(WITH_QHY "Install QHY Driver" Off)


if (WITH_QSI)
find_package(QSI)
if (QSI_FOUND)
add_subdirectory(indi-qsi)
else (QSI_FOUND)
add_subdirectory(libqsi)
SET(LIBRARIES_FOUND FALSE)
endif (QSI_FOUND)
endif (WITH_QSI)

if (WITH_SBIG)
find_package(SBIG)
if (SBIG_FOUND)
add_subdirectory(indi-sbig)
else (SBIG_FOUND)
add_subdirectory(sbig)
SET(LIBRARIES_FOUND FALSE)
endif (SBIG_FOUND)
endif (WITH_SBIG)

if (WITH_FLI)
find_package(FLI)
if (FLI_FOUND)
add_subdirectory(indi-fli)
else (FLI_FOUND)
add_subdirectory(libfli)
SET(LIBRARIES_FOUND FALSE)
endif (FLI_FOUND)
endif(WITH_FLI)

if (WITH_ARDUINO)
add_subdirectory(indi-duino)
endif (WITH_ARDUINO)

if (WITH_APOGEE)
find_package(APOGEE)
if (APOGEE_FOUND)
add_subdirectory(indi-apogee)
else (APOGEE_FOUND)
add_subdirectory(libapogee)
SET(LIBRARIES_FOUND FALSE)
endif (APOGEE_FOUND)
endif(WITH_APOGEE)

if (WITH_GPHOTO)
add_subdirectory(indi-gphoto)
endif(WITH_GPHOTO)

if (WITH_FISHCAMP)
find_package(FISHCAMP)
if (FISHCAMP_FOUND)
add_subdirectory(indi-fishcamp)
else (FISHCAMP_FOUND)
add_subdirectory(libfishcamp)
SET(LIBRARIES_FOUND FALSE)
endif(FISHCAMP_FOUND)
endif(WITH_FISHCAMP)

if (WITH_ASICAM)
add_subdirectory(indi-asicam)
endif(WITH_ASICAM)

# Check if libraries are found. If not, we must build them, install them, THEN run CMake again to build and instal the drivers. If all the libraraies are installed, then we build and install the drivers only now.
if (LIBRARIES_FOUND)
message(STATUS "############################################################################")
message(STATUS "######### All libraraies are found. Building all INDI 3rd party drivers now.")
message(STATUS "############################################################################")
else (LIBRARIES_FOUND)
message(STATUS "####################################################################################################################################")
message(STATUS "Not all libraries found, you must build and install all libraries first:")

if (WITH_QSI AND NOT QSI_FOUND)
message(STATUS "libqsi was not found and will now be built. Please install libqsi first before running cmake again to install indi-qsi.")
endif (WITH_QSI AND NOT QSI_FOUND)

if (WITH_SBIG AND NOT SBIG_FOUND)
message(STATUS "libsbigudrv was not found and will now be built. Please install libsbigudrv first before running cmake again to install indi-sbig.")
endif (WITH_SBIG AND NOT SBIG_FOUND)

if (WITH_FLI AND NOT FLI_FOUND)
message(STATUS "libfli was not found and will now be built. Please install libfli first before running cmake again to install indi-fli.")
endif (WITH_FLI AND NOT FLI_FOUND)

if (WITH_APOGEE AND NOT APOGEE_FOUND)
message(STATUS "libapogee was not found and will now be built. Please install libapogee first before running cmake again to install indi-apogee.")
endif (WITH_APOGEE AND NOT APOGEE_FOUND)

if (WITH_FISHCAMP AND NOT FISHCAMP_FOUND)
message(STATUS "libfishcamp was not found and will now be built. Please install libfishcamp first before running cmake again to install indi-fishcamp.")
endif (WITH_FISHCAMP AND NOT FISHCAMP_FOUND)

message(STATUS "####################################################################################################################################")
endif (LIBRARIES_FOUND)

endif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")