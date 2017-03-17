#!/bin/bash

# This is a script building libraries for travis-ci
# It is *not* for general audience

SRC=../../3rdparty/

if [ ${TRAVIS_OS_NAME} == "linux" ] ; then
    LIBS="libapogee libfishcamp libfli libqhy libqsi libsbig"
else 
    LIBS="libqsi"
fi

for lib in $LIBS ; do
(
    echo "Building $lib ..."
    mkdir -p build/$lib
    pushd build/$lib
    cmake -DCMAKE_INSTALL_PREFIX=/usr/local . $SRC/$lib
    make
    sudo make install
    popd
)
done
