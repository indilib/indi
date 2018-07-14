# - Try to find ffmpeg libraries (libavcodec, libavdevice, libavformat, libavutil, and libswscale)
# Once done this will define
#
# FFMPEG_FOUND - system has ffmpeg or libav
# FFMPEG_INCLUDE_DIR - the ffmpeg include directory
# FFMPEG_LIBRARIES - Link these to use ffmpeg
# FFMPEG_LIBAVCODEC
# FFMPEG_LIBAVDEVICE
# FFMPEG_LIBAVFORMAT
# FFMPEG_LIBAVUTIL
# FFMPEG_LIBSWSCALE
#
# Copyright (c) 2008 Andreas Schneider <mail@cynapses.org>
# Modified for other libraries by Lasse Kärkkäinen <tronic>
# Modified for Hedgewars by Stepik777
# Modified for INDILIB by rlancaste
#
# Redistribution and use is allowed according to the terms of the New
# BSD license.
#

# required ffmpeg library versions, Requiring at least FFMPEG 3.2.11, Hypatia
set(_avcodec_ver ">=57.64.101" REQUIRED)
set(_avdevice_ver ">=57.1.100" REQUIRED)
set(_avformat_ver ">=57.56.100" REQUIRED)
set(_avutil_ver ">=55.34.100" REQUIRED)
set(_swscale_ver ">=4.2.100" REQUIRED)

 set(FFMPEG_PKGS libavcodec${_avcodec_ver}
                 libavdevice${_avdevice_ver}
                 libavformat${_avformat_ver}
                 libavutil${_avutil_ver}
                 libswscale${_swscale_ver})

if (FFMPEG_LIBRARIES AND FFMPEG_INCLUDE_DIR)
# in cache already
set(FFMPEG_FOUND TRUE)
else (FFMPEG_LIBRARIES AND FFMPEG_INCLUDE_DIR)
# use pkg-config to get the directories and then use these values
# in the FIND_PATH() and FIND_LIBRARY() calls
find_package(PkgConfig)
if (PKG_CONFIG_FOUND)
pkg_check_modules(_FFMPEG_AVCODEC ${FFMPEG_PKGS})
endif (PKG_CONFIG_FOUND)

find_path(FFMPEG_AVCODEC_INCLUDE_DIR
NAMES libavcodec/avcodec.h
PATHS ${_FFMPEG_AVCODEC_INCLUDE_DIRS} /usr/include /usr/local/include /opt/local/include /sw/include
PATH_SUFFIXES ffmpeg libav
)

find_library(FFMPEG_LIBAVCODEC
NAMES avcodec libavcodec
PATHS ${_FFMPEG_AVCODEC_LIBRARY_DIRS} /usr/lib /usr/local/lib /opt/local/lib /sw/lib
)

find_library(FFMPEG_LIBAVDEVICE
NAMES avdevice libavdevice
PATHS ${_FFMPEG_AVDEVICE_LIBRARY_DIRS} /usr/lib /usr/local/lib /opt/local/lib /sw/lib
)

find_library(FFMPEG_LIBAVFORMAT
NAMES avformat libavformat
PATHS ${_FFMPEG_AVFORMAT_LIBRARY_DIRS} /usr/lib /usr/local/lib /opt/local/lib /sw/lib
)

find_library(FFMPEG_LIBAVUTIL
NAMES avutil libavutil
PATHS ${_FFMPEG_AVUTIL_LIBRARY_DIRS} /usr/lib /usr/local/lib /opt/local/lib /sw/lib
)

find_library(FFMPEG_LIBSWSCALE
NAMES swscale libswscale
PATHS ${_FFMPEG_SWSCALE_LIBRARY_DIRS} /usr/lib /usr/local/lib /opt/local/lib /sw/lib
)


if (FFMPEG_LIBAVCODEC AND FFMPEG_LIBAVDEVICE AND FFMPEG_LIBAVFORMAT AND FFMPEG_LIBAVUTIL AND FFMPEG_LIBSWSCALE)
set(FFMPEG_FOUND TRUE)
endif()

if (FFMPEG_FOUND)
set(FFMPEG_INCLUDE_DIR ${FFMPEG_AVCODEC_INCLUDE_DIR})

set(FFMPEG_LIBRARIES
${FFMPEG_LIBAVCODEC}
${FFMPEG_LIBAVDEVICE}
${FFMPEG_LIBAVFORMAT}
${FFMPEG_LIBAVUTIL}
${FFMPEG_LIBSWSCALE}
)

endif (FFMPEG_FOUND)

if (FFMPEG_FOUND)
if (NOT FFMPEG_FIND_QUIETLY)
message(STATUS "Found FFMPEG: ${FFMPEG_LIBRARIES}, ${FFMPEG_INCLUDE_DIR}")
endif (NOT FFMPEG_FIND_QUIETLY)
else (FFMPEG_FOUND)
if (FFMPEG_FIND_REQUIRED)
message(FATAL_ERROR "Could not find recent enough libavcodec, libavdevice, libavformat, libavutil, or libswscale")
endif (FFMPEG_FIND_REQUIRED)
endif (FFMPEG_FOUND)

endif (FFMPEG_LIBRARIES AND FFMPEG_INCLUDE_DIR)


