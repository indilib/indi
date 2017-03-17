#!/bin/bash

set -x -e

echo ${TRAVIS_OS_NAME}

if [ -z $BUILD_THIRD_PARTY ]; then
    exit 0
fi

# The build-libs.sh must be run first for this to work
if [ .${TRAVIS_BRANCH%_*} == '.drv' ] ; then 
    if [ ${TRAVIS_OS_NAME} == 'osx' ] ; then
        echo "Cannot build one driver on OSX"
        exit 0
    fi
    DRV="indi-${TRAVIS_BRANCH#drv_}"
    echo "Building $DRV"
    mkdir -p build/$DRV
    pushd build/$DRV
    cmake -DCMAKE_INSTALL_PREFIX=/usr/local/ . ../../3rdparty/$DRV
    make
    popd
else
    echo "Building all 3rd party drivers"
    mkdir -p build/3rdparty
    pushd build/3rdparty
    cmake -DCMAKE_INSTALL_PREFIX=/usr/local/ . ../../3rdparty/
    make
    popd
fi
exit 0

