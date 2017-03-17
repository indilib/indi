#!/bin/bash

# This is a script building libraries for travis-ci
# It is *not* for general audience

SRC=../../3rdparty/
LIBS="libapogee libfishcamp libfli libqhy libqsi libsbig"

for lib in $LIBS ; do
(
    echo "Building $lib ..."
    mkdir -p build/$lib
    pushd build/$lib
    cmake -DCMAKE_INSTALL_PREFIX=/opt . $SRC/$lib
    make
    sudo make install
    popd
)
done
