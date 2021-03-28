#!/bin/bash

set -e

SRCS=$(dirname $(realpath $0))/..

mkdir -p build/indi-core
pushd build/indi-core
cmake \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DFIX_WARNINGS=ON \
    -DCMAKE_BUILD_TYPE=$1 \
    -DINDI_BUILD_UNITTESTS=ON \
    . $SRCS

make -j3
popd
