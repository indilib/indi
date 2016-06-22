#!/bin/bash

set -x

sudo apt-get -qq update
sudo apt-get -q -y install libusb-1.0-0-dev libcfitsio3-dev libnova-dev
sudo apt-get -q -y install libgphoto2-dev libgps-dev libjpeg-dev cdbs
sudo apt-get -q -y install libopenal-dev libgsl0-dev libboost-dev
sudo apt-get -q -y install libboost-regex-dev libftdi-dev dcraw fakeroot
sudo apt-get -q -y install wget curl

if [ ! -z $BUILD_INSTALL_GTEST ]; then
  echo "==> BUILD_INSTALL_GTEST activated"
  mkdir -p build
  pushd build
  git clone https://github.com/google/googletest.git
  if [ ! -d googletest ]; then
    echo "Failed to get googletest.git repo"
    exit 1
  fi
  pushd googletest
  mkdir build
  cd build
  cmake ..
  sudo make install
  popd
  rm -rf googletest
  popd
else
  echo "==> BUILD_INSTALL_GTEST not specified"
fi

exit 0

