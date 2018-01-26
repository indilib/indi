#!/bin/bash

set -x -e

FLAGS="-DCMAKE_INSTALL_PREFIX=/usr/local -DFIX_WARNINGS=ON -DCMAKE_BUILD_TYPE=$1"
FLAGS+=" -DINDI_BUILD_UNITTESTS=ON"

if [ .${CIRCLE_BRANCH%_*} == '.drv' -a `lsb_release -si` == 'Ubuntu' ] ; then
    # Skip the build just use recent upstream version if it exists
    echo 'deb http://ppa.launchpad.net/jochym/indi-devel/ubuntu ' `lsb_release -cs` main > /etc/apt/sources.list.d/indi.list
    apt-key adv --keyserver hkp://keyserver.ubuntu.com:80 --recv-keys DD9784BC4376B5DC
    apt-get -qq update
    apt-get -q -y install libindi-dev
else
    # Build everything on master
    echo "==> Building INDI Core"
    mkdir -p build/libindi
    pushd build/libindi
    cmake $FLAGS . ../../libindi/
    make
    make install
    popd
fi

exit 0
