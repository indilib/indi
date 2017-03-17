#!/bin/bash

set -x -e

echo ${TRAVIS_OS_NAME}

# The build-libs.sh must be run first for this to work
if [ ${TRAVIS_BRANCH} == 'master' ] ; then
    if [ ! -z $BUILD_THIRD_PARTY ]; then
      echo "==> BUILD_THIRD_PARTY activated"
      mkdir -p build/3rdparty
      pushd build/3rdparty
      cmake -DCMAKE_INSTALL_PREFIX=/usr/local/ . ../../3rdparty/
      make
      popd
    else
      echo "==> BUILD_THIRD_PARTY not specified"
    fi
else
    if [ .${TRAVIS_BRANCH%_*} == '.drv' ] ; then 
        DRV="indi-${TRAVIS_BRANCH#drv_}"
        echo "Building $DRV"
        mkdir -p build/$DRV
        pushd build/$DRV
        cmake -DCMAKE_INSTALL_PREFIX=/usr/local/ . ../../3rdparty/$DRV
        make
        popd
    fi
fi
exit 0

