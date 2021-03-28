#!/bin/bash

set -e

test -d googletest || git clone https://github.com/google/googletest.git googletest

mkdir -p build/googletest
pushd build/googletest
cmake \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    . ../../googletest

make -j3
popd
