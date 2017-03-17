#!/bin/bash

set -x -e

if [ ${TRAVIS_BRANCH} == 'master' ] ; then
    # Build everything on master
    echo "==> Building INDI Core"
    mkdir -p build/libindi
    pushd build/libindi
    cmake -DINDI_BUILD_UNITTESTS=ON -DCMAKE_INSTALL_PREFIX=/usr/local/ . ../../libindi/
    make
    sudo make install
    popd
else 
    # Skip the build just use recent upstream version if it exists
    if [ ${TRAVIS_OS_NAME} == 'linux' ] ; then
        sudo apt-add-repository -y ppa:jochym/indi-devel
        sudo apt-get -qq update
        sudo apt-get -q -y install libindi-dev indi-bin indi-data
    fi
fi

exit 0

