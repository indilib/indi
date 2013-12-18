cmake_minimum_required(VERSION 2.6)

PROJECT(libindi-3rdparty CXX C)

option(WITH_EQMOD "Install EQMod Driver" On)
option(WITH_SX "Install StarLight Xpress Driver" On)
option(WITH_QHY "Install QHY Driver" On)
option(WITH_MAXDOME "Install MaxDomeII Driver" On)
option(WITH_SPECTRACYBER "Install Spectracyber Driver" On)

if (WITH_EQMOD)
add_subdirectory(indi-eqmod)
endif(WITH_EQMOD)

if (WITH_SX)
add_subdirectory(indi-sx)
endif(WITH_SX)

if (WITH_QHY)
add_subdirectory(indi-qhy)
endif(WITH_QHY)

if (WITH_MAXDOME)
add_subdirectory(indi-maxdomeii)
endif(WITH_MAXDOME)

if (WITH_SPECTRACYBER)
add_subdirectory(indi-spectracyber)
endif(WITH_SPECTRACYBER)

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
option(WITH_QSI "Install QSI Driver" On)
option(WITH_SBIG "Install SBIG Driver" On)
option(WITH_FLI "Install FLI Driver" On)
option(WITH_DUINO "Install Ariduino Driver" On)
option(WITH_GPHOTO "Install GPhoto Driver" On)
option(WITH_APOGEE "Install Apogee Driver" On)
option(WITH_FISHCAMP "Install Fishcamp Driver" On)

if (WITH_QSI)
add_subdirectory(libqsi)
add_subdirectory(indi-qsi)
endif (WITH_QSI)

if (WITH_SBIG)
add_subdirectory(sbig)
add_subdirectory(indi-sbig)
endif (WITH_SBIG)

if (WITH_FLI)
add_subdirectory(libfli)
add_subdirectory(indi-fli)
endif(WITH_FLI)

if (WITH_ARDUINO)
add_subdirectory(indi-duino)
endif (WITH_ARDUINO)

if (WITH_APOGEE)
add_subdirectory(libapogee)
add_subdirectory(indi-apogee)
endif(WITH_APOGEE)

if (WITH_GPHOTO)
add_subdirectory(indi-gphoto)
endif(WITH_GPHOTO)

if (WITH_FISHCAMP)
add_subdirectory(libfishcamp)
add_subdirectory(indi-fishcamp)
endif(WITH_FISHCAMP)

endif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
