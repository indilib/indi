cmake_minimum_required(VERSION 2.6)

PROJECT(libindi-3rdparty CXX C)

option(WITH_QSI "Install QSI Driver" On)

if (WITH_QSI)
find_package(QSI)
if (QSI_FOUND)
add_subdirectory(indi-qsi)
else (QSI_FOUND)
add_subdirectory(libqsi)
message(STATUS "libqsi was not found and will now be built. Please install libqsi first before running cmake again to install indi-qsi")
endif (QSI_FOUND)
endif (WITH_QSI)

