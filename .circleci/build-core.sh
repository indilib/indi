#!/bin/bash

set -x -e

FLAGS="-DCMAKE_INSTALL_PREFIX=/usr/local -DFIX_WARNINGS=ON -DCMAKE_BUILD_TYPE=$1"
FLAGS+=" -DINDI_BUILD_UNITTESTS=ON"

# Build everything on master
echo "==> Building INDI Core"
mkdir -p build/indi-core
pushd build/indi-core
cmake $FLAGS . ../../
make
make install
popd

exit 0
