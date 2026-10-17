#!/bin/bash

set -e

command -v nproc >/dev/null 2>&1 || function nproc {
    command -v sysctl >/dev/null 2>&1 &&  \
        sysctl -n hw.logicalcpu ||
        echo "3"
}

test -d googletest || git clone https://github.com/google/googletest.git googletest

# macOS-specific compiler settings (Apple's default clang is missing C++ headers)
if [[ "$(uname -s)" == "Darwin" ]]; then
    ARCH=$(uname -m)
    if [[ "$ARCH" == "arm64" ]]; then
        HOMEBREW_PREFIX="/opt/homebrew"
        OSX_ARCH="arm64"
    else
        HOMEBREW_PREFIX="/usr/local"
        OSX_ARCH="x86_64"
    fi

    # Use Homebrew LLVM clang
    CLANG_C="$HOMEBREW_PREFIX/opt/llvm/bin/clang"
    CLANG_CXX="$HOMEBREW_PREFIX/opt/llvm/bin/clang++"

    if [[ -x "$CLANG_CXX" ]]; then
        CMAKE_COMPILER_FLAGS=(
            -DCMAKE_C_COMPILER="$CLANG_C"
            -DCMAKE_CXX_COMPILER="$CLANG_CXX"
        )
    fi

    CMAKE_MACOS_FLAGS=(
        -DCMAKE_OSX_ARCHITECTURES="$OSX_ARCH"
        -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0
    )
fi

mkdir -p build/googletest
pushd build/googletest
cmake \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DBUILD_SHARED_LIBS=ON \
    "${CMAKE_COMPILER_FLAGS[@]}" \
    "${CMAKE_MACOS_FLAGS[@]}" \
    . ../../googletest

make -j$(($(nproc)+1))
popd
