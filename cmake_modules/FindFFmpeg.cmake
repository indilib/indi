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
set(_avcodec_ver ">=57.64.101")
set(_avdevice_ver ">=57.1.100")
set(_avformat_ver ">=57.56.100")
set(_avutil_ver ">=55.34.100")
set(_swscale_ver ">=4.2.100")

if (FFMPEG_LIBRARIES AND FFMPEG_INCLUDE_DIR)

# in cache already
set(FFMPEG_FOUND TRUE)

else (FFMPEG_LIBRARIES AND FFMPEG_INCLUDE_DIR)
# use pkg-config to get the directories and then use these values
# in the FIND_PATH() and FIND_LIBRARY() calls

find_package(PkgConfig)
if (PKG_CONFIG_FOUND)
pkg_check_modules(AVCODEC libavcodec${_avcodec_ver})
pkg_check_modules(AVDEVICE libavdevice${_avdevice_ver})
pkg_check_modules(AVFORMAT libavformat${_avformat_ver})
pkg_check_modules(AVUTIL libavutil${_avutil_ver})
pkg_check_modules(SWSCALE libswscale${_swscale_ver})

endif (PKG_CONFIG_FOUND)

find_path(FFMPEG_AVCODEC_INCLUDE_DIR
NAMES libavcodec/avcodec.h
PATHS ${AVCODEC_INCLUDE_DIRS} /usr/include /usr/local/include /opt/local/include /sw/include
PATH_SUFFIXES ffmpeg libav
)

find_library(FFMPEG_LIBAVCODEC
NAMES avcodec libavcodec
PATHS ${AVCODEC_LIBRARY_DIRS} /usr/lib /usr/local/lib /opt/local/lib /sw/lib
)

find_library(FFMPEG_LIBAVDEVICE
NAMES avdevice libavdevice
PATHS ${AVDEVICE_LIBRARY_DIRS} /usr/lib /usr/local/lib /opt/local/lib /sw/lib
)

find_library(FFMPEG_LIBAVFORMAT
NAMES avformat libavformat
PATHS ${AVFORMAT_LIBRARY_DIRS} /usr/lib /usr/local/lib /opt/local/lib /sw/lib
)

find_library(FFMPEG_LIBAVUTIL
NAMES avutil libavutil
PATHS ${AVUTIL_LIBRARY_DIRS} /usr/lib /usr/local/lib /opt/local/lib /sw/lib
)

find_library(FFMPEG_LIBSWSCALE
NAMES swscale libswscale
PATHS ${SWSCALE_LIBRARY_DIRS} /usr/lib /usr/local/lib /opt/local/lib /sw/lib
)

#Only set FFMPEG to found if all the libraries are found in the right versions.
if(AVCODEC_VERSION AND
                 AVDEVICE_VERSION AND
                 AVFORMAT_VERSION AND
                 AVUTIL_VERSION AND
                 SWSCALE_VERSION AND
				 FFMPEG_LIBAVCODEC AND
				 FFMPEG_LIBAVDEVICE AND
				 FFMPEG_LIBAVFORMAT AND
				 FFMPEG_LIBAVUTIL AND
				 FFMPEG_LIBSWSCALE)
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
message(STATUS "Could not find up to date FFMPEG, could be due to libavcodec, libavdevice, libavformat, libavutil, or libswscale version")
if (FFMPEG_FIND_REQUIRED)
message(FATAL_ERROR "Error: FFMPEG is required by this package!")
endif (FFMPEG_FIND_REQUIRED)
endif (FFMPEG_FOUND)

endif (FFMPEG_LIBRARIES AND FFMPEG_INCLUDE_DIR)


