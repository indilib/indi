#!/bin/bash

set -x -e

echo "==> Building INDI Core"
mkdir -p build/libindi
pushd build/libindi
cmake -DINDI_BUILD_UNITTESTS=ON -DCMAKE_INSTALL_PREFIX=/usr/local/ . ../../libindi/
make
sudo make install
popd

exit 0

