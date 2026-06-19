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

INDI_SRCS=$(dirname $(realpath $0))/..

rm -rf build/deb_indi-core
mkdir -p build/deb_indi-core
pushd build/deb_indi-core
for x in "${INDI_SRCS}"/*; do
    [ "$(basename "$x")" = "build" ] && continue
    cp -r "$x" ./
done
fakeroot debian/rules -j$(($(nproc)+1)) binary
pwd
popd

echo
echo "Available packages:"
ls -l build/*.deb
