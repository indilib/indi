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
BUILD_LINK=$TOP_DIR/build
BUILD_DIR=/dev/shm/indi-build





if [ ! x"$1" = x"" ]; then
    # Take the option given as the new build directory
    BUILD_DIR="$1"
fi

echo ">>> Target build directory: $BUILD_DIR"

case $BUILD_DIR/ in
    /dev/shm/*)
    # Build directory is in RAM
    # ONLY continue if there is at least 1GB of RAM available
    NEED_MEMORY=1000000
    FREE_MEMORY=$(free | awk '$1 == "Mem:" {print $NF}')

    if [ $FREE_MEMORY -lt $NEED_MEMORY ]; then
        echo "*** Not enough memory available in RAM (current "\
             "$FREE_MEMORY, need $NEED_MEMORY)"
        exit 1
    fi
    ;;

    *)
    # Build directory is not in RAM
    ;;
esac





# Create the build directory and a symbolic link for quick access
if [ ! -d $BUILD_DIR ]; then
    mkdir -p $BUILD_DIR
fi

if [ ! -h $BUILD_LINK ]; then
    echo ">>> Creating a symbolic link for quick access:"
    ln -s $BUILD_DIR $BUILD_LINK
    echo ">>> New link: $BUILD_LINK"
elif [ $(file $BUILD_LINK | awk '{print $NF}') == $BUILD_DIR ]; then
    echo ">>> Symbolic link intact"
fi





# Begin the build process, with all cores available
cd $BUILD_DIR
cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Debug $TOP_DIR
make -j
