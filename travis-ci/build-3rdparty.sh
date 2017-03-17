#!/bin/bash

set -x -e

echo ${TRAVIS_OS_NAME}

# The build-libs.sh must be run first for this to work

if [ ! -z $BUILD_THIRD_PARTY ]; then
  echo "==> BUILD_THIRD_PARTY activated"
  mkdir -p build/3rdparty
  pushd build/3rdparty
  cmake -DCMAKE_INSTALL_PREFIX=/usr/local/ . ../../3rdparty/
  make
  popd
else
  echo "==> BUILD_THIRD_PARTY not specified"
fi

exit 0

