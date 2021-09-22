#!/bin/bash

# Developer Build Script
#
# Copyright (C) 2021 Pedram Ashofte Ardakani <pedramardakani@pm.me>
#
# This script is free software you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as
# published by the Free Software Foundation either version 2.1 of the
# License, or (at your option) any later version.
#
# This script is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110
# - 1301 USA





# This script is meant to build the program in RAM to prevent wear and
# tear stress on HDD/SDD. RAM is a volatile memory that handles such
# needs gracefully.

TOP_DIR=$PWD
NEED_MEMORY=524288
BUILD_LINK=$TOP_DIR/build
BUILD_DIR=/dev/shm/indi-build





# A useful short tutorial on getopt:
# https://wiki.bash-hackers.org/howto/getopts_tutorial
while getopts ":o:j:f" opt; do
  case $opt in
    o)
        BUILD_DIR="$OPTARG"
        ;;
    j)
        NUMBER_OF_JOBS=$OPTARG
        ;;
    f)
        NEED_MEMORY=""
        ;;
    \?)
        echo "Invalid option -$OPTARG" >&2
        exit 1
        ;;
    :)
        echo "Option -$OPTARG requires an argument." >&2
        exit 1
        ;;
  esac

  # Check if another option is mistakenly passed as an argument
  case $OPTARG in
  -*)
        echo "Option '-$opt' requires an argument."
        exit 1
        ;;
  esac
done





# Use maximum cores available in GNU/Linux and macOS by default
if [ -z $NUMBER_OF_JOBS ]; then
    OS_TYPE=$(uname -s)
    case $OS_TYPE in
    Linux*)
        NUMBER_OF_JOBS=$(nproc)
        ;;
    Darwin*)
        NUMBER_OF_JOBS=$(sysctl -n hw.ncpu)
        ;;
    *)
        NUMBER_OF_JOBS=1
        ;;
    esac
fi
echo ">>> Build using $NUMBER_OF_JOBS core(s), " \
     "i.e. \$ make -j$NUMBER_OF_JOBS"





# Create the build directory if it is not available already
if [ ! -d $BUILD_DIR ]; then
    mkdir -p $BUILD_DIR >&2

    # Exit in case of receiving errors from mkdir
    if [ ! $? -eq 0 ]; then
        echo "*** Please fix the issue with build directory '$BUILD_DIR' and try again."
        echo "*** OR create the directory yourself and re-run the script."
        exit 1
    fi
fi

echo ">>> Target build directory: $BUILD_DIR"





# Check for available free space in the build directory
FREE_MEMORY=$(df $BUILD_DIR | awk 'END{print $4}')

if [ -z $NEED_MEMORY ]; then
    echo ">>> Skipping free memory check. Available: $FREE_MEMORY"
elif [ $FREE_MEMORY -lt $NEED_MEMORY ]; then
    echo "*** Not enough memory available in '$BUILD_DIR'"
    echo "*** (current $FREE_MEMORY, need $NEED_MEMORY)"
    exit 1
fi





# Create the symbolic link
if [ ! -h $BUILD_LINK ]; then
    echo ">>> Creating a symbolic link for quick access:"
    ln -s $BUILD_DIR $BUILD_LINK
    echo ">>> New link: $BUILD_LINK"
elif [ $(file $BUILD_LINK | awk '{print $NF}') == $BUILD_DIR ]; then
    echo ">>> Symbolic link intact"
fi





# Begin the build process
cd $BUILD_DIR
cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Debug $TOP_DIR
make -j$NUMBER_OF_JOBS
