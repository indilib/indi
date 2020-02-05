#!/bin/bash

if [ ! -d $(pwd)/build/indi-core ]; then
    mkdir -p $(pwd)/build/indi-core
else
    rm -rf $(pwd)/build/indi-core
    mkdir -p $(pwd)/build/indi-core
fi

if [ ! -d $1 ]; then
    mkdir -p $1
    INSTALL_DIR=$(readlink -f $1)
else
    rm -rf $1
    mkdir -p $1
    INSTALL_DIR=$(readlink -f $1)
fi

if [ ! -d $INSTALL_DIR/udev ]; then
    mkdir -p $INSTALL_DIR/udev
fi

cd $(pwd)/build/indi-core
cmake -DCMAKE_INSTALL_PREFIX=$INSTALL_DIR -DCMAKE_BUILD_TYPE=Debug -DUDEVRULES_INSTALL_DIR=$INSTALL_DIR/udev ../../.
make -j4
make install
