#!/bin/bash

set -x -e

FLAGS="-DCMAKE_INSTALL_PREFIX=/usr/local -DFIX_WARNINGS=ON -DCMAKE_BUILD_TYPE=$1"

# The build-libs.sh must be run first for this to work
if [ .${CIRCLE_BRANCH%_*} == '.drv' -a `lsb_release -si` == 'Ubuntu' ] ; then
    DRV="indi-${CIRCLE_BRANCH#drv_}"
    echo "Building $DRV"
    mkdir -p build/$DRV
    pushd build/$DRV
    cmake $FLAGS . ../../3rdparty/$DRV
    make
    popd
else
    echo "Building all 3rd party drivers"
    mkdir -p build/3rdparty
    pushd build/3rdparty
    cmake $FLAGS . ../../3rdparty/
    make
    popd
fi

exit 0
