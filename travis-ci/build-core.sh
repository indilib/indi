#!/bin/bash

set -x

echo "==> Building INDI Core"
mkdir -p build/libindi
pushd build/libindi
cmake -DCMAKE_INSTALL_PREFIX=/usr/local/ . ../../libindi/
sudo make install
popd

exit 0

