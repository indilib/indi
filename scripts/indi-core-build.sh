#!/bin/bash

set -e

command -v realpath >/dev/null 2>&1 || realpath() {
    [[ $1 = /* ]] && echo "$1" || echo "$PWD/${1#./}"
}

command -v nproc >/dev/null 2>&1 || function nproc {
    command -v sysctl >/dev/null 2>&1 &&  \
        sysctl -n hw.logicalcpu ||
        echo "3"
}

SRCS=$(dirname $(realpath $0))/..

if [[ "$(uname -s)" == "Darwin" ]]; then
    export PATH="$(brew --prefix qt5)/bin:$PATH"
fi

mkdir -p build/indi-core
pushd build/indi-core
cmake \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DFIX_WARNINGS=ON \
    -DCMAKE_BUILD_TYPE=$1 \
    -DINDI_BUILD_UNITTESTS=ON \
    -DINDI_BUILD_INTEGTESTS=ON \
    -DINDI_BUILD_QT5_CLIENT=ON \
    . $SRCS

make -j$(($(nproc)+1))
popd
