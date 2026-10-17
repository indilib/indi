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

    # Detect architecture
    ARCH=$(uname -m)
    if [[ "$ARCH" == "arm64" ]]; then
        HOMEBREW_PREFIX="/opt/homebrew"
        OSX_ARCH="arm64"
    else
        HOMEBREW_PREFIX="/usr/local"
        OSX_ARCH="x86_64"
    fi

    # Use Homebrew LLVM clang (Apple's default clang may miss C++ headers)
    CLANG_C="$HOMEBREW_PREFIX/opt/llvm/bin/clang"
    CLANG_CXX="$HOMEBREW_PREFIX/opt/llvm/bin/clang++"

    if [[ -x "$CLANG_CXX" ]]; then
        CMAKE_COMPILER_FLAGS=(
            -DCMAKE_C_COMPILER="$CLANG_C"
            -DCMAKE_CXX_COMPILER="$CLANG_CXX"
        )
    fi

    # macOS-specific CMake flags
    CMAKE_MACOS_FLAGS=(
        -DCMAKE_OSX_ARCHITECTURES="$OSX_ARCH"
        -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0
        -DCMAKE_CXX_FLAGS="-Wno-error=deprecated-declarations -Wno-error=unused-but-set-variable -Wno-error=unused-but-set-global -Wno-error=deprecated"
        -DCMAKE_C_FLAGS="-Wno-error=unused-but-set-variable -Wno-error=unused-but-set-global"
        -DINDI_INSTALL_UDEV_RULES=OFF
    )

    # Use Ninja if available (faster)
    if command -v ninja >/dev/null 2>&1; then
        GENERATOR=(-G Ninja)
        BUILD_CMD="cmake --build . -- -j$(($(nproc)+1))"
    else
        GENERATOR=()
        BUILD_CMD="make -j$(($(nproc)+1))"
    fi
fi

mkdir -p build/indi-core
pushd build/indi-core
cmake \
    -DCMAKE_INSTALL_PREFIX=$INSTALL_PREFIX \
    -DFIX_WARNINGS=ON \
    -DCMAKE_BUILD_TYPE=$1 \
    -DINDI_BUILD_UNITTESTS=ON \
    -DINDI_BUILD_INTEGTESTS=ON \
    -DINDI_BUILD_QT_CLIENT=ON \
    "${CMAKE_COMPILER_FLAGS[@]}" \
    "${CMAKE_MACOS_FLAGS[@]}" \
    "${GENERATOR[@]}" \
    . $SRCS

if [[ "$(uname -s)" == "Darwin" ]] && command -v ninja >/dev/null 2>&1; then
    cmake --build . -- -j$(($(nproc)+1))
else
    make -j$(($(nproc)+1))
fi
popd
