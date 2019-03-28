#!/bin/bash

if [ ! -d $(pwd)/build/libindi ]; then
    mkdir -p $(pwd)/build/libindi
else
    rm -rf $(pwd)/build/libindi
    mkdir -p $(pwd)/build/libindi
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

cd $(pwd)/build/libindi
cmake -DCMAKE_INSTALL_PREFIX=$INSTALL_DIR -DCMAKE_BUILD_TYPE=Debug -DUDEVRULES_INSTALL_DIR=$INSTALL_DIR/udev ../../libindi
make
make install
