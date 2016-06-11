#!/bin/bash

set -x

if [ ! -z $BUILD_INSTALL_GTEST ]; then
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
fi

mkdir -p build/libindi
pushd build/libindi
cmake -DCMAKE_INSTALL_PREFIX=/usr/local/ . ../../libindi/
sudo make install
popd

if [ ! -z $BUILD_THIRD_PARTY ]; then
  mkdir -p build/3rdparty
  pushd build/3rdparty
  bash ../../3rdparty/make_libraries
  cmake -DCMAKE_INSTALL_PREFIX=/usr/local/ -DWITH_NSE:OPTION=ON . ../../3rdparty/
  sudo make install
  popd
fi

if [ ! -z $BUILD_DEB_PACKAGES ]; then
  mkdir -p build/deb_libindi
  pushd build/deb_libindi
  cp -r ../../libindi .
  cp -r ../../cmake_modules libindi/
  cp -r ../../debian/libindi debian
  fakeroot debian/rules binary
  cd ..
  ../3rdparty/make_deb_pkgs 
fi

exit 0

