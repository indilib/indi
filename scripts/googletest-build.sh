#!/bin/bash

set -e

command -v nproc >/dev/null 2>&1 || function nproc {
    command -v sysctl >/dev/null 2>&1 &&  \
        sysctl -n hw.logicalcpu ||
        echo "3"
}

test -d googletest || git clone https://github.com/google/googletest.git googletest

# Always configure from scratch: a stale CMake cache can point to a compiler or macOS SDK that was
# removed by a system update, and the build then fails with "'cstddef' file not found".
rm -rf build/googletest
mkdir -p build/googletest
pushd build/googletest
cmake \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DBUILD_SHARED_LIBS=ON \
    . ../../googletest

make -j$(($(nproc)+1))
popd
