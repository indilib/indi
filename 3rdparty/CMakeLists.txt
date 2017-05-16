cmake_minimum_required(VERSION 2.6)

PROJECT(libindi-3rdparty CXX C)
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include(GNUInstallDirs)

## libnova.h is in /usr/local/include but it includes other files like <libnova/ln_types.h> while ln_types.h
## is in the same directory on libnova.h. On Linux, this is fine since /usr/include is already included and therefore
## <libnova/ln_types.h> is resolved. But on Mac it has to be explicitly added. Ideal solution would be for libnova
## to include header files without libnova prefix since they reside as the same directory.
if (APPLE)
include_directories( "/usr/local/include")
endif(APPLE)

set(LIBRARIES_FOUND TRUE)

# Define standard set of drivers to build (default linux target)
option(WITH_EQMOD "Install EQMod Driver" On)
option(WITH_NSE "Install Nexstar Evo Driver" On)
option(WITH_SX "Install StarLight Xpress Driver" On)
option(WITH_MAXDOME "Install MaxDomeII Driver" On)
option(WITH_SPECTRACYBER "Install Spectracyber Driver" On)
option(WITH_CLOUDWATCHER "Install AAG Cloud Watcher Driver" On)
option(WITH_MI "Install Moravian Driver" Off)
option(WITH_FLI "Install FLI Driver" On)
option(WITH_SBIG "Install SBIG Driver" On)
option(WITH_APOGEE "Install Apogee Driver" On)
option(WITH_QHY "Install QHY Driver" On)
option(WITH_GPHOTO "Install GPhoto Driver" On)
option(WITH_QSI "Install QSI Driver" On)
option(WITH_DUINO "Install Ariduino Driver" On)
option(WITH_FISHCAMP "Install Fishcamp Driver" On)
option(WITH_GPSD "Install GPSD Driver" On)
option(WITH_GIGE "Install GiGE machine vision Driver" Off)
option(WITH_DSI "Install Meade DSI Driver" On)
option(WITH_ASICAM "Install ZWO Optics ASI Driver" On)
option(WITH_MGEN "Install MGen Autoguider" Off)

# Add/remove cases for OSX
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
set(WITH_MI FALSE)
set(WITH_FLI FALSE)
set(WITH_SBIG FALSE)
set(WITH_APOGEE FALSE)
set(WITH_DUINO FALSE) 
set(WITH_FISHCAMP FALSE) 
set(WITH_GIGE FALSE) 
set(WITH_DSI FALSE) 
endif()

## EQMod
if (WITH_EQMOD)
add_subdirectory(indi-eqmod)
endif(WITH_EQMOD)

## Stalight Xpress
if (WITH_SX)
add_subdirectory(indi-sx)
endif(WITH_SX)

## Maxdome II
if (WITH_MAXDOME)
add_subdirectory(indi-maxdomeii)
endif(WITH_MAXDOME)

## SpectraCyber
if (WITH_SPECTRACYBER)
add_subdirectory(indi-spectracyber)
endif(WITH_SPECTRACYBER)

## Cloud Watcher
if (WITH_CLOUDWATCHER)
add_subdirectory(indi-aagcloudwatcher)
endif (WITH_CLOUDWATCHER)

## Nexstar Evo
if (WITH_NSE)
add_subdirectory(indi-nexstarevo)
endif (WITH_NSE)

## GPhoto
if (WITH_GPHOTO)
add_subdirectory(indi-gphoto)
endif(WITH_GPHOTO)

## QSI
if (WITH_QSI)
find_package(QSI)
if (QSI_FOUND)
add_subdirectory(indi-qsi)
else (QSI_FOUND)
add_subdirectory(libqsi)
SET(LIBRARIES_FOUND FALSE)
endif (QSI_FOUND)
endif (WITH_QSI)

#if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")

## SBIG
if (WITH_SBIG)
find_package(SBIG)
if (SBIG_FOUND)
add_subdirectory(indi-sbig)
else (SBIG_FOUND)
add_subdirectory(libsbig)
SET(LIBRARIES_FOUND FALSE)
endif (SBIG_FOUND)
endif (WITH_SBIG)

## FLI
if (WITH_FLI)
find_package(FLI)
if (FLI_FOUND)
add_subdirectory(indi-fli)
else (FLI_FOUND)
add_subdirectory(libfli)
SET(LIBRARIES_FOUND FALSE)
endif (FLI_FOUND)
endif(WITH_FLI)

## Apogee
if (WITH_APOGEE)
find_package(APOGEE)
if (APOGEE_FOUND)
add_subdirectory(indi-apogee)
else (APOGEE_FOUND)
add_subdirectory(libapogee)
SET(LIBRARIES_FOUND FALSE)
endif (APOGEE_FOUND)
endif(WITH_APOGEE)

## Moravian
if (WITH_MI)
add_subdirectory(indi-mi)
endif (WITH_MI)

## Arduino
if (WITH_DUINO)
add_subdirectory(indi-duino)
endif (WITH_DUINO)

## Fishcamp
if (WITH_FISHCAMP)
find_package(FISHCAMP)
if (FISHCAMP_FOUND)
add_subdirectory(indi-fishcamp)
else (FISHCAMP_FOUND)
add_subdirectory(libfishcamp)
SET(LIBRARIES_FOUND FALSE)
endif(FISHCAMP_FOUND)
endif(WITH_FISHCAMP)

## ASI
if (WITH_ASICAM)
add_subdirectory(asi-common)
add_subdirectory(indi-asi)
endif(WITH_ASICAM)

## DSI
if (WITH_DSI)
add_subdirectory(indi-dsi)
endif(WITH_DSI)

## QHY
if (WITH_QHY)
find_package(QHY)
if (QHY_FOUND)
add_subdirectory(indi-qhy)
else (QHY_FOUND)
add_subdirectory(libqhy)
SET(LIBRARIES_FOUND FALSE)
endif (QHY_FOUND)
endif (WITH_QHY)

## GPSD
if (WITH_GPSD)
add_subdirectory(indi-gpsd)
endif (WITH_GPSD)

## GIGE
if (WITH_GIGE)
add_subdirectory(indi-gige)
endif (WITH_GIGE)

# MGen
if (WITH_MGEN)
add_subdirectory(indi-mgen)
endif (WITH_MGEN)

#endif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")


if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
add_subdirectory(fxload-libusb)
endif()

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

if (WITH_QHY AND NOT QHY_FOUND)
message(STATUS "libqhy was not found and will now be built. Please install libqhy first before running cmake again to install indi-qhy.")
endif (WITH_QHY AND NOT QHY_FOUND)

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

