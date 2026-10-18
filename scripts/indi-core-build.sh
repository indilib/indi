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
INSTALL_PREFIX=/usr

if [[ "$(uname -s)" == "Darwin" ]]; then
    export PATH="$(brew --prefix qt@5)/bin:$PATH"
    INSTALL_PREFIX=/usr/local
fi

mkdir -p build/indi-core
pushd build/indi-core

# A system update can remove the compiler or macOS SDK recorded in an existing CMake cache, and the
# build then fails with "'cstddef' file not found". Configure from scratch when that happens.
if [[ -f CMakeCache.txt ]]; then
    for CACHED in "$(sed -n 's/^CMAKE_CXX_COMPILER:[A-Z]*=//p' CMakeCache.txt)" \
                  "$(sed -n 's/^CMAKE_OSX_SYSROOT:[A-Z]*=//p' CMakeCache.txt)"; do
        if [[ "$CACHED" == /* && ! -e "$CACHED" ]]; then
            echo "$CACHED no longer exists, configuring from scratch"
            rm -rf CMakeCache.txt CMakeFiles
            break
        fi
    done
fi

cmake \
    -DCMAKE_INSTALL_PREFIX=$INSTALL_PREFIX \
    -DFIX_WARNINGS=ON \
    -DCMAKE_BUILD_TYPE=$1 \
    -DINDI_BUILD_UNITTESTS=ON \
    -DINDI_BUILD_INTEGTESTS=ON \
    -DINDI_BUILD_QT_CLIENT=ON \
    . $SRCS

make -j$(($(nproc)+1))
popd
