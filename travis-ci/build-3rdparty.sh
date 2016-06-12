#!/bin/bash

set -x

if [ ! -z $BUILD_THIRD_PARTY ]; then
  echo "==> BUILD_THIRD_PARTY activated"
  mkdir -p build/3rdparty
  pushd build/3rdparty
  bash ../../3rdparty/make_libraries
  cmake -DCMAKE_INSTALL_PREFIX=/usr/local/ -DWITH_NSE:OPTION=ON . ../../3rdparty/
  sudo make install
  popd
else
  echo "==> BUILD_THIRD_PARTY not specified"
fi

exit 0

